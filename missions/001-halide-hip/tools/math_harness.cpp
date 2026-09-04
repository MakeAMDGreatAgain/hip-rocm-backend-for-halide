// Accuracy harness for the transcendental section of amdgpu_dev.ll, compiled
// for the host with llc (generic LLVM intrinsics only) and compared with
// glibc's long double libm as the reference.
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>

extern "C" {
float sin_f32(float), cos_f32(float), tan_f32(float), exp_f32(float), log_f32(float);
float pow_f32(float, float), asin_f32(float), acos_f32(float), atan_f32(float), atan2_f32(float, float);
float sinh_f32(float), cosh_f32(float), tanh_f32(float), asinh_f32(float), acosh_f32(float), atanh_f32(float);
float sqrt_f32(float), floor_f32(float), ceil_f32(float), trunc_f32(float), round_f32(float), abs_f32(float);
float nan_f32(), inf_f32(), neg_inf_f32();
double sin_f64(double), cos_f64(double), tan_f64(double), exp_f64(double), log_f64(double);
double pow_f64(double, double), asin_f64(double), acos_f64(double), atan_f64(double), atan2_f64(double, double);
double sinh_f64(double), cosh_f64(double), tanh_f64(double), asinh_f64(double), acosh_f64(double), atanh_f64(double);
double sqrt_f64(double), floor_f64(double), ceil_f64(double), trunc_f64(double), round_f64(double), abs_f64(double);
}

static int failures = 0;

template<typename T>
static double ulp_err(T got, long double ref) {
    if (std::isnan(got) && std::isnan(ref)) return 0;
    if (std::isnan(got) || std::isnan(ref)) return 1e30;
    if (std::isinf(got) || std::isinf(ref)) return (got == ref) ? 0 : 1e30;
    T r = (T)ref;
    long double u = std::fabs(std::nextafter(r, (T)INFINITY) - r);
    if (u == 0) u = std::numeric_limits<T>::denorm_min();
    return (double)(fabsl((long double)got - ref) / u);
}

template<typename T, typename F, typename R>
static void check1(const char *name, F f, R ref, T lo, T hi, bool logspace, double limit, long N = 200000) {
    std::mt19937_64 rng(12345);
    std::uniform_real_distribution<double> d(0.0, 1.0);
    double max_ulp = 0; T worst = 0;
    for (long i = 0; i < N; i++) {
        double u = d(rng);
        T x;
        if (logspace) {
            double lx = std::log((double)lo), hx = std::log((double)hi);
            x = (T)std::exp(lx + u * (hx - lx));
            if (rng() & 1) x = -x;
        } else {
            x = (T)((double)lo + u * ((double)hi - (double)lo));
        }
        double e = ulp_err<T>(f(x), ref((long double)x));
        if (e > max_ulp) { max_ulp = e; worst = x; }
    }
    bool ok = max_ulp <= limit;
    if (!ok) failures++;
    printf("%-10s [%-9g, %-9g]%s max %.3f ulp (worst x=%.17g) %s\n", name, (double)lo, (double)hi,
           logspace ? " log" : "    ", max_ulp, (double)worst, ok ? "OK" : "FAIL");
}

template<typename T, typename F, typename R>
static void check2(const char *name, F f, R ref, T lo1, T hi1, T lo2, T hi2, double limit, long N = 200000) {
    std::mt19937_64 rng(777);
    std::uniform_real_distribution<double> d(0.0, 1.0);
    double max_ulp = 0; T wx = 0, wy = 0;
    for (long i = 0; i < N; i++) {
        T x = (T)((double)lo1 + d(rng) * ((double)hi1 - (double)lo1));
        T y = (T)((double)lo2 + d(rng) * ((double)hi2 - (double)lo2));
        double e = ulp_err<T>(f(x, y), ref((long double)x, (long double)y));
        if (e > max_ulp) { max_ulp = e; wx = x; wy = y; }
    }
    bool ok = max_ulp <= limit;
    if (!ok) failures++;
    printf("%-10s x[%g,%g] y[%g,%g] max %.3f ulp (worst x=%.17g y=%.17g) %s\n", name, (double)lo1, (double)hi1,
           (double)lo2, (double)hi2, max_ulp, (double)wx, (double)wy, ok ? "OK" : "FAIL");
}

#define EXPECT(cond) do { if (!(cond)) { printf("FAIL: %s (line %d)\n", #cond, __LINE__); failures++; } } while (0)

