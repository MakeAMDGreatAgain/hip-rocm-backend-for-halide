# tools/ — generators and verifiers for the device math library

These are development tools, not part of the Halide patch series. They were used to produce and validate `src/runtime/amdgpu_dev.ll` (patch 0006).

| File | Purpose |
|---|---|
| `gen_amdgpu_dev.py` | Generator for the transcendental section of `amdgpu_dev.ll`. `python3 gen_amdgpu_dev.py emit` reproduces the committed file byte-for-byte; `emit-test` emits an LLVM IR module with kernels calling all 65 helpers (used for the `llc` lowering checks). Each algorithm (sin/cos/tan, exp/log, pow, atan/atan2, hyperbolics, inverse hyperbolics) is written once against a small IR builder DSL, with the accuracy bounds documented in the emitted header. |
| `verify_amdgpu_dev.sh` | Regenerates the `.ll`, runs `llvm-as` + `opt -passes=verify`, links the test kernels and runs `llc` for gfx90a/gfx942/gfx950 and gfx1030/gfx1100/gfx1201 (`+wavefrontsize32`), checking for undefined symbols and surviving calls. |
| `verify_math_accuracy.sh` + `math_harness.cpp` + `strip_for_host.py` | Host-side accuracy harness: strips the AMDGPU-only intrinsics from the IR, compiles the same math code for x86-64 (+fma) and compares against glibc `long double` libm over 2e5 samples per domain, reporting max ulp. This is how the `pow_f64` double-double fix (406 ulp → 0.61 ulp) was measured. |

Paths inside the scripts point at the WSL development layout (`~/halide-hip/...`); adjust `ROOT`/`S` variables when running elsewhere. None of this requires an AMD GPU.
