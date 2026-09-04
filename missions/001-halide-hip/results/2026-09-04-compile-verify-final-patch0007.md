# compile-verify — 2026-09-04

- build: `/home/benje/halide-hip/build` (Halide `91b9daa`, branch `hip-backend`)
- tools: `/home/benje/halide-hip/llvm/LLVM-22.1.8-Linux-X64/bin` (LLVM (http://llvm.org/):)
- archs: gfx942 gfx1100 gfx1151 gfx1201
- log: `compile-verify-20260904-152423.log`; per-object readelf/objdump dumps under `/home/benje/halide-hip/compile-verify/<arch>/`
- checks per code object: EM_AMDGPU, OS/ABI AMDGPU-HSA, ET_DYN, e_flags MACH byte, ELF ABI 3/4, NT_AMDGPU_METADATA + amdhsa.kernels, zero relocations, s_endpgm; shared pipeline additionally ds_* + s_barrier

| arch | pipeline | result | notes |
|---|---|---|---|
| gfx942 | smoke/simple | PASS | (1 code object(s)) |
| gfx942 | smoke/shared | PASS | (1 code object(s)) |
| gfx942 | smoke/atomic | PASS | (1 code object(s)) |
| gfx942 | smoke/f16 | PASS | (1 code object(s)) |
| gfx942 | smoke/idx64 | PASS | (1 code object(s)) |
| gfx942 | apps/local_laplacian | PASS | (1 code object(s), lib local_laplacian) |
| gfx942 | apps/bilateral_grid | PASS | (1 code object(s), lib bilateral_grid) |
| gfx942 | apps/blur | PASS | (1 code object(s), lib halide_blur) |
| gfx942 | apps/harris | PASS | (1 code object(s), lib harris) |
| gfx942 | apps/interpolate | PASS | (1 code object(s), lib interpolate) |
| gfx942 | apps/lens_blur | PASS | (1 code object(s), lib lens_blur) |
| gfx942 | apps/nl_means | PASS | (1 code object(s), lib nl_means) |
| gfx942 | apps/stencil_chain | PASS | (1 code object(s), lib stencil_chain) |
| gfx942 | apps/camera_pipe | PASS | (1 code object(s), lib camera_pipe) |
| gfx1100 | smoke/simple | PASS | (1 code object(s)) |
| gfx1100 | smoke/shared | PASS | (1 code object(s)) |
| gfx1100 | smoke/atomic | PASS | (1 code object(s)) |
| gfx1100 | smoke/f16 | PASS | (1 code object(s)) |
| gfx1100 | smoke/idx64 | PASS | (1 code object(s)) |
| gfx1100 | apps/local_laplacian | PASS | (1 code object(s), lib local_laplacian) |
| gfx1100 | apps/bilateral_grid | PASS | (1 code object(s), lib bilateral_grid) |
| gfx1100 | apps/blur | PASS | (1 code object(s), lib halide_blur) |
| gfx1100 | apps/harris | PASS | (1 code object(s), lib harris) |
| gfx1100 | apps/interpolate | PASS | (1 code object(s), lib interpolate) |
| gfx1100 | apps/lens_blur | PASS | (1 code object(s), lib lens_blur) |
| gfx1100 | apps/nl_means | PASS | (1 code object(s), lib nl_means) |
| gfx1100 | apps/stencil_chain | PASS | (1 code object(s), lib stencil_chain) |
| gfx1100 | apps/camera_pipe | PASS | (1 code object(s), lib camera_pipe) |
| gfx1151 | smoke/simple | PASS | (1 code object(s)) |
| gfx1151 | smoke/shared | PASS | (1 code object(s)) |
| gfx1151 | smoke/atomic | PASS | (1 code object(s)) |
| gfx1151 | smoke/f16 | PASS | (1 code object(s)) |
| gfx1151 | smoke/idx64 | PASS | (1 code object(s)) |
| gfx1151 | apps/local_laplacian | PASS | (1 code object(s), lib local_laplacian) |
| gfx1151 | apps/bilateral_grid | PASS | (1 code object(s), lib bilateral_grid) |
| gfx1151 | apps/blur | PASS | (1 code object(s), lib halide_blur) |
| gfx1151 | apps/harris | PASS | (1 code object(s), lib harris) |
| gfx1151 | apps/interpolate | PASS | (1 code object(s), lib interpolate) |
| gfx1151 | apps/lens_blur | PASS | (1 code object(s), lib lens_blur) |
| gfx1151 | apps/nl_means | PASS | (1 code object(s), lib nl_means) |
| gfx1151 | apps/stencil_chain | PASS | (1 code object(s), lib stencil_chain) |
| gfx1151 | apps/camera_pipe | PASS | (1 code object(s), lib camera_pipe) |
| gfx1201 | smoke/simple | PASS | (1 code object(s)) |
| gfx1201 | smoke/shared | PASS | (1 code object(s)) |
| gfx1201 | smoke/atomic | PASS | (1 code object(s)) |
| gfx1201 | smoke/f16 | PASS | (1 code object(s)) |
| gfx1201 | smoke/idx64 | PASS | (1 code object(s)) |
| gfx1201 | apps/local_laplacian | PASS | (1 code object(s), lib local_laplacian) |
| gfx1201 | apps/bilateral_grid | PASS | (1 code object(s), lib bilateral_grid) |
| gfx1201 | apps/blur | PASS | (1 code object(s), lib halide_blur) |
| gfx1201 | apps/harris | PASS | (1 code object(s), lib harris) |
| gfx1201 | apps/interpolate | PASS | (1 code object(s), lib interpolate) |
| gfx1201 | apps/lens_blur | PASS | (1 code object(s), lib lens_blur) |
| gfx1201 | apps/nl_means | PASS | (1 code object(s), lib nl_means) |
| gfx1201 | apps/stencil_chain | PASS | (1 code object(s), lib stencil_chain) |
| gfx1201 | apps/camera_pipe | PASS | (1 code object(s), lib camera_pipe) |

**0 FAIL(s)** across 56 rows.
