// End-to-end (compile-only) check for the device math library on the HIP
// backend: f32, f64 and f16 transcendentals in a gpu_tile'd kernel.
#include "Halide.h"
#include <cstdio>
using namespace Halide;

int main(int argc, char **argv) {
    const char *ts = argc > 1 ? argv[1] : "host-hip-hip_gfx942";
    Target t(ts);
    printf("target=%s supported=%d\n", t.to_string().c_str(), (int)t.supported());

    ImageParam in(Float(32), 2, "in");
    ImageParam ind(Float(64), 2, "ind");
    Param<float> p("p");
    Var x("x"), y("y"), xo("xo"), yo("yo"), xi("xi"), yi("yi");

    Func f32("f32");
    Expr v = in(x, y);
    f32(x, y) = sin(v) + cos(v) + tan(v) + exp(v) + log(v) + pow(v, p) + sqrt(v) +
                asin(v) + acos(v) + atan(v) + atan2(v, p) + sinh(v) + cosh(v) + tanh(v) +
                asinh(v) + acosh(v) + atanh(v) + floor(v) + ceil(v) + round(v) + trunc(v) +
                abs(v) + fast_inverse(v) + fast_inverse_sqrt(v) +
                select(is_nan(v), 1.0f, 0.0f) + select(is_inf(v), 1.0f, 0.0f) + select(is_finite(v), 1.0f, 0.0f);
    f32.gpu_tile(x, y, xo, yo, xi, yi, 16, 16);

    Func f64("f64");
    Expr d = ind(x, y);
    f64(x, y) = sin(d) + cos(d) + tan(d) + exp(d) + log(d) + pow(d, cast<double>(p)) + sqrt(d) +
                asin(d) + acos(d) + atan(d) + atan2(d, cast<double>(p)) + sinh(d) + cosh(d) + tanh(d) +
                asinh(d) + acosh(d) + atanh(d) + floor(d) + ceil(d) + round(d) + trunc(d) + abs(d);
    f64.gpu_tile(x, y, xo, yo, xi, yi, 16, 16);

    Func f16("f16");
    Expr h = cast<float16_t>(in(x, y));
    f16(x, y) = sin(h) + exp(h) + log(h) + pow(h, cast<float16_t>(p)) + sqrt(h) + floor(h) + ceil(h) +
                trunc(h) + abs(h) + fast_inverse(h) + fast_inverse_sqrt(h) + tanh(h);
    f16.gpu_tile(x, y, xo, yo, xi, yi, 16, 16);

    Pipeline pipe({f32, f64, f16});
    std::string obj = std::string("e2e_math_") + t.get_hip_arch_string() + ".o";
    pipe.compile_to_object(obj, {in, ind, p}, "e2e_math", t);
    printf("compiled %s\n", obj.c_str());
    return 0;
}