int main() {
    printf("=== f32 (reference: long double libm) ===\n");
    check1<float>("sin_f32", sin_f32, sinl, 1e-6f, 0.78f, true, 2.0);
    check1<float>("sin_f32", sin_f32, sinl, -100.f, 100.f, false, 2.0);
    check1<float>("sin_f32", sin_f32, sinl, 1.f, 16000.f, true, 2.0);
    check1<float>("sin_f32", sin_f32, sinl, 16384.f, 1e9f, true, 2.0);
    check1<float>("cos_f32", cos_f32, cosl, -100.f, 100.f, false, 2.0);
    check1<float>("cos_f32", cos_f32, cosl, 1.f, 1e9f, true, 2.0);
    check1<float>("tan_f32", tan_f32, tanl, -1.5f, 1.5f, false, 4.0);
    check1<float>("tan_f32", tan_f32, tanl, 1.f, 1e6f, true, 4.0);
    check1<float>("exp_f32", exp_f32, expl, -87.f, 88.f, false, 1.0);
    check1<float>("log_f32", log_f32, logl, 1e-30f, 1e30f, true, 1.0);
    check1<float>("log_f32", log_f32, logl, 0.5f, 2.f, false, 1.0);
    check2<float>("pow_f32", pow_f32, powl, 0.01f, 10.f, -10.f, 10.f, 1.0);
    check2<float>("pow_f32", pow_f32, powl, 0.9f, 1.1f, -300.f, 300.f, 1.0);
    check1<float>("asin_f32", asin_f32, asinl, -1.f, 1.f, false, 5.0);
    check1<float>("acos_f32", acos_f32, acosl, -1.f, 1.f, false, 5.0);
    check1<float>("atan_f32", atan_f32, atanl, -1e6f, 1e6f, false, 4.0);
    check1<float>("atan_f32", atan_f32, atanl, 1e-6f, 1e6f, true, 4.0);
    check2<float>("atan2_f32", atan2_f32, atan2l, -10.f, 10.f, -10.f, 10.f, 5.0);
    check1<float>("sinh_f32", sinh_f32, sinhl, -88.f, 88.f, false, 3.0);
    check1<float>("sinh_f32", sinh_f32, sinhl, 1e-6f, 1.f, true, 3.0);
    check1<float>("cosh_f32", cosh_f32, coshl, -88.f, 88.f, false, 3.0);
    check1<float>("tanh_f32", tanh_f32, tanhl, -20.f, 20.f, false, 3.0);
    check1<float>("tanh_f32", tanh_f32, tanhl, 1e-6f, 1.f, true, 3.0);
    check1<float>("asinh_f32", asinh_f32, asinhl, -1e6f, 1e6f, false, 3.0);
    check1<float>("asinh_f32", asinh_f32, asinhl, 1e-6f, 1e6f, true, 3.0);
    check1<float>("acosh_f32", acosh_f32, acoshl, 1.f, 1e6f, false, 3.0);
    check1<float>("acosh_f32", acosh_f32, acoshl, 1.f, 1e30f, true, 3.0);
    check1<float>("atanh_f32", atanh_f32, atanhl, -0.999f, 0.999f, false, 3.0);
    check1<float>("sqrt_f32", sqrt_f32, sqrtl, 1e-30f, 1e30f, true, 0.5);
    printf("=== f64 (reference: long double libm) ===\n");
    check1<double>("sin_f64", sin_f64, sinl, -100., 100., false, 2.0);
    check1<double>("sin_f64", sin_f64, sinl, 1., 1e15, true, 2.0);
    check1<double>("cos_f64", cos_f64, cosl, -100., 100., false, 2.0);
    check1<double>("cos_f64", cos_f64, cosl, 1., 1e15, true, 2.0);
    check1<double>("tan_f64", tan_f64, tanl, -1.5, 1.5, false, 3.0);
    check1<double>("exp_f64", exp_f64, expl, -700., 700., false, 1.0);
    check1<double>("log_f64", log_f64, logl, 1e-300, 1e300, true, 1.5);
    check1<double>("log_f64", log_f64, logl, 0.5, 2., false, 1.5);
    check2<double>("pow_f64", pow_f64, powl, 0.01, 10., -10., 10., 1.2);
    check2<double>("pow_f64", pow_f64, powl, 0.9, 1.1, -3000., 3000., 1.2);
    check2<double>("pow_f64", pow_f64, powl, 1e-3, 1e3, -100., 100., 1.2);
    check2<double>("pow_f64", pow_f64, powl, 0.999, 1.001, -300000., 300000., 1.2);
    check2<double>("pow_f64", pow_f64, powl, -10., 10., -8., 8., 1.2, 20000);  // mostly NaN (non-integer y), int y hit by chance
    check1<double>("asin_f64", asin_f64, asinl, -1., 1., false, 5.0);
    check1<double>("acos_f64", acos_f64, acosl, -1., 1., false, 5.0);
    check1<double>("atan_f64", atan_f64, atanl, -1e6, 1e6, false, 5.0);
    check2<double>("atan2_f64", atan2_f64, atan2l, -10., 10., -10., 10., 5.0);
    check1<double>("sinh_f64", sinh_f64, sinhl, -700., 700., false, 3.0);
    check1<double>("cosh_f64", cosh_f64, coshl, -700., 700., false, 3.0);
    check1<double>("tanh_f64", tanh_f64, tanhl, -20., 20., false, 3.0);
    check1<double>("asinh_f64", asinh_f64, asinhl, -1e6, 1e6, false, 3.0);
    check1<double>("acosh_f64", acosh_f64, acoshl, 1., 1e300, true, 3.0);
    check1<double>("atanh_f64", atanh_f64, atanhl, -0.999, 0.999, false, 3.0);
    check1<double>("sqrt_f64", sqrt_f64, sqrtl, 1e-300, 1e300, true, 0.5);

    printf("=== exact integer powers ===\n");
    for (int b = -12; b <= 12; b++) {
        for (int e = -6; e <= 12; e++) {
            if (b == 0) continue;
            // b^e with e >= 0 is exactly representable here, so it must come out
            // exact; negative exponents are not representable and only need to
            // be within 1 ulp (0.6 ulp max error is not correctly rounded).
            long double ref = powl((long double)b, (long double)e);
            double got = pow_f64((double)b, (double)e);
            float gotf = pow_f32((float)b, (float)e);
            if (e >= 0) {
                if (got != (double)ref) { printf("FAIL: pow_f64(%d, %d) = %.17g want %.17Lg\n", b, e, got, ref); failures++; }
                if (gotf != (float)ref) { printf("FAIL: pow_f32(%d, %d) = %.9g want %.9Lg\n", b, e, gotf, ref); failures++; }
            } else {
                if (ulp_err<double>(got, ref) > 1.0) { printf("FAIL: pow_f64(%d, %d) = %.17g want %.17Lg\n", b, e, got, ref); failures++; }
                if (ulp_err<float>(gotf, ref) > 1.0) { printf("FAIL: pow_f32(%d, %d) = %.9g want %.9Lg\n", b, e, gotf, ref); failures++; }
            }
        }
    }
    printf("=== special values ===\n");
    EXPECT(std::isnan(nan_f32()));
    EXPECT(inf_f32() == INFINITY);
    EXPECT(neg_inf_f32() == -INFINITY);
    EXPECT(exp_f32(1000.f) == INFINITY);
    EXPECT(exp_f32(-1000.f) == 0.f);
    EXPECT(exp_f64(1000.) == INFINITY);
    EXPECT(exp_f64(-1000.) == 0.);
    EXPECT(exp_f32(0.f) == 1.f);
    EXPECT(exp_f64(0.) == 1.);
    EXPECT(log_f32(0.f) == -INFINITY);
    EXPECT(log_f64(0.) == -INFINITY);
    EXPECT(std::isnan(log_f32(-1.f)));
    EXPECT(std::isnan(log_f64(-1.)));
    EXPECT(log_f32(1.f) == 0.f);
    EXPECT(log_f64(1.) == 0.);
    EXPECT(log_f32(INFINITY) == INFINITY);
    EXPECT(std::isnan(sin_f32(INFINITY)));
    EXPECT(std::isnan(sin_f64(NAN)));
    EXPECT(sin_f32(0.f) == 0.f && std::signbit(sin_f32(-0.f)));
    EXPECT(cos_f32(0.f) == 1.f);
    // C99 pow special cases
    EXPECT(pow_f64(2., 10.) == 1024.);
    EXPECT(pow_f64(-2., 3.) == -8.);
    EXPECT(pow_f64(-2., 4.) == 16.);
    EXPECT(pow_f64(-2., -3.) == -0.125);
    EXPECT(std::isnan(pow_f64(-2., 0.5)));
    EXPECT(pow_f64(0., 0.) == 1. && pow_f32(0.f, 0.f) == 1.f);
    EXPECT(pow_f64(NAN, 0.) == 1. && pow_f32(NAN, 0.f) == 1.f);
    EXPECT(pow_f64(1., NAN) == 1. && pow_f32(1.f, NAN) == 1.f);
    EXPECT(pow_f64(1., INFINITY) == 1. && pow_f64(1., -INFINITY) == 1.);
    EXPECT(pow_f64(-1., INFINITY) == 1. && pow_f64(-1., -INFINITY) == 1.);
    EXPECT(std::isnan(pow_f64(NAN, 1.)) && std::isnan(pow_f32(NAN, 1.f)));
    EXPECT(std::isnan(pow_f64(NAN, 2.5)) && std::isnan(pow_f64(2., NAN)));
    EXPECT(pow_f64(0., 2.) == 0. && pow_f64(0., -1.) == INFINITY);
    EXPECT(pow_f64(-0., 3.) == 0. && pow_f64(-0., -1.) == INFINITY);
    EXPECT(pow_f64(INFINITY, 2.) == INFINITY && pow_f64(INFINITY, -2.) == 0.);
    EXPECT(pow_f64(-INFINITY, 3.) == -INFINITY && pow_f64(-INFINITY, 2.) == INFINITY);
    EXPECT(pow_f64(-INFINITY, -3.) == 0. && std::signbit(pow_f64(-INFINITY, -3.)));
    EXPECT(pow_f64(2., INFINITY) == INFINITY && pow_f64(2., -INFINITY) == 0.);
    EXPECT(pow_f64(0.5, INFINITY) == 0. && pow_f64(0.5, -INFINITY) == INFINITY);
    EXPECT(pow_f64(10., 308.) == 1e308);
    EXPECT(pow_f64(10., 309.) == INFINITY);
    EXPECT(pow_f64(10., -300.) == 1e-300);
    EXPECT(pow_f64(2., 1023.) == std::ldexp(1.0, 1023));
    EXPECT(pow_f64(2., 1024.) == INFINITY);
    EXPECT(pow_f64(2., -1074.) == std::ldexp(1.0, -1074));
    EXPECT(pow_f64(2., -1075.) == 0.);
    EXPECT(pow_f64(5e-324, 1.) == 5e-324);  // subnormal x
    EXPECT(pow_f64(5e-324, 0.5) == std::sqrt(5e-324));
    EXPECT(pow_f32(10.f, 38.f) == 1e38f);
    EXPECT(pow_f32(10.f, 39.f) == INFINITY);
    EXPECT(pow_f32(-2.f, 3.f) == -8.f);
    EXPECT(std::isnan(pow_f32(-2.f, 0.5f)));
    EXPECT(round_f32(2.5f) == 2.f && round_f32(3.5f) == 4.f && round_f32(-2.5f) == -2.f);
    EXPECT(round_f64(2.5) == 2. && round_f64(3.5) == 4.);
    EXPECT(floor_f32(-1.5f) == -2.f && ceil_f32(-1.5f) == -1.f && trunc_f32(-1.5f) == -1.f);
    EXPECT(floor_f64(-1.5) == -2. && ceil_f64(-1.5) == -1. && trunc_f64(-1.5) == -1.);
    EXPECT(abs_f32(-3.f) == 3.f && abs_f64(-3.) == 3.);
    EXPECT(sqrt_f32(4.f) == 2.f && sqrt_f64(4.) == 2.);
    EXPECT(std::isnan(sqrt_f32(-1.f)));
    EXPECT(tanh_f32(100.f) == 1.f && tanh_f32(-100.f) == -1.f);
    EXPECT(tanh_f64(100.) == 1. && tanh_f64(-100.) == -1.);
    EXPECT(atan_f32(INFINITY) == (float)(M_PI / 2));
    EXPECT(atan_f64(INFINITY) == M_PI / 2);
    EXPECT(atan2_f32(1.f, 0.f) == (float)(M_PI / 2));
    EXPECT(atan2_f32(0.f, -1.f) == (float)M_PI);
    EXPECT(atan2_f64(0., -1.) == M_PI);
    EXPECT(atan2_f32(-0.f, -1.f) == -(float)M_PI);
    EXPECT(asin_f32(1.f) == (float)(M_PI / 2));
    EXPECT(acos_f32(1.f) == 0.f);
    EXPECT(acos_f32(-1.f) == (float)M_PI);
    EXPECT(std::isnan(asin_f32(1.5f)));
    EXPECT(std::isnan(acos_f64(1.5)));
    EXPECT(acosh_f32(1.f) == 0.f);
    EXPECT(std::isnan(acosh_f32(0.5f)));
    EXPECT(atanh_f32(1.f) == INFINITY);
    EXPECT(atanh_f32(-1.f) == -INFINITY);
    EXPECT(std::isnan(atanh_f64(1.5)));
    EXPECT(sinh_f32(0.f) == 0.f && cosh_f32(0.f) == 1.f);
    EXPECT(sinh_f32(100.f) == INFINITY && sinh_f32(-100.f) == -INFINITY && cosh_f32(100.f) == INFINITY);
    EXPECT(sinh_f64(1000.) == INFINITY && cosh_f64(-1000.) == INFINITY);
    EXPECT(asinh_f32(-3.f) == -asinh_f32(3.f));

    printf("%s (%d failures)\n", failures ? "HARNESS FAILED" : "HARNESS PASSED", failures);
    return failures ? 1 : 0;
}
