# Halide native HIP/AMDGPU backend — design document

Status: v1 (2026-09-04), derived from a read-only survey of `halide/Halide` `main` @ `0537858c` (2026-09-04). Authored by an AI agent team (Claude Fable 5.1) for review by AMD / Halide maintainers. Every claim about existing Halide code below names the file and symbol it came from.

## 0. Survey of the current Halide `main` code (what the new backend plugs into)

### Device codegen: `src/CodeGen_PTX_Dev.cpp` / `.h`
- Header exposes only `std::unique_ptr<CodeGen_GPU_Dev> new_CodeGen_PTX_Dev(const Target &)`; the class lives in an anonymous namespace: `class CodeGen_PTX_Dev : public CodeGen_LLVM, public CodeGen_GPU_Dev`. Constructor: `CodeGen_LLVM(host)` then `context = new llvm::LLVMContext();`.
- Overrides: `add_kernel`, `compile_to_src`, `get_current_kernel_name`, `dump`, `print_gpu_name`, `api_unique_name` (returns `"cuda"`), `visit(Call/For/Allocate/Free/AssertStmt/Load/Store/Atomic/ProducerConsumer)`, `codegen_vector_reduce`, `mcpu_target`, `mcpu_tune`, `mattrs`, `use_soft_float_abi` (false), `native_vector_bits` (64), `promote_indices` (false), `upgrade_type_for_arithmetic/storage` (keep f16), `supports_atomic_add`.
- `init_module()`: `init_context(); module = get_initial_module_for_ptx_device(target, context);` then `declare_intrin_overload` for `dp4a`/`dp2a`.
- `add_kernel(Stmt, name, vector<DeviceArgument>)`: buffers → `ptr_t`, scalars → `llvm_type_of(type)`; `FunctionType::get(void_t, arg_types, false)`; `ExternalLinkage`; `set_function_attributes_from_halide_target_options(*function)`; `NoAlias` on buffer params; `setCallingConv(PTX_Kernel)`; `entry`/`body` blocks; `sym_push(args[i].name, &fn_arg)`; after body `CreateRetVoid`, `nvvm.annotations` `{fn,"kernel",1}`; `BlockSize` IRVisitor (max extent of loops ending in `gpu_thread_name(i)`) → `"nvvm.maxntid"`; `verifyFunction/verifyModule`.
- Thread/block ids: `visit(const For*)`: `if (is_gpu(loop->for_type))` → `Call::make(Int(32), simt_intrinsic(loop->name), {}, Call::Extern)`; `simt_intrinsic` maps `gpu_thread_name(0..2)` → `llvm.nvvm.read.ptx.sreg.tid.{x,y,z}` and `gpu_block_name` → `...ctaid.{x,y,z}`.
- Shared memory: `visit(const Allocate*)`: `if (is_gpu_shared(alloc->memory_type)) { Value *shared_base = Constant::getNullValue(PointerType::get(*context, 3)); sym_push(alloc->name, shared_base); }` else `CreateAlloca` in `entry_block` with `constant_allocation_size()`. `visit(Free)` = `sym_pop`.
- Barrier: `visit(Call)`: `Call::gpu_thread_barrier` → `await_all_copies()` then `llvm.nvvm.barrier.cta.sync.aligned.all` or `llvm.nvvm.barrier0`. Otherwise `call_overloaded_intrin` then `CodeGen_LLVM::visit`.
- `visit(AssertStmt)`: `IfThenElse(!cond, halide_ptx_trap())`.
- `compile_to_src()`: `TargetRegistry::lookupTarget(triple)`, `TargetOptions options; options.AllowFPOpFusion = any_strict_float ? Strict : Fast`, `createTargetMachine(triple, mcpu_target(), mattrs(), options, Reloc::PIC_, CodeModel::Small, Aggressive)`, module flag `nvvm-reflect-ftz`, `nvptx-f32ftz` fn attr, new-PM `buildPerModuleDefaultPipeline(O3)`, then legacy `addPassesToEmitFile(..., CodeGenFileType::AssemblyFile)`; returns `vector<char>` of PTX text **plus a trailing NUL**.
- `mcpu_target()`: chain of `has_feature(CUDACapabilityNN)` → `sm_NN`; `mattrs()` → `+ptxNN`.
- Warp shuffles are in `src/LowerWarpShuffles.cpp`: `lower_warp_shuffles(Stmt, const Target&)` rewrites lane loops only when `op->device_api == DeviceAPI::CUDA && has_lane_loop(op)`, asserts `"CUDA gpu lanes loop must have constant extent of at most 32"`, and emits `Call::Extern` calls named `llvm.nvvm.shfl.sync.{down,up,idx}.{i32,f32}` with a `0xffffffff` member mask. `src/FuseGPUThreadLoops.cpp` contains **no warp-size constant**.

