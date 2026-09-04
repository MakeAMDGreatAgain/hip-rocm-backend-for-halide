# compile-verify — 2026-09-04

- build: `/home/benje/halide-hip/build` (Halide `e3c847d`, branch `hip-backend`)
- tools: `/home/benje/halide-hip/llvm/LLVM-22.1.8-Linux-X64/bin` (LLVM (http://llvm.org/):)
- archs: gfx942 gfx1100 gfx1151 gfx1201
- log: `compile-verify-20260904-142528.log`; per-object readelf/objdump dumps under `/home/benje/halide-hip/compile-verify/<arch>/`
- checks per code object: EM_AMDGPU, OS/ABI AMDGPU-HSA, ET_DYN, e_flags MACH byte, ELF ABI 3/4, NT_AMDGPU_METADATA + amdhsa.kernels, zero relocations, s_endpgm; shared pipeline additionally ds_* + s_barrier

| arch | pipeline | result | notes |
|---|---|---|---|
| gfx942 | smoke/simple | PASS | (1 code object(s)) |
| gfx942 | smoke/shared | PASS | (1 code object(s)) |
| gfx942 | smoke/atomic | PASS | (1 code object(s)) |
| gfx942 | smoke/f16 | PASS | (1 code object(s)) |
| gfx942 | smoke/idx64 | PASS | (1 code object(s)) |
| gfx942 | apps/configure | FAIL | cmake configure failed (see gfx942/apps-configure.log; libpng/libjpeg-dev missing?) |
| gfx1100 | smoke/simple | PASS | (1 code object(s)) |
| gfx1100 | smoke/shared | PASS | (1 code object(s)) |
| gfx1100 | smoke/atomic | PASS | (1 code object(s)) |
| gfx1100 | smoke/f16 | PASS | (1 code object(s)) |
| gfx1100 | smoke/idx64 | PASS | (1 code object(s)) |
| gfx1100 | apps/configure | FAIL | cmake configure failed (see gfx1100/apps-configure.log; libpng/libjpeg-dev missing?) |
| gfx1151 | smoke/simple | PASS | (1 code object(s)) |
| gfx1151 | smoke/shared | PASS | (1 code object(s)) |
| gfx1151 | smoke/atomic | PASS | (1 code object(s)) |
| gfx1151 | smoke/f16 | PASS | (1 code object(s)) |
| gfx1151 | smoke/idx64 | PASS | (1 code object(s)) |
| gfx1151 | apps/configure | FAIL | cmake configure failed (see gfx1151/apps-configure.log; libpng/libjpeg-dev missing?) |
| gfx1201 | smoke/simple | PASS | (1 code object(s)) |
| gfx1201 | smoke/shared | PASS | (1 code object(s)) |
| gfx1201 | smoke/atomic | PASS | (1 code object(s)) |
| gfx1201 | smoke/f16 | PASS | (1 code object(s)) |
| gfx1201 | smoke/idx64 | PASS | (1 code object(s)) |
| gfx1201 | apps/configure | FAIL | cmake configure failed (see gfx1201/apps-configure.log; libpng/libjpeg-dev missing?) |

**4 FAIL(s)** across 24 rows.
