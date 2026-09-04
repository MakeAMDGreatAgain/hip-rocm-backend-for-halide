# Mission 001 scripts

All scripts are bash, `set -euo pipefail`, and accept `--help`. They assume the
layout created by `wsl-setup.sh`: `~/halide-hip/Halide` (checkout, branch
`hip-backend`), `~/halide-hip/build` (CMake build tree), `~/halide-hip/llvm/LLVM-22.1.8-Linux-X64`
(official LLVM release tarball with AMDGPU + lld), `~/halide-hip/deps` (static
zstd). Override with `ROOT`, `BUILD`, `HALIDE_SRC`, `LLVM_BIN` where documented.
Results go to `../results/`.

**`wsl-setup.sh`** — one-shot, idempotent environment setup that needs no sudo
and no ROCm: pip-installs cmake + ninja into `~/.local`, downloads the LLVM
release tarball (checks it has the AMDGPU target), builds static zstd (the
release LLVM links `zstd::libzstd_static`), configures Halide with
`-DLLD_DIR`/`-DLLVM_DIR` pointing at that LLVM (`Halide_WASM_BACKEND=OFF`,
tests ON, python/serialization/docs OFF) and builds the `Halide` library
target. Works on WSL2 Ubuntu 24.04 and on plain Ubuntu 22.04/24.04 (used by
`hardware-test.sh` and the CI workflow unchanged). `ROOT` is hard-coded to
`$HOME/halide-hip`; arguments are `[halide_dir] [build_dir]`.

**`compile-verify.sh`** — compile-only verification, no GPU needed. Builds
`smoke/smoke.cpp` (five AOT pipelines: plain gpu_tile, shared-memory producer,
atomic histogram, float16, 64-bit index math) against the build tree and runs
it for each arch in `ARCHS` (default `gfx942 gfx1100 gfx1151 gfx1201`) with
`HL_TARGET=host-hip-hip_<arch>` and `HL_HIP_DUMP_OBJ`; then configures the
`apps/` project against the build tree with `-DHalide_TARGET=host-hip-hip_<arch>`
and builds the manually scheduled library of each app in `APPS` (default
`local_laplacian bilateral_grid blur harris interpolate lens_blur nl_means
stencil_chain camera_pipe`; the ones present are used). Every code object is
checked with `llvm-readelf`/`llvm-objdump` (EM_AMDGPU, AMDGPU-HSA OS/ABI,
ET_DYN, e_flags MACH byte, ABI version 3/4, NT_AMDGPU_METADATA, zero
relocations, `s_endpgm`; `ds_*` + `s_barrier` for the shared-memory pipeline).
If the backend did not dump `.hsaco` files, the embedded ELF is carved out of
the generated archive (python helper). Writes
`results/compile-verify-<date>.md` (arch × pipeline table) and exits non-zero
on any FAIL. Not yet executed — the backend was not buildable when it was
written.

**`hardware-test.sh`** — end-to-end run on an AMD machine with ROCm (AMD
Developer Cloud MI300X = gfx942, or a Radeon). Detects ROCm and the gfx arch,
clones Halide at the SHA pinned in `STATUS.md`, applies `../patches/*.patch`
with `git am`, runs `wsl-setup.sh`, builds the tests, then runs the GPU
correctness subset with `HL_JIT_TARGET=host-hip-hip_<arch>`, an autodetect run
(`host-hip`), an `HL_HIP_ARCH` run, the `-debug` runtime on
`gpu_object_lifetime`, `gpu_lanes` tests, a few AOT generator tests, the apps
benchmark (RunGen `--benchmarks=all`) for HIP vs OpenCL vs Vulkan, and a
`rocprofv3 --kernel-trace` one-off. Every step logs to
`results/hardware-<arch>-<stamp>/`; failures are collected, the script
continues, and `results/hardware-<arch>-<date>.{json,md}` summarise
everything. Exits non-zero if any step failed. Flags: `--skip-build`,
`--skip-apps`, `--only-tests`. Not yet executed (no AMD hardware available to
the authors).

**`smoke/smoke.cpp`** — the pipelines used by `compile-verify.sh`; can be run
by hand: `HL_TARGET=host-hip-hip_gfx942 HL_HIP_DUMP_OBJ=/tmp/h ./smoke /tmp/out [name...]`.

## e2e/ — end-to-end compile-only programs

`e2e_lanes.cpp` (a `gpu_lanes` reduction exercising the warp-shuffle lowering) and `e2e_math.cpp` (f32/f64/f16 transcendentals) are Halide programs that `compile_to_object` for `host-hip-hip_gfx942` and `host-hip-hip_gfx1100` with `HL_HIP_DUMP_OBJ`; `e2e.sh` builds and runs both and inspects the resulting `.hsaco` files (`llvm-readelf --dyn-syms` must show no `UND`, `-r` no relocations, and the lanes kernels must contain `ds_bpermute_b32`). They complement `compile-verify.sh` with shuffle- and math-specific ISA checks. Paths assume the WSL layout (`~/halide-hip`).