### GPU-dev interface and host-side offload
- `src/CodeGen_GPU_Dev.h`: pure virtuals `add_kernel`, `init_module`, `compile_to_src`, `get_current_kernel_name`, `api_unique_name`; statics `is_block_uniform`, `is_buffer_constant`, `scalarize_predicated_loads_stores`; `MemoryFenceType {None, Device, Shared}`; `kernel_run_takes_types()`.
- **`src/CodeGen_GPU_Host.cpp` no longer exists.** Its role is the IR pass `src/OffloadGPULoops.cpp` (`inject_gpu_offload(const Stmt&, const Target&, bool any_strict_float)`, invoked from `src/Lower.cpp`). `InjectGpuOffload` fills `cgdev[DeviceAPI::X] = new_CodeGen_X_Dev(device_target)` for CUDA/OpenCL/Metal/D3D12Compute/Vulkan/WebGPU. Per kernel it builds the closure args and emits `call_extern_and_assert("halide_" + api_unique_name + "_run", {state_var, kernel_name, blocks x3, threads x3, shared_mem_size, sizes-struct, args-struct, is_buffer-struct})`. At module init: `kernel_src = compile_to_src()`, embedded as a `Buffer<uint8_t>` named `api + "_gpu_source_kernels"`, then `call_extern_and_assert("halide_" + api + "_initialize_kernels", {state_ptr, src, size})` and `Call::register_destructor("halide_" + api + "_finalize_kernels", state)`.
- `src/DeviceAPI.h`: `enum class DeviceAPI { None, Host, Default_GPU, CUDA, OpenCL, Metal, Hexagon, HexagonDma, D3D12Compute, Vulkan, WebGPU, SMEStreaming }` and `all_device_apis[]`. `src/IRPrinter.cpp` prints `"<CUDA>"` etc.

### Target
- `src/Target.h`: `std::bitset<FeatureEnd> features;` `Feature` mirrors `halide_target_feature_t` in `src/runtime/HalideRuntime.h` (keep in sync with `get_runtime_compatible_target` and PyEnums.cpp). Runtime side is `halide_can_use_target_features(int count, const uint64_t *features)` — a `uint64_t[]` bitmask, so no hard cap; ~180 features exist. New features are appended before `halide_target_feature_end`.
- `src/Target.cpp`: `feature_name_map`; `validate_features()`; `has_gpu_feature()` = CUDA||OpenCL||Metal||D3D12Compute||Vulkan||WebGPU; `get_required_device_api()` priority chain (CUDA first); `target_feature_for_device_api(DeviceAPI)`; `supports_type(Type, DeviceAPI)`; `get_cuda_capability_lower_bound()`; `calculate_host_cuda_capability(Target)` uses `get_device_interface_for_device_api(DeviceAPI::CUDA, t)->compute_capability(...)`; `merge_string` auto-adds host CUDA capability; `get_runtime_compatible_target`.

### Runtime
- `src/runtime/cuda.cpp` (namespace `Halide::Runtime::Internal::Cuda`): `lib_names[]`; `halide_cuda_get_symbol`; `load_libcuda` via `CUDA_FN / CUDA_FN_OPTIONAL` macros and `cuda_functions.h`; `WEAK GPUCompilationCache<CUcontext, CUmodule> compilation_cache;` (`gpu_context_common.h`); `compile_kernel` → `cuModuleLoadData`; `halide_cuda_initialize_kernels`; `halide_cuda_finalize_kernels`; acquire/release/get_stream hooks; `create_cuda_context`; RAII `Context`; `halide_cuda_run(user_context, state_ptr, entry_name, blocksX/Y/Z, threadsX/Y/Z, shared_mem_bytes, size_t arg_sizes[], void *args[], int8_t arg_is_buffer[])` → `cuModuleGetFunction`, buffer args translated to `&dev_handles[i]`, `cuLaunchKernel(..., translated_args, nullptr)`; `halide_cuda_device_malloc` with free-list reuse; `cuda_do_multidimensional_copy`; copy_to_device/host, buffer_copy, device_crop, device_slice, device_release_crop, device_sync, device_release, wrap/detach/get device ptr, `halide_cuda_compute_capability`; `cuda_device_interface_impl` / `cuda_device_interface`; constructor/destructor registration; `error_cuda` + `get_cuda_error_name`.
- `src/runtime/windows_cuda.cpp` = `#define WINDOWS` + `#include "cuda.cpp"`. `src/runtime/mini_cuda.h`: typedefs/enums only. `src/runtime/HalideRuntimeCuda.h`: public API.
- `src/runtime/CMakeLists.txt`: `RUNTIME_CPP` (includes `cuda`, `windows_cuda`, …), `RUNTIME_LL` (`ptx_dev`, …), `RUNTIME_HEADER_FILES`; `foreach (SUFFIX IN ITEMS "" "_debug")` adds `-g -DDEBUG_RUNTIME`; each module compiled with clang to bitcode → `binary2cpp halide_internal_initmod_*`; special rule for `nvidia_libdevice_bitcode/libdevice.10.bc`.
- `src/LLVM_Runtime_Linker.cpp`: `DECLARE_CPP_INITMOD(cuda)`, `DECLARE_LL_INITMOD(ptx_dev)`, `#ifdef WITH_NVPTX DECLARE_LL_INITMOD(ptx_libdevice)`; `get_initial_module_for_ptx_device(Target, LLVMContext*)`; `get_initial_module_for_target` pushes the cuda initmod when `t.has_feature(Target::CUDA)`.

