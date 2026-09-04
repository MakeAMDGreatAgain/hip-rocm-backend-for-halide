// End-to-end (compile-only) check for gpu_lanes on the HIP backend: a
// warp-level reduction plus a register-striped stencil, compiled to an object
// for gfx942 (wave64) and gfx1100 (wave32). The .hsaco is dumped with
// HL_HIP_DUMP_OBJ so it can be inspected with llvm-readelf / llvm-objdump.
#include "Halide.h"
#include <cstdio>
using namespace Halide;

int main(int argc, char **argv) {
    const char *ts = argc > 1 ? argv[1] : "host-hip-hip_gfx942";
    const int lanes = argc > 2 ? atoi(argv[2]) : 32;
    Target t(ts);
    printf("target=%s supported=%d lanes=%d\n", t.to_string().c_str(), (int)t.supported(), lanes);

    ImageParam in(Float(32), 2, "in");

    // 1. A reduction across the lanes of a warp: sum of `lanes` values of
    //    each row is accumulated in registers striped across the lanes and
    //    read back with warp shuffles (same shape as the CUDA warp-sum
    //    schedules in test/correctness/gpu_lanes / register_shuffle).
    Func f("f"), g("g");
    Var x("x"), y("y"), xo("xo"), xi("xi"), yi("yi");
    f(x, y) = in(x, y) * 2.0f;
    g(x, y) = f(x - 1, y) + f(x + 1, y) + f(x, y);
    g.gpu_tile(x, y, xi, yi, lanes, 2, TailStrategy::RoundUp).gpu_lanes(xi);
    f.compute_root();
    f.in(g).compute_at(g, yi).split(x, xo, xi, lanes, TailStrategy::RoundUp).gpu_lanes(xi).unroll(xo);

    // 2. An in-warp sum: c(y) = sum over x of a(x, y), computed with a lane
    //    loop and a serial rfactor-free reduction that reads across lanes.
    Func a("a"), s("s");
    RDom r(0, lanes);
    a(x, y) = in(x, y);
    s(x, y) = 0.0f;
    s(x, y) += a(x + r, y);
    Var yo("yo");
    s.gpu_tile(x, y, xo, yo, xi, yi, lanes, 1, TailStrategy::RoundUp).gpu_lanes(xi);
    s.update().gpu_tile(x, y, xo, yo, xi, yi, lanes, 1, TailStrategy::RoundUp).gpu_lanes(xi);
    a.compute_at(s, yi).split(x, xo, xi, lanes, TailStrategy::RoundUp).gpu_lanes(xi).unroll(xo).store_in(MemoryType::Register);

    Pipeline p({g, s});
    std::string obj = std::string("e2e_lanes_") + t.get_hip_arch_string() + ".o";
    p.compile_to_object(obj, {in}, "e2e_lanes", t);
    printf("compiled %s\n", obj.c_str());
    return 0;
}
