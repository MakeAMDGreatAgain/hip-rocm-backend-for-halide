// Mission 001 — HIP backend compile-only smoke pipelines.
//
// Builds five small Halide pipelines with GPU schedules and AOT-compiles them
// (compile_to_static_library) for the target given by HL_TARGET, e.g.
//     HL_TARGET=host-hip-hip_gfx942 HL_HIP_DUMP_OBJ=/tmp/hsaco ./smoke /tmp/out
// No GPU or ROCm is needed: nothing is executed, only compiled. Each pipeline
// produces <outdir>/<name>.a + <name>.h; with HL_HIP_DUMP_OBJ set the backend
// also writes the device object and the linked code object (.hsaco).
//
// Pipelines:
//   simple   gpu_tile, buffer + scalar args
//   shared   producer computed at block level in MemoryType::GPUShared
//            (mirrors test/correctness/gpu_dynamic_shared.cpp) -> ds_* + s_barrier
//   atomic   histogram update with .atomic() on gpu_blocks/gpu_threads
//   f16      float16 input/output arithmetic
//   idx64    64-bit integer arithmetic inside the kernel
//
// Usage: smoke <outdir> [pipeline-name ...]   (default: all five)
// Exit code: 0 if every requested pipeline compiled, 1 otherwise.

#include "Halide.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

using namespace Halide;

namespace {

struct Case {
    std::string name;
    std::function<void(const std::string &prefix, const Target &t)> build;
};

void compile(Func out, std::vector<Argument> args, const std::string &prefix,
             const std::string &fn_name, const Target &t) {
    out.compile_to_static_library(prefix, args, fn_name, t);
}

// 1. plain gpu_tile with a buffer and a scalar parameter
void build_simple(const std::string &prefix, const Target &t) {
    ImageParam in(Int(32), 2, "in");
    Param<int> p("p");
    Var x("x"), y("y"), xo, yo, xi, yi;
    Func out("smoke_simple");
    out(x, y) = in(x, y) * 2 + p;
    out.gpu_tile(x, y, xo, yo, xi, yi, 16, 16);
    compile(out, {in, p}, prefix, "smoke_simple", t);
}

// 2. shared memory: f is computed once per block into LDS, consumed by g.
//    Same structure as gpu_dynamic_shared (per_thread == 0, GPUShared).
void build_shared(const std::string &prefix, const Target &t) {
    ImageParam in(Int(32), 1, "in");
    Var x("x"), xi("xi");
    Func f("f"), g("smoke_shared");
    f(x) = in(x) + x;
    g(x) = f(x) + f(2 * x);
    g.gpu_tile(x, xi, 16);
    f.compute_at(g, x).gpu_threads(x).store_in(MemoryType::GPUShared);
    compile(g, {in}, prefix, "smoke_shared", t);
}

// 3. atomics: 256-bin histogram, update scheduled on the GPU with .atomic()
void build_atomic(const std::string &prefix, const Target &t) {
    ImageParam in(UInt(8), 2, "in");
    Var x("x");
    RVar rxo("rxo"), rxi("rxi");
    Func hist("smoke_atomic");
    hist(x) = 0;
    RDom r(0, 64, 0, 64);
    hist(cast<int>(in(r.x, r.y))) += 1;
    hist.gpu_tile(x, Var("xi"), 64);
    hist.update()
        .atomic()
        .split(r.x, rxo, rxi, 16)
        .gpu_blocks(rxo)
        .gpu_threads(rxi);
    compile(hist, {in}, prefix, "smoke_atomic", t);
}

// 4. float16 arithmetic on the device
void build_f16(const std::string &prefix, const Target &t) {
    ImageParam in(Float(16), 2, "in");
    Var x("x"), y("y"), xo, yo, xi, yi;
    Func out("smoke_f16");
    Expr two = cast(Float(16), 2.0f);
    Expr half = cast(Float(16), 0.5f);
    out(x, y) = in(x, y) * two + half * in(x, y);
    out.gpu_tile(x, y, xo, yo, xi, yi, 16, 8);
    compile(out, {in}, prefix, "smoke_f16", t);
}

// 5. 64-bit integer arithmetic (index math widened to int64 inside the kernel)
void build_idx64(const std::string &prefix, const Target &t) {
    ImageParam in(Int(32), 2, "in");
    Param<int64_t> stride("stride");
    Var x("x"), y("y"), xo, yo, xi, yi;
    Func out("smoke_idx64");
    Expr lin = cast<int64_t>(y) * stride + cast<int64_t>(x);
    Expr h = (lin * Expr((int64_t)6364136223846793005LL) + Expr((int64_t)1442695040888963407LL)) >> 17;
    out(x, y) = in(x, y) + cast<int32_t>(h & Expr((int64_t)0x7fffffff));
    out.gpu_tile(x, y, xo, yo, xi, yi, 32, 4);
    compile(out, {in, stride}, prefix, "smoke_idx64", t);
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <outdir> [pipeline ...]\n", argv[0]);
        return 2;
    }
    const std::string outdir = argv[1];
    const Target t = get_target_from_environment();
    if (!t.has_feature(Target::HIP)) {
        fprintf(stderr, "[smoke] HL_TARGET=%s does not contain 'hip'; refusing to run.\n",
                t.to_string().c_str());
        return 2;
    }
    printf("[smoke] target: %s\n", t.to_string().c_str());

    const std::vector<Case> cases = {
        {"simple", build_simple},
        {"shared", build_shared},
        {"atomic", build_atomic},
        {"f16", build_f16},
        {"idx64", build_idx64},
    };

    std::vector<std::string> wanted;
    for (int i = 2; i < argc; i++) {
        wanted.push_back(argv[i]);
    }

    int failures = 0;
    for (const Case &c : cases) {
        if (!wanted.empty()) {
            bool found = false;
            for (const std::string &w : wanted) {
                found = found || (w == c.name);
            }
            if (!found) {
                continue;
            }
        }
        const std::string prefix = outdir + "/smoke_" + c.name;
        try {
            c.build(prefix, t);
            printf("[smoke] PASS %s -> %s.a\n", c.name.c_str(), prefix.c_str());
        } catch (const Halide::Error &e) {
            fprintf(stderr, "[smoke] FAIL %s: %s\n", c.name.c_str(), e.what());
            failures++;
        } catch (const std::exception &e) {
            fprintf(stderr, "[smoke] FAIL %s: %s\n", c.name.c_str(), e.what());
            failures++;
        }
    }
    printf("[smoke] %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