### Build/LLVM plumbing
- Root `CMakeLists.txt`: `find_package(Halide_LLVM 21...99 REQUIRED COMPONENTS WebAssembly X86 OPTIONAL_COMPONENTS AArch64 ARM Hexagon NVPTX PowerPC RISCV)`; PR #8382 removed `AMDGPU`. `cmake_minimum_required(VERSION 3.28)`.
- `cmake/FindHalide_LLVM.cmake`: components → `Halide_LLVM::<comp>` via `llvm_map_components_to_libnames`; `find_package(LLD ${Halide_LLVM_VERSION} EXACT HINTS ...)`; only `Halide_LLVM::WebAssembly` gets `INTERFACE lldWasm lldCommon`.
- `src/CMakeLists.txt`: `foreach (backend IN LISTS Halide_LLVM_COMPONENTS) … target_compile_definitions(Halide PRIVATE WITH_${backend}) target_link_libraries(Halide PRIVATE Halide_LLVM::${backend})` — `WITH_AMDGPU` is auto-defined once the component is requested.
- `src/LLVM_Headers.h`: `#if WITH_WABT || WITH_V8 #include <lld/Common/Driver.h> …`; enforces `LLVM_VERSION >= 210`.
- `src/CodeGen_LLVM.cpp` `initialize_llvm()`: per-`WITH_*` `InitializeXTarget/AsmParser/AsmPrinter`. PR #8382 removed the `WITH_AMDGPU` block.
- `src/WasmExecutor.cpp`: in-process lld: `LLD_HAS_DRIVER(wasm)`, `lld::lldMain(args, outs, errs, {{lld::Wasm, &lld::wasm::link}})`.
- Prior art: PR #2734 (2018) added `src/CodeGen_AMDGPU_Dev.{cpp,h}`, `src/runtime/amdgpu.cpp` (loaded `libhip_hcc.so`), features `AMDGPUGFX900/GFX803`, and — the fatal part — a hand-written `AMDGPULinker` that patched `R_AMDGPU_ABS64`/`R_AMDGPU_RELATIVE64` itself instead of using lld. That is the "GOT relocation" dead end this design avoids.
- Verified LLVM/HIP facts: `DefaultAMDHSACodeObjectVersion` = COV6; module flag `amdhsa_code_object_version` (value/100); ELF ABI version 3 (COV5) / 4 (COV6); `EM_AMDGPU=224`, `ELFOSABI_AMDGPU_HSA=64`, `NT_AMDGPU_METADATA=32`; `EF_AMDGPU_MACH`: GFX942=0x4c, GFX950=0x4f, GFX90A=0x3f, GFX1030=0x36, GFX1100=0x41, GFX1151=0x4a, GFX1201=0x4e. `getWavefrontSize` defaults to 64 unless `FeatureWavefrontSize32`. clang's HIP device link = `ld.lld -flavor gnu -m elf64_amdgpu --no-undefined -shared -plugin-opt=-amdgpu-internalize-symbols … -o out`. `hipModuleLaunchKernel(f, gx,gy,gz, bx,by,bz, sharedMemBytes, stream, void **kernelParams, void **extra)` packs `kernelParams` using the code object's `.args` metadata. `hipModuleLoadData` accepts a code object or a clang offload bundle. Error codes: `hipSuccess=0, hipErrorOutOfMemory=2, hipErrorInvalidImage=200, hipErrorNoBinaryForGpu=209, hipErrorNotFound=500, hipErrorIllegalAddress=700, hipErrorLaunchFailure=719, hipErrorNotSupported=801, hipErrorUnknown=999`. Kernel descriptor: `group_segment_fixed_size@0, private_segment_fixed_size@4, kernarg_size@8, kernel_code_entry_byte_offset@16`.

## 1. Files to add / modify

