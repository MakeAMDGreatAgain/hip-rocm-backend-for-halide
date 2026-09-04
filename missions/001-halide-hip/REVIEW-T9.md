# T9 independent review — native HIP/AMDGPU backend (branch `hip-backend`, 5 commits on `0537858c`)

Reviewer: independent AI agent (Claude Fable 5.1), read-only, 2026-09-04. Scope: `src/runtime/hip.cpp` family, `src/CodeGen_AMDGPU_Dev.cpp`, cross-cutting plumbing, test, docs, build. Not in scope (still in progress at review time): `LowerWarpShuffles.cpp`, `Lower.cpp`, `amdgpu_dev.ll`. Line numbers refer to the tree at review time; see the fix commit for what changed.

## BLOCKER
None. The kernarg ABI (the highest-risk design point) checks out — see "Verified correct".

## MAJOR
- **M1 In-kernel `AssertStmt` → `llvm.trap` aborts the process on ROCm.** `CodeGen_AMDGPU_Dev.cpp` lowers asserts to `halide_amdgpu_trap` (`s_trap 2`); rocclr delivers wave traps to the HSA queue error callback which logs and `abort()`s, unlike CUDA's recoverable `CUDA_ERROR_LAUNCH_FAILED`. `test/correctness/gpu_assertion_in_kernel.cpp` is CUDA-only so the sweep won't show it. Fix: document as a known behavioural difference; longer term a device-side error flag. Needs hardware confirmation.
- **M2 `test/common/gpu_object_lifetime_tracker.h` has no HIP entries**, so every `gpu_object_lifetime*` test fails on HIP. Add `{"hipMalloc", "hipFree"}` (the HIP debug runtime prints those); no context object to track.
- **M3 `HL_HIP_ARCH` silently overrides an explicit `hip_gfx*` feature** (`Target::get_hip_arch_string()`, `CodeGen_AMDGPU_Dev::hip_arch()`), while Module/JIT/generator caches are keyed on the target string. Fix: feature wins; env consulted only when no `hip_gfx*` is set.

## MINOR
- m1 `halide_hip_device_release` changes the current device and never restores it (CUDA push/pops). Wrap in the RAII `Context`.
- m2 `calculate_host_hip_arch` warns "host HIP device (gfx000) …" on hosts without ROCm because (0,0)+success is treated as unknown; add `if (major == 0) return FeatureEnd;`.
- m3 `supports_atomic_add` returns false for f64 on non-CDNA, which then hits `user_assert(op->mutex_name.empty())`; AMDGPU CAS-expands `atomicrmw fadd double` everywhere, so return true.
- m4 `hip_code_object.cpp` is weaker than STATUS claims: only fails on relocations against undefined symbols; metadata check is a substring search. Add "no SHT_RELA at all" and decode `.args` for the simple variant (`global_buffer@0/8 size 8`, `by_value@16 size 4`).
- m5 `IRPrinter.cpp` / `StmtToHTML.cpp` print `*_gpu_source_kernels` as text — binary for HIP. Gate on `hip_` prefix.
- m6 Generated C headers omit `HalideRuntimeHIP.h` (`CodeGen_C.cpp`); add the `Target::HIP` branch next to CUDA.
- m7 `doc/HIP.md` inaccuracies: `HL_HIP_LLD` wording; Makefile also requires `liblldELF`; status section ahead of T5/T7; denormal justification (clang HIP defaults to IEEE denormals on gfx9+); `HL_DEBUG_CODEGEN=2` dumps post-optimization IR; not in `doc/guides.md`.
- m8 Static-destructor order: `halide_hip_cleanup` calls into libamdhip64 at `exit()`; flag for the hardware run.
- m9 `!amdgpu.no.fine.grained.memory` on all atomics is wrong for user-wrapped fine-grained (managed/host) memory; document at `halide_hip_wrap_device_ptr`.
- m10 Link failures raise `InternalError`; should be `user_error` with captured stderr. `HL_HIP_DUMP_OBJ` to a missing dir asserts; check/create.

## NIT
- `DeviceAPI::HIP` inserted mid-enum renumbers later C++ enumerators (flatbuffer enum was appended correctly); upstream will prefer appending.
- Debug print over-reads small scalars (`*(void**)args[i]`, inherited from cuda.cpp).
- Makefile checks only `liblldELF.*`; `FindHalide_LLVM.cmake` now runs `find_package(LLD)` unconditionally (harmless).
- `.device_code` extension for a binary; `.hsaco` would be friendlier.
- xcframework header list lacks `HalideRuntimeHIP.h`.

## Verified correct (do not re-audit)
- **Kernarg ABI**: LLVM `AMDGPUHSAMetadataStreamer::emitKernelArg` records `.size = DL.getTypeAllocSize(Ty)`, `.offset = alignTo(Offset, ABITypeAlign)`; rocclr `platform/kernel.cpp::captureHIPArgs` copies `desc.size_` bytes from `kernelParams[idx]` to `mem + desc.offset_` (4/8-byte fast paths, memcpy otherwise), hidden args appended by the runtime. Halide's `arg_sizes` are never consumed on the default path; buffer args translate to `&dev_handles[i]` (8 bytes, `global_buffer`); host `make_struct` writes exactly `type.bytes()` per scalar. Nothing can shift later arguments. The `HL_HIP_PACK_KERNARGS` diagnostic path uses the same natural alignment.
- `mini_hip.h` attribute numbering matches ROCm `hip_runtime_api.h` (develop) when enumerated mechanically; `HIP_MEMCPY3D` and `HIP_LAUNCH_PARAM_*` match `driver_types.h`; `hip_functions.h` prototypes match.
- `HalideRuntimeHIP.h` matches definitions; `runtime_api.cpp` exports the same 12 symbols CUDA does.
- `get_hip_arch` encoding round-trips with `calculate_host_hip_arch` for all seven features; 64 KiB opaque buffer is safe for both `hipDeviceProp_t` layouts.
- `Context` RAII, cache key `(void*)(device+1)`, all device-interface entry points one-for-one with `cuda.cpp` (pool reuse, OOM retry, copies incl. 3-D path with correct rejects, crop/slice, release ordering).
- Codegen vs PTX: CC, `noalias`, entry-block casts, `BlockSize`→flat-work-group-size, lazily created dynamic-LDS symbol reset per module, fenced barrier scopes, atomics annotated pre-verify; AS3 symbol only flows through `codegen_buffer_pointer` (GEP preserves AS); shared allocations are merged by `ExtractSharedAndHeapAllocations` for HIP exactly as for CUDA and `sharedMemBytes` sums extents.
- Module setup: DL installed from the TargetMachine before any IR; `halide_mcpu_*` module flags make `target-cpu` correct; COV flag set before passes; O3 pipeline with `registerPassBuilderCallbacks`.
- lld link: temp-file lifetimes, global mutex, poison→external fallback, SIGABRT handler dance, link line identical to clang's HIP device link minus LTO options.
- Target/JIT plumbing, serializer, printer, Python enums, `CodeGen_Internal` user_context list, `Module.cpp` binary device_code, CMake/Makefile/vcpkg consistency with NVPTX/WebAssembly precedents; missing `custom_hip_*` JIT hooks are safe (no per-process context object).