### New files
| Path | Responsibility |
|---|---|
| `src/CodeGen_AMDGPU_Dev.h` | `std::unique_ptr<CodeGen_GPU_Dev> new_CodeGen_AMDGPU_Dev(const Target &);` |
| `src/CodeGen_AMDGPU_Dev.cpp` | `class CodeGen_AMDGPU_Dev : public CodeGen_LLVM, public CodeGen_GPU_Dev` — kernels, intrinsics, LDS, object emission, in-process lld link. `api_unique_name()` returns `"hip"`. |
| `src/runtime/hip.cpp` | Runtime, namespace `Halide::Runtime::Internal::Hip`, section-by-section mirror of `cuda.cpp`. |
| `src/runtime/windows_hip.cpp` | `#define WINDOWS` / `#include "hip.cpp"`. |
| `src/runtime/mini_hip.h` | Minimal typedefs/enums (`hipError_t`, `hipDeviceptr_t`, `hipModule_t`, `hipFunction_t`, `hipStream_t`, `hipDevice_t`, attribute subset, `HIP_MEMCPY3D`), guard `HALIDE_MINI_HIP_H`. |
| `src/runtime/hip_functions.h` | `HIP_FN(...)` / `HIP_FN_OPTIONAL(...)` X-macro list. |
| `src/runtime/HalideRuntimeHIP.h` | Public C API. |
| `src/runtime/amdgpu_dev.ll` | Device-side helper IR: barrier, trap, shuffles, `fast_inverse*`, math wrappers to `llvm.*` intrinsics. No triple/datalayout (like `ptx_dev.ll`). |
| `src/runtime/amd_device_libs_bitcode/ocml.bc` (phase 2) | Vendored ROCm device library (Apache-2.0 w/ LLVM exception), same pattern as `nvidia_libdevice_bitcode/libdevice.10.bc`. |
| `doc/HIP.md` | Mirrors `doc/Vulkan.md`. |
| internal test `CodeGen_AMDGPU_Dev::test()` | ELF/msgpack validation without hardware (§6). |

### Modified files
| Path | Change |
|---|---|
| `src/runtime/HalideRuntime.h` | Append `halide_target_feature_hip`, `_hip_gfx90a`, `_gfx942`, `_gfx950`, `_gfx1030`, `_gfx1100`, `_gfx1151`, `_gfx1201` immediately before `halide_target_feature_end`. |
| `src/Target.h` | `HIP = halide_target_feature_hip, HIPGFX90A = …` |
| `src/Target.cpp` | `feature_name_map` (`"hip"`, `"hip_gfx942"`, …); `has_gpu_feature()` add HIP; `get_required_device_api()` add HIP after CUDA; `target_feature_for_device_api`; `validate_features()` (`hip_gfx*` require `hip`; ≤1 arch); `supports_type(Type, DeviceAPI::HIP)` → all; `get_runtime_compatible_target` (differing hip arch ⇒ incompatible); `merge_string` host autodetect; `get_host_hip_arch` helper. |
| `src/DeviceAPI.h` | Add `HIP` after `CUDA` in enum and `all_device_apis[]`. |
| `src/IRPrinter.cpp` | `case DeviceAPI::HIP: out << "<HIP>";` |
| `src/OffloadGPULoops.cpp` | `if (target.has_feature(Target::HIP)) cgdev[DeviceAPI::HIP] = new_CodeGen_AMDGPU_Dev(device_target);` |
| `src/InjectHostDevBufferCopies.cpp` | `case DeviceAPI::HIP: "halide_hip_device_interface"`. |
| `src/JITModule.cpp` | `RuntimeKind::HIP`, `HIPDebug`; module `"hip"`; device-interface lookup `halide_hip_device_interface`. |
| `src/runtime/runtime_api.cpp` | Export all `halide_hip_*` symbols. |
| `src/LowerWarpShuffles.cpp` | Accept `DeviceAPI::HIP`; warp size parameter (32/64); backend-neutral shuffle calls. |
| `src/LLVM_Runtime_Linker.cpp` | `DECLARE_CPP_INITMOD(hip)`, `(windows_hip)`, `DECLARE_LL_INITMOD(amdgpu_dev)`; `get_initial_module_for_amdgpu_device`; push hip initmod when `t.has_feature(Target::HIP)`. |
| `src/CodeGen_LLVM.cpp` | Re-add (correctly) `WITH_AMDGPU` Initialize macros (Target, AsmParser, AsmPrinter). |
| `src/LLVM_Headers.h` | `#if WITH_WABT || WITH_V8 || WITH_AMDGPU` for lld includes. |
| `src/CMakeLists.txt`, `src/runtime/CMakeLists.txt`, root `CMakeLists.txt`, `cmake/FindHalide_LLVM.cmake` | Sources; runtime modules `hip windows_hip`, `amdgpu_dev`, header; `AMDGPU` in `OPTIONAL_COMPONENTS`; AMDGPU ⇒ link `lldELF lldCommon` (disable AMDGPU if LLD missing). |
| `vcpkg.json`, `Makefile` | `target-amdgpu` feature; `WITH_AMDGPU` lines mirroring NVPTX. |
| `python_bindings/.../PyEnums.cpp` | Feature and DeviceAPI values. |
| `src/halide_ir.fbs`, `src/Serializer.cpp`, `src/Deserializer.cpp` | `HIP` in the flatbuffer `DeviceAPI` enum and both conversion switches. |
| cmake helpers / test & app CMakeLists that enumerate GPU features | add `hip`. |

## 2. Target-feature design

`hip` + curated per-arch features (names → LLVM `-mcpu`):
```
hip
hip_gfx90a   -> gfx90a   (MI200; wave64)
hip_gfx942   -> gfx942   (MI300A/X; wave64)
hip_gfx950   -> gfx950   (MI350; wave64)
hip_gfx1030  -> gfx1030  (RDNA2; wave32)
hip_gfx1100  -> gfx1100  (RDNA3; wave32)
hip_gfx1151  -> gfx1151  (Strix Halo; wave32)
hip_gfx1201  -> gfx1201  (RDNA4; wave32)
```
- Feature storage is a bitset / `uint64_t[]` — 8 bits are free; CUDA already spends 14 on capabilities. Target strings stay self-describing (`host-hip-hip_gfx942`) for AOT generators and CI matrices.
- Long tail via env `HL_HIP_ARCH=<gfxNNN[:xnack-][:sramecc+]>` read in `mcpu_target()/mattrs()`; overrides any `hip_gfx*`.
- `hip` alone: JIT autodetects from the device (§4 capability encoding); AOT errors `"hip target requires a hip_gfx* feature or HL_HIP_ARCH"`.
- Phase 2: multiple `hip_gfx*` ⇒ one code object per arch wrapped in a clang offload bundle (`__CLANG_OFFLOAD_BUNDLE__`, ids `hip-amdgcn-amd-amdhsa--gfx942`), accepted by `hipModuleLoadData`. Until then `validate_features` asserts ≤1.

## 3. Codegen: PTX → AMDGPU mapping

| Concern | PTX backend | AMDGPU backend |
|---|---|---|
| Triple / DL | `nvptx64--`, hard-coded DL | `amdgcn-amd-amdhsa`; DL from `TargetMachine::createDataLayout()` in `init_module()` (never hard-coded). |
| Kernel CC | `PTX_Kernel` + `nvvm.annotations` | `CallingConv::AMDGPU_KERNEL`; no metadata. |
| Kernel attrs | `nvvm.maxntid` | `"amdgpu-flat-work-group-size"="1,<x*y*z>"` when known (else `"1,1024"`), `"uniform-work-group-size"="true"`. |
| Kernel args | buffers `ptr` (AS0), scalars by value | buffers `ptr addrspace(1) noalias`; in `add_kernel` emit `addrspacecast → ptr` and `sym_push` the flat pointer so inherited `CodeGen_LLVM` code works; `InferAddressSpaces` folds back to `global_load/store`. |
| Kernarg ABI | driver packs from `kernelParams` | Same: `hipModuleLaunchKernel(..., kernelParams, nullptr)`; HIP reads `.args[].offset/.size` from the `amdhsa.kernels` msgpack note and appends hidden args itself. No manual packing. Debug fallback `HL_HIP_PACK_KERNARGS=1` uses `extra` with `HIP_LAUNCH_PARAM_BUFFER_POINTER`. |
| Thread / block ids | `llvm.nvvm.read.ptx.sreg.tid/ctaid.*` | `llvm.amdgcn.workitem.id.{x,y,z}` / `llvm.amdgcn.workgroup.id.{x,y,z}` (i32). |
| Barrier | `llvm.nvvm.barrier0` | `fence syncscope("workgroup") release; llvm.amdgcn.s.barrier; fence syncscope("workgroup") acquire` (`s_barrier` alone is not a memory fence). `MemoryFenceType::Device` ⇒ `syncscope("agent")`. |
| Shared memory | `null addrspace(3)` as base | **Never `null addrspace(3)`** (AMDGPU LDS null is `0xFFFFFFFF`). Declare `@__halide_dynamic_lds = external addrspace(3) global [0 x i8], align 16` (dynamic-LDS idiom) and `sym_push(name, @__halide_dynamic_lds)` for `is_gpu_shared` allocations; `hipModuleLaunchKernel(sharedMemBytes)` sizes the group segment. Static `group_segment_fixed_size` stays 0. |
| Private allocs | `CreateAlloca` in entry | Same; amdgcn DL has `A5` ⇒ allocas are AS5; cast to flat in the symbol table. |
| Warp/lane ops | `LowerWarpShuffles` (CUDA only, ≤32) → `llvm.nvvm.shfl.sync.*` | Generalize to `(DeviceAPI, warp_size)`: 64 on gfx9, 32 on gfx10+ (`+wavefrontsize32`). Emit `halide_amdgpu_shfl_{idx,down,up}_{i32,f32}(val, lane_or_delta, width)` defined in `amdgpu_dev.ll` on `llvm.amdgcn.ds.bpermute(src_lane*4, val)`; lane id via `llvm.amdgcn.mbcnt.lo/hi`. |
| Math | `ptx_dev.ll` + `libdevice.10.bc` | `amdgpu_dev.ll` (native `llvm.sqrt/floor/ceil/trunc/round/fabs/fma/exp2/log2/minnum/maxnum`, `llvm.amdgcn.rcp/rsq`) + phase-2 `ocml.bc` for `sin/cos/tan/asin/…/exp/log/pow/erf` (`__ocml_*_f32/f64`), with `__oclc_*` control constants defined in-module (`__oclc_wavefrontsize64`, `__oclc_ISA_version`, `__oclc_daz_opt`, `__oclc_finite_only_opt=0`, `__oclc_unsafe_math_opt=0`, `__oclc_correctly_rounded_sqrt32=1`, `__oclc_ABI_version`). Phase 1 ships with `amdgpu_dev.ll` only. |
| Trap / asserts | `halide_ptx_trap` | `halide_amdgpu_trap` = `llvm.trap` (s_trap 2 ⇒ launch failure on sync). |
| Atomics | ints ≥32, f32, f64 (≥sm_61) | ints 32/64; f32 (HW on gfx908+, else CAS); f64 on gfx90a/942/950. Add `!amdgpu.no.fine.grained.memory` on `atomicrmw` (LLVM ≥19 needs it for HW FP atomics). |
| FP / denormals | `nvvm-reflect-ftz` | `"denormal-fp-math-f32"="preserve-sign,preserve-sign"` unless `StrictFloat` ⇒ `"ieee,ieee"`. `AllowFPOpFusion` as PTX. |
| Vector width | 64 | 128 (dwordx4). `promote_indices()=false`, `use_soft_float_abi()=false`, keep f16. |
| mcpu / mattrs | `sm_NN` / `+ptxNN` | `HL_HIP_ARCH` ⇒ that, else feature map, else `internal_error`; `mattrs()`: gfx10/11/12 ⇒ `"+wavefrontsize32"`, gfx9 ⇒ `""`; `HL_HIP_MATTRS` appended. |
| Code object version | n/a | Module flag `amdhsa_code_object_version` = 500 default (ROCm 5–7), `HL_HIP_CODE_OBJECT_VERSION=6` opt-in (required for `*-generic`). Set in `init_module()` before passes. |
| Pipeline | new-PM O3 + legacy emit `AssemblyFile` | Same skeleton + `target_machine->registerPassBuilderCallbacks(pb)`, then `addPassesToEmitFile(..., CodeGenFileType::ObjectFile)` ⇒ ET_REL ELF. |
| Link | none | In-process lld: write ET_REL to `TemporaryFile`, argv `{"ld.lld","-flavor","gnu","-m","elf64_amdgpu","--no-undefined","-shared","-o",<out>,<in>}`, `LLD_HAS_DRIVER(elf)`, `lld::lldMain(args, outs, errs, {{lld::Gnu, &lld::elf::link}})` under a global mutex; honour `canRunAgain`. Fallback `HL_HIP_USE_EXTERNAL_LLD=1` / no lld libs ⇒ `run_process(ld.lld …)` (`HL_HIP_LLD` path). `compile_to_src` returns raw `.hsaco` bytes (no trailing NUL). `HL_HIP_DUMP_OBJ=<dir>` writes `.o`/`.hsaco`. |

## 4. Runtime design: `src/runtime/hip.cpp`

1. Includes/namespace: `HalideRuntimeHIP.h`, `device_buffer_utils.h`, `device_interface.h`, `gpu_context_common.h`, `mini_hip.h`, `printer.h`, `scoped_mutex_lock.h`; `namespace Halide::Runtime::Internal::Hip`.
2. Library loading: `lib_names[] = { WINDOWS: "amdhip64_7.dll","amdhip64_6.dll","amdhip64.dll" | else "libamdhip64.so.7","libamdhip64.so.6","libamdhip64.so" }`, `HL_HIP_LIB` override first. `hip_functions.h`: `hipInit, hipGetDeviceCount, hipDeviceGet, hipDeviceGetAttribute, hipDeviceGetName, hipDeviceComputeCapability, hipSetDevice, hipGetDevice, hipDeviceSynchronize, hipModuleLoadData, hipModuleUnload, hipModuleGetFunction, hipModuleLaunchKernel, hipMalloc, hipFree, hipMemcpyHtoD/DtoH/DtoD[Async], hipStreamSynchronize, hipGetErrorName`; optional `hipDrvMemcpy3D, hipMemGetInfo, hipPointerGetAttributes`. One library provides everything.
3. Context model: "context" = device ordinal as `void*`. Hooks `halide_hip_acquire_context_t(void *user_context, int *device, bool create)`, `release_context`, `get_stream(void*, int device, void **stream)`, `halide_set_hip_*`. Default acquire creates once: `hipInit(0)`, `hipGetDeviceCount`, pick `halide_get_gpu_device` else max `hipDeviceAttributeMultiprocessorCount`. RAII `Context { int device; acquire; ensure_libhip_init; hipSetDevice }`. `GPUCompilationCache<int, hipModule_t>` keyed by device ordinal.
4. Kernels: `compile_kernel` → `hipModuleLoadData`; `hipErrorNoBinaryForGpu` ⇒ `"code object built for <arch> does not match device; set hip_gfx*/HL_HIP_ARCH"`. `halide_hip_initialize_kernels` / `finalize_kernels` via the cache.
5. `halide_hip_run`: identical signature/body to `halide_cuda_run` with `hipModuleGetFunction` + `hipModuleLaunchKernel(f, bx,by,bz, tx,ty,tz, shared_mem_bytes, stream, translated_args, nullptr)`; `DEBUG_RUNTIME` ⇒ sync + timing.
6. Memory: `device_malloc` with free-list reuse (`hipMalloc`, retry after `release_unused_device_allocations` on OOM), `device_free`, multidimensional copy recursion (`hipDrvMemcpy3D` when available), copy_to_device/host, device_and_host_malloc/free, buffer_copy, device_crop (`dst->device = src->device + calc_device_crop_byte_offset`), device_slice, device_release_crop, device_sync, device_release (unload modules, free pool; no `hipDeviceReset`), wrap/detach/get device ptr.
7. Capability: `halide_hip_compute_capability(uc, &major, &minor)` returns gfx as `major = generation (9/10/11/12)`, `minor = low byte` (gfx942 → 9/0x42, gfx90a → 9/0x0a, gfx1100 → 11/0, gfx1201 → 12/1) from `hipDeviceAttributeGcnArch`, fallback `hipDeviceComputeCapability`. `Target.cpp` decodes to a `hip_gfx*` feature or warns to use `HL_HIP_ARCH`.
8. Device interface: `hip_device_interface_impl` / `hip_device_interface` with CUDA's field order; constructor registers allocation pool; destructor cleanup.
9. Errors: `error_hip` → `halide_error_code_gpu_device_error`; names via dynamically loaded `hipGetErrorName` with static fallback table; launch failure after a kernel ⇒ "Illegal instruction or Halide assertion failure inside kernel".
10. Debug variant free from the runtime CMake suffix loop; `JITModule.cpp` needs `RuntimeKind::HIPDebug`.
11. `HalideRuntimeHIP.h`: `halide_hip_device_interface()`, `initialize_kernels`, `run`, `finalize_kernels`, `wrap_device_ptr`, `detach_device_ptr`, `get_device_ptr`, `release_unused_device_allocations`, hook typedefs/setters, `compute_capability`.

## 5. Build-system changes
- Root: `AMDGPU` in `OPTIONAL_COMPONENTS`; the existing loop defines `WITH_AMDGPU` and links `Halide_LLVM::AMDGPU`.
- `FindHalide_LLVM.cmake`: if AMDGPU found and LLD found ⇒ `target_link_libraries(Halide_LLVM::AMDGPU INTERFACE lldELF lldCommon)` + include dirs; else disable AMDGPU with a status message (same policy as WebAssembly).
- `CodeGen_LLVM.cpp` initialize macros; `LLVM_Headers.h` lld includes gated on `WITH_AMDGPU` too.
- `runtime/CMakeLists.txt`: modules `hip windows_hip`, `amdgpu_dev`, header; phase-2 ocml embedding rule gated on AMDGPU component.
- `LLVM_Runtime_Linker.cpp` initmods; Python enums; Makefile; vcpkg feature `amdgpu`.
- Note: ROCm's `/opt/rocm/llvm` lacks the WebAssembly component Halide requires — use upstream LLVM (apt.llvm.org or release tarballs) with lld.

## 6. Test plan
**A. No AMD hardware (every CI runner)**
1. Internal test `CodeGen_AMDGPU_Dev::test()`: 2-arg kernel (`out[x] = in[x] + p`) for `hip_gfx942` and `hip_gfx1100`; parse `compile_to_src()` result with `llvm::object::ELFObjectFile<ELF64LE>`; assert `e_machine==224`, OSABI 64, `ET_DYN`, ABI version 3 (COV5) / 4 (COV6), `e_flags & EF_AMDGPU_MACH == 0x4c / 0x41`, `NT_AMDGPU_METADATA` note with `amdhsa.kernels[0].args` (2 explicit) and `.kernarg_segment_size >= 16`, no undefined dynsyms. Second case with a shared allocation: `.group_segment_fixed_size == 0`, `__halide_dynamic_lds` resolved.
2. AOT compile-only sweep: apps (`local_laplacian, bilateral_grid, lens_blur, nl_means, stencil_chain, camera_pipe, harris, interpolate`) and `test/generator` under `HL_TARGET=host-hip-hip_gfxNNNN`; inspect embedded objects with `llvm-readelf --notes --file-header`, `llvm-objdump -d --mcpu=…` (no `flat_load` in hot loops), `llvm-readelf -r` (no unresolved relocs).
3. Negative JIT test: without `libamdhip64`, `HL_JIT_TARGET=host-hip-hip_gfx942 correctness_hello_gpu` fails cleanly with a "could not load libamdhip64" device error.
4. `LowerWarpShuffles` unit test for wave64 (`gpu_lanes(64)` accepted on gfx942, rejected on gfx1100).

**B. Needs hardware**: everything that launches (kernarg ABI, LDS addressing, barriers, atomics, shuffles, copies, allocation reuse, object lifetimes, in-kernel asserts).

**C. Hardware script (AMD Developer Cloud MI300X gfx942; Radeon gfx1100/gfx1201)** — see `scripts/hardware-test.sh`: build Halide with upstream LLVM + lld, `HL_JIT_TARGET=host-hip-hip_gfx942`, `ctest -L correctness -R "hello_gpu|^correctness_gpu_|device_|async_|atomics|float16|vector_reductions"`, autodetect and `HL_HIP_ARCH` variants, debug runtime run, apps HIP vs OpenCL vs Vulkan into `results.csv`, `rocprofv3 --kernel-trace` LDS check, `gpu_lanes(32/64)` cases.

## 7. Risks and mitigations
| Risk | Mitigation |
|---|---|
| 2018 failure: hand-rolled relocation patching | Always produce ET_DYN via lld with clang's exact link line; test asserts ET_DYN + zero unresolved relocs. |
| LDS null is `0xFFFFFFFF` | Dynamic-LDS `external addrspace(3) global [0 x i8]` idiom; tests `gpu_dynamic_shared`, `gpu_mixed_shared_mem_types`, `gpu_reuse_shared_memory`; assert `group_segment_fixed_size==0`. |
| `A5` allocas vs inherited AS0 assumptions | Cast to flat immediately; `verifyModule` after each kernel; per-site fixes. |
| wave64 vs wave32 | Warp size from `mcpu`; force `+wavefrontsize32` on gfx10+; document limits (≤64 gfx9, ≤32 gfx10+). |
| kernarg ABI | Use `kernelParams`; keep packed path for diagnosis; `gpu_arg_types`/`gpu_mixed_dimensionality` cover scalar types. |
| LLVM drift (DL, COV default, `s.barrier` on gfx12) | DL from TargetMachine; COV5 pinned; intrinsic lookup by name with fallback list; CI on LLVM 21 and 22. |
| COV vs ROCm runtime | COV5 default; actionable error text on `hipErrorNoBinaryForGpu` / `InvalidImage`. |
| ocml size/licensing | Apache-2.0 w/ LLVM exception; phase 1 works without it. |
| lld global state | Global mutex; `canRunAgain`; external fallback; never `exitLld`. |
| FP atomics on fine-grained memory | `hipMalloc` is coarse-grained; `!amdgpu.no.fine.grained.memory`. |
| JIT arch autodetect ambiguity (gfx9.0) | `GcnArch` attr first; `HL_HIP_ARCH` always wins; loud warning. |

## 8. Work breakdown
| # | Task | Est. | Depends |
|---|---|---|---|
| T1 | Target/DeviceAPI plumbing | 8 h | — |
| T2 | Build system + empty `CodeGen_AMDGPU_Dev` stub that links | 6 h | — |
| T3 | `CodeGen_AMDGPU_Dev` core | 16 h | T2 (T1 names) |
| T4 | Linking + ELF validation test | 8 h | T3 |
| T5 | Device math (`amdgpu_dev.ll`, later ocml) | 10 h | T2, T3 |
| T6 | Runtime (`hip.cpp` family) | 16 h | T1, T2 |
| T7 | Warp shuffles + atomics | 10 h | T3, T5 |
| T8 | Validation & docs (compile sweep, CI, `doc/HIP.md`, hardware script, BRIEF) | 12 h | T1–T7 |
Critical path T2 → T3 → T4/T5 → T7 → T8 ≈ 52 h serial; T1 and T6 parallel. Total ≈ 86 h.
