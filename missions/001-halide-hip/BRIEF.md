# Halide native HIP/AMDGPU backend — handoff brief

For: AMD ROCm / developer-ecosystem reviewers and Halide maintainers.
From: the amd-port-agent AI agent team (human sponsor: the repository owner).
Date: 2026-09-04. Status: **compile-only verification; hardware validation pending.**

Conventions used below: every external claim carries a link that was fetched
while writing this brief, or is marked *(not verified)*. Numbers that do not
exist yet are written as `<pending>`; nothing has been estimated or invented.

## 1. Summary

This package adds a native HIP/ROCm GPU backend to Halide: device kernels are
compiled by LLVM's AMDGPU target into HSA code objects inside Halide's own code
generator, linked in-process with lld, and launched by a new runtime module
(`src/runtime/hip.cpp`) through `libamdhip64`, loaded dynamically — the same
architecture as Halide's CUDA/NVPTX backend, with the AMD-specific differences
(LDS null pointer, wave32/wave64, kernarg ABI, code-object versions, ET_DYN
linking) handled explicitly. The honest headline: **this is the first native
ROCm/HIP backend for Halide, but not the first way to run Halide on AMD GPUs —
the OpenCL and Vulkan backends already do that.** What the native path adds is
architecture-specific machine code from Halide's LLVM pipeline (wave-size-aware
`gpu_lanes`, native LDS/atomics/f16, no OpenCL/SPIR-V translation layer) and a
foundation for ROCm-specific work (ocml math, profiler integration). The code
builds against LLVM 22.1.8 (Halide `main` requires LLVM ≥ 21) and produces
structurally valid code objects for gfx942/gfx1100/gfx1151/gfx1201 —
**it has not yet been executed on an AMD GPU.**

## 2. Why this is a first — prior art

| Item | What | Dates / state | Source |
|---|---|---|---|
| Issue #2443 "ROCm backend for AMDGPU?" | masahi (TVM ROCm backend author) asks whether Halide wants a ROCm backend PR. abadams: "We have no immediate plans to support it, but would welcome a PR. Seems like it would be very similar to our cuda backend." | opened + closed 2017-10-15 | https://github.com/halide/Halide/issues/2443, comments via https://api.github.com/repos/halide/Halide/issues/2443/comments |
| PR #2730 "added makefile changes to build for amdgpu" | Build-system precursor to #2734. | 2018-02-07 → closed 2018-02-08 | https://github.com/halide/Halide/pull/2730 (author *not verified*) |
| **PR #2734 "added initial source code support for AMDGPU backend"** | aditya4d (branch `rocm-src-v1`), 28 commits, 32 files, +2,813/−12. Added `src/CodeGen_AMDGPU_Dev.{cpp,h}`, `src/AMDGPUOffload.{cpp,h}`, `src/runtime/amdgpu.cpp`, `HalideRuntimeAMDGPU.h`, features `AMDGPUGFX900/GFX803`, and an AMDGPU relocator built on Halide's in-tree ELF linker framework (`src/Elf.{cpp,h}`, shared with Hexagon; the AMDGPU-specific code is in `src/AMDGPUOffload.cpp`, +599 lines). Never merged. | opened 2018-02-09, closed 2019-03-26, not merged | https://github.com/halide/Halide/pull/2734, files: https://api.github.com/repos/halide/Halide/pulls/2734/files |
| Why #2734 stalled (from the PR thread) | 2018-02-27 abadams: "I'd rather not merge semi-functional things into master. This part of the compiler is very stable." 2018-02-28 aditya4d asks about adding lld as a dependency; dsharletg points to the Hexagon in-tree linker instead. 2018-04-12 aditya4d: `std::bad_alloc` from `hipModuleLoadData`, "When I put print statement in `relocate` function, it is not printed out at runtime". April–June 2018: extended GOT/relocation discussion with t-tye (AMD): "If the symbol is not defined then an ABSOLUTE relocation must be used". 2019-03-26 aditya4d: "Not working on it anymore. Closing PR". | — | https://api.github.com/repos/halide/Halide/issues/2734/comments |
| **PR #8382 "Remove vestigial AMDGPU backend"** | alexreinking removes the remnants: the backend "was started in 2018 but never completed. Removing the stale references reduces confusion." Merge commit `ba085221`. | opened + merged 2024-08-09 | https://github.com/halide/Halide/pull/8382 |
| Tracker search: `rocm` | 3 hits: #2443, #2734, and open PR #9389 "Add scheduling directive to cap the max GPU registers" (2026-08-25; matched the search, relevance to ROCm *not verified*). | fetched 2026-09-04 | https://api.github.com/search/issues?q=repo:halide/Halide+rocm |
| Tracker search: `hip` | 2 hits, only #2734 relevant (the other is a CMake issue matching a substring); `hip in:title` → 0. | fetched 2026-09-04 | https://api.github.com/search/issues?q=repo:halide/Halide+hip |
| Tracker search: `amdgpu` | 17 hits; the only backend-related ones are #2443, #2730, #2734, #8382 and #2065 (2017, "Allow LLVM targets to run custom bitcode optimization passes", relation *not verified*). The rest are generic LLVM build-failure reports. | fetched 2026-09-04 | https://api.github.com/search/issues?q=repo:halide/Halide+amdgpu |
| Halide on AMD today | `README.md` at the pinned SHA lists GPU compute APIs as "CUDA, OpenCL, Apple Metal, Microsoft Direct X 12, Vulkan" — no HIP/ROCm. `doc/Vulkan.md` documents AMD as a supported Vulkan vendor (driver via `mesa-vulkan-drivers` / AMD driver) with all correctness tests passing on tested Linux/Windows configs; OpenCL on AMD works through ROCm's or Mesa's OpenCL ICD. | pinned SHA `0537858c` (2026-09-04) | local checkout; https://github.com/halide/Halide/blob/main/doc/Vulkan.md |
| Forks / wheels / blogs with a working Halide-HIP path | None found. *(Search limited to the GitHub tracker and web search performed during target selection — see `research/08-port-candidates-draft.md`; not exhaustive.)* | — | *(not verified beyond the above)* |

How this design differs from the 2018 attempt: #2734 emitted an AMDGPU
relocatable object and tried to turn it into a loadable code object with
Halide's hand-written ELF linker (originally built for Hexagon), which meant
re-implementing AMDGPU GOT handling and `R_AMDGPU_ABS64` / `R_AMDGPU_RELATIVE64`
relocations by hand; that is where it died. This backend never touches
relocations: it invokes lld's ELF driver in-process with clang's exact HIP
device link line (`ld.lld -flavor gnu -m elf64_amdgpu --no-undefined -shared`)
and asserts in a test that the output is `ET_DYN` with zero relocations. The
other AMD-specific pitfalls (LDS null is `0xFFFFFFFF`, wave64 vs wave32,
`kernelParams` packing, COV5 vs COV6) are each handled and documented
(`DESIGN.md` §3, §7).

## 3. Who built it and how

- **Authorship.** All code, tests, scripts and documents in this package were
  written by an AI agent team: Claude Fable 5.1 running in Claude Code, with
  one orchestrating agent decomposing the work (tasks T0–T9 in `STATUS.md`)
  and dispatching sub-agents that each owned a set of files: 11 implementation
  / review sub-agent sessions (T1, T2, T6, T3+T4, T5+T7 in two sessions, T8,
  T9 review in two sessions, T9 fixes) plus 12 research, sizing, prior-art and
  design agents before code was written. A human directed target selection,
  design review, and every external action; no upstream PR, issue or post has
  been created. An independent reviewer agent audited the backend against the
  CUDA implementation (T9, `REVIEW-T9.md`: 0 blockers, 3 major, 10 minor, 6
  nits; all actionable items fixed in patch 0007 and re-tested).
- **Disclosure policy.** Halide's `CONTRIBUTING.md` (fetched 2026-09-04,
  https://raw.githubusercontent.com/halide/Halide/main/CONTRIBUTING.md) states:
  "If a significant part of your contribution was generated by an AI tool, you
  must note this in the commit message using a `Co-authored-by` trailer, as
  described in the Code of Conduct." The Code of Conduct
  (https://raw.githubusercontent.com/halide/Halide/main/CODE_OF_CONDUCT.md)
  adds that "any code generated by an AI tool (such as a large language model)
  must be clearly identified as such in the commit message using
  'Co-authored-by: The name of the tool'." Every Halide commit in `patches/`
  carries `Co-authored-by: Claude Fable 5.1 <noreply@anthropic.com>` (rule in
  `AGENT.md`; verified on the exported series: 7 of 7 patches). CONTRIBUTING.md
  also requires tests for new features, clang-format/pre-commit cleanliness,
  and Python-binding updates for public API changes; the patch series follows
  those (Python enums updated; tests added; `clang-format --dry-run -Werror`
  with the repo `.clang-format` is clean on every changed C++ file; the full
  pre-commit hook set was not executed on this machine).
- **Method.** Design was grounded in a file/symbol-level survey of Halide
  `main` (`DESIGN.md` §0) and verified LLVM/HIP facts (ELF constants, code
  object layout, HIP module API). Nothing is reported as verified unless a
  command was run and its output checked (`STATUS.md`).

## 4. Design summary

Full design: [`DESIGN.md`](DESIGN.md). The six decisions that matter most:

1. **Mirror the CUDA backend, not OpenCL.** `CodeGen_AMDGPU_Dev` derives from
   `CodeGen_LLVM` + `CodeGen_GPU_Dev` exactly like `CodeGen_PTX_Dev`; the runtime
   `hip.cpp` is a section-by-section mirror of `cuda.cpp` (same hook API,
   compilation cache, free-list allocator, multidimensional copies).
2. **Link with lld in-process, produce ET_DYN.** Object → `lld::lldMain` (ELF
   driver, clang's HIP link line) under a global mutex; external `ld.lld`
   fallback (`HL_HIP_USE_EXTERNAL_LLD`). This is the direct answer to the 2018
   failure.
3. **Dynamic-LDS idiom for shared memory.** `@__halide_dynamic_lds = external
   addrspace(3) global [0 x i8]` bound to every `GPUShared` allocation;
   `sharedMemBytes` at launch sizes it; `group_segment_fixed_size` stays 0.
4. **Let HIP pack kernel arguments.** `hipModuleLaunchKernel(..., kernelParams,
   nullptr)`; HIP reads `.args` offsets from the code object's metadata. A
   hand-packed path exists only for diagnosis (`HL_HIP_PACK_KERNARGS`).
5. **Explicit architecture features + escape hatch.** `hip` plus one of
   `hip_gfx90a/942/950/1030/1100/1151/1201` (LLVM `-mcpu`), `HL_HIP_ARCH` for
   any other part, JIT autodetect from `gcnArchName`. AOT requires an explicit
   arch because AMD code objects are not forward-compatible. Wave size follows
   the arch (`gpu_lanes` ≤ 64 on gfx9, ≤ 32 on gfx10+).
6. **COV5 by default, no ROCm at build time.** `amdhsa_code_object_version =
   500` for ROCm 5.x–7.x compatibility (`HL_HIP_CODE_OBJECT_VERSION=6` opt-in);
   `libamdhip64` is resolved at run time only; Halide builds with upstream LLVM
   (ROCm's `/opt/rocm/llvm` lacks the WebAssembly target Halide needs).

Phase 2 (not in this package): vendored `ocml` for transcendental math, multi-arch
offload bundles, Windows validation.

## 5. What is verified vs. not

From `STATUS.md` (authoritative, updated every loop tick):

Verified (command run, output checked):
- Pristine Halide `main` @ `0537858c273bd12f38ccfb0c115efb050dde94e1` builds with
  the LLVM 22.1.8 release tarball in WSL Ubuntu 24.04 (2026-09-04 13:47).
- Halide `main` requires LLVM 21+; `src/CodeGen_GPU_Host.cpp` no longer exists
  (offload is `src/OffloadGPULoops.cpp`); `WITH_AMDGPU` is referenced nowhere
  in the tree (confirms #8382 removed everything).
- The full backend (7 patches, 50 files, +9,073/-57 lines) builds with LLVM
  22.1.8 (`WITH_AMDGPU`, lld linked, 10 HIP runtime bitcode modules).
- M2: `correctness_hip_code_object` passes: linked ET_DYN code objects for
  gfx942 (plain, shared-memory, atomic variants) and gfx1100, with the ELF
  header, mach flags, `NT_AMDGPU_METADATA` (decoded: exact 6-entry kernarg
  layout), kernel descriptors, zero relocation entries and no undefined
  symbols asserted (`results/2026-09-04-m2-linked-code-objects.txt`).
- M3 (compile side): `compile-verify.sh`: **56/56 rows pass**: 5 smoke
  pipelines + 9 apps (local_laplacian, bilateral_grid, blur, camera_pipe,
  harris, interpolate, lens_blur, nl_means, stencil_chain) x gfx942 / gfx1100 /
  gfx1151 / gfx1201 (`results/2026-09-04-compile-verify-run5-apps.md`). The
  same smoke pipeline also links for gfx90a, gfx950 and gfx1030.
- Regression: the full upstream `ctest -L correctness` suite on the modified
  tree with the CPU target: 464 run, **0 failed**, 53 skipped (GPU/vendor
  tests that self-skip without a device). `correctness_target`,
  `correctness_hip_code_object`, `correctness_hip_warp_shuffles` and
  `error_hip_gpu_lanes_too_wide` pass.
- Device math: 65 helpers in `amdgpu_dev.ll` verified with `llvm-as`,
  `opt -passes=verify`, and `llc` for six architectures; accuracy measured on
  the host build of the same IR (worst f32 4.75 ulp for `asin`; `pow_f64`
  at most 0.61 ulp after a double-double rewrite; bounds in the file header).
- CI: the workflow in `ci/halide-hip.yml` is written but has **not** been
  executed (no GitHub push has been made).
- **Everything that launches a kernel — kernarg ABI, LDS addressing, barriers,
  atomics, shuffles, copies, allocation reuse, object lifetimes, in-kernel
  asserts, and all performance numbers — is unverified.** No AMD GPU was
  available to the authors. Results will be recorded in
  `results/hardware-<arch>-<date>.{json,md}` (M5).

## 6. How to verify

Three scripts (`scripts/README.md` has one paragraph each) and one workflow:

1. `scripts/wsl-setup.sh` — reproducible toolchain and build without sudo or
   ROCm (WSL2 or any Ubuntu 22.04/24.04): pip cmake/ninja, LLVM 22.1.8 release
   tarball (AMDGPU + lld), static zstd, CMake configure with `-DLLD_DIR`, build
   `libHalide`.
2. `scripts/compile-verify.sh` — no GPU: AOT-compiles five smoke pipelines
   (`scripts/smoke/smoke.cpp`: gpu_tile, shared-memory producer, atomic
   histogram, float16, int64 index math) and the GPU-scheduled apps
   (`local_laplacian, bilateral_grid, blur, harris, interpolate, lens_blur,
   nl_means, stencil_chain, camera_pipe`) for each arch, then checks every code
   object with `llvm-readelf`/`llvm-objdump` (EM_AMDGPU, AMDGPU-HSA, ET_DYN,
   e_flags MACH byte, NT_AMDGPU_METADATA, zero relocations, `ds_*` +
   `s_barrier` in the shared-memory kernel). Writes `results/compile-verify-<date>.md`.
3. `scripts/hardware-test.sh` — on an AMD box with ROCm (AMD Developer Cloud
   MI300X gfx942 first; Radeon gfx1100/gfx1201): clone at the pinned SHA,
   `git am patches/*.patch`, build, run the GPU correctness subset
   (`HL_JIT_TARGET=host-hip-hip_<arch>`, autodetect, `HL_HIP_ARCH`, `-debug`
   runtime, `gpu_lanes`, AOT generator tests), apps benchmark HIP vs OpenCL vs
   Vulkan (RunGen `--benchmarks=all`), and a `rocprofv3 --kernel-trace`
   one-off. Everything logs to `results/`; failures are collected, never hidden.
4. `ci/halide-hip.yml` — GitHub Actions, ubuntu-24.04, no GPU: checkout at the
   pinned SHA, apply patches, cached LLVM tarball, build, run `ctest -R
   'internal|amdgpu|hip'` and `compile-verify.sh`, upload `results/`.

`wsl-setup.sh` and `compile-verify.sh` have been executed repeatedly (the
first-run fixes, namely local zstd/libpng/libjpeg builds, an installed Halide
package for `apps/`, and autoschedulers on `CMAKE_PREFIX_PATH`, are in the
scripts). `hardware-test.sh` has not been executed: no AMD GPU was available.

## 7. How to adopt / upstream

- The canonical artifact is the patch series `patches/*.patch` (`git
  format-patch` against `halide/Halide` `main` @ `0537858c`), applied with
  `git am --3way`: 7 patches, 50 files, +9,073 / -57 lines (the largest single
  file is the generated `amdgpu_dev.ll`).
- Suggested PR split for review (each independently buildable; other targets
  byte-identical throughout):
  1. **plumbing** — `DeviceAPI::HIP`, target features, `Target.cpp`
     validation/autodetect, IRPrinter, serialization, Python enums, CMake
     `AMDGPU` optional component + lld detection, initialize macros;
  2. **runtime** — `src/runtime/hip.cpp`, `windows_hip.cpp`, `mini_hip.h`,
     `hip_functions.h`, `HalideRuntimeHIP.h`, runtime CMake, `runtime_api.cpp`,
     JIT `RuntimeKind::HIP/HIPDebug`;
  3. **codegen** — `CodeGen_AMDGPU_Dev.{h,cpp}`, `amdgpu_dev.ll`,
     `OffloadGPULoops.cpp` hook, `LLVM_Runtime_Linker.cpp`, warp-shuffle
     generalisation in `LowerWarpShuffles.cpp`;
  4. **tests** — internal ELF-validation test, `gpu_lanes` wave64 case,
     negative JIT test (no `libamdhip64`), test/apps CMake feature lists;
  5. **docs** — `doc/HIP.md`, README table row, `doc/guides.md` entry.
- Halide's policy: maintainers review within about a business week; at most
  one draft PR from this project at a time and only after the human sponsor's
  explicit go (`AGENT.md`).
- What AMD could contribute that this team cannot:
  - a **hardware CI runner** (MI300X and one RDNA3/RDNA4 part) running
    `hardware-test.sh` — Halide currently has no AMD GPU in CI;
  - **ocml integration** (phase 2): vendoring/licensing guidance for the ROCm
    device library the way `libdevice.10.bc` is vendored for CUDA, and
    the `__oclc_*` control-constant conventions;
  - **architecture autodetect on Windows** (`amdhip64.dll` +
    `hipGetDevicePropertiesR0600` behaviour with the HIP SDK / Core SDK), and
    confirmation of which ROCm versions accept COV5 vs COV6 code objects;
  - review of the wave32/wave64 shuffle lowering (`ds_bpermute` + `mbcnt`) and
    of the `!amdgpu.no.fine.grained.memory` atomics annotation against the
    current ROCm compiler behaviour.

## 8. Risks and open questions

From `DESIGN.md` §7, plus what surfaced during implementation:

| Risk | Mitigation / status |
|---|---|
| Repeat of 2018: code object that will not load | lld in-process, ET_DYN + zero relocations asserted by test; `hipErrorNoBinaryForGpu`/`InvalidImage` produce actionable messages. Load on hardware `<pending>`. |
| LDS null pointer is `0xFFFFFFFF` | Dynamic-LDS external global idiom; `group_segment_fixed_size == 0` asserted; `gpu_dynamic_shared`, `gpu_mixed_shared_mem_types`, `gpu_reuse_shared_memory` tests on hardware `<pending>`. |
| Allocas in AS5 vs inherited AS0 code | Cast to flat immediately; `verifyModule` per kernel. |
| wave64 vs wave32 | Warp size from `-mcpu`; `+wavefrontsize32` forced on gfx10+; documented `gpu_lanes` limits; hardware `<pending>`. |
| kernarg ABI | `kernelParams` path (HIP packs from metadata); packed fallback for diagnosis; `gpu_arg_types`/`gpu_mixed_dimensionality` on hardware `<pending>`. |
| LLVM drift (data layout, COV default, gfx12 `s_barrier` split) | Data layout from `TargetMachine`; COV pinned; intrinsics looked up by name; CI on LLVM 21 and 22 (`<pending>`: LLVM 21 matrix entry). |
| ROCm version vs code object version | COV5 default; COV6 opt-in; needs confirmation per ROCm release. |
| JIT arch detection on ROCm 6+ | `hipDeviceAttributeGcnArch` is gone on ROCm 6+; the runtime uses `gcnArchName` from `hipGetDevicePropertiesR0600` with fallbacks; an explicit `hip_gfx*` feature wins and `HL_HIP_ARCH` applies only when no arch feature is set (review fix). Hardware `<pending>`. |
| lld global state / re-entrancy | Global mutex; `canRunAgain` honoured; never `exitLld`; external-process fallback. |
| FP atomics on fine-grained memory | Only `hipMalloc` (coarse-grained) is used; `!amdgpu.no.fine.grained.memory` + `!amdgpu.no.remote.memory` on atomics. Documented caveat: wrapping fine-grained (managed/host-pinned) memory via `halide_hip_wrap_device_ptr` may drop FP atomics on MI200/MI300. |
| Math accuracy without ocml (phase 1) | Generated polynomial library with measured ulp bounds (f32 at most 4.75 ulp worst case, most at most 2.9; f64 at most 4.61); `asin_f32`/`atan2_f32` slightly exceed OpenCL's 4-ulp budget. Phase 2: vendor ocml. In-kernel asserts trap (`s_trap`), which ROCm reports via the queue error callback and may abort the process rather than return an error; to be confirmed on hardware. |
| Windows | Compiles (`windows_hip.cpp`), never run. |
| Performance | Untuned; HIP vs OpenCL vs Vulkan numbers `<pending>` from `hardware-test.sh`. |

Open questions for reviewers: (a) is a `hip_gfx*` feature set the right
granularity for Halide, or should the arch live only in `HL_HIP_ARCH`-style
strings? (b) should the internal test also assert COV6 output under
`HL_HIP_CODE_OBJECT_VERSION=6`? (c) does Halide want `lld` as a hard
dependency of the AMDGPU component (current design: disable the backend if lld
is missing, like WebAssembly)?

## 9. Contact / next steps

- Done: M1 (toolchain/build), M2 (first code objects, ELF test), M3 compile
  side (56/56 sweep, 0 regressions), M4 (this package), independent review
  with fixes. Next: M5, run `scripts/hardware-test.sh` on AMD Developer Cloud
  (MI300X, gfx942) and a Radeon (gfx1100/gfx1201), record
  `results/hardware-*`, fill the remaining `<pending>` rows above; then M6, an
  upstream draft PR to `halide/Halide` using the suggested split, only with the
  sponsor's explicit go. Asks for AMD reviewers: a ROCm CI runner, ocml
  integration guidance, confirmation of trap semantics and COV5/COV6 policy.
- Contact: the repository owner (human sponsor) — see the amd-port-agent
  repository README. Questions about the design can be raised as comments on
  `DESIGN.md`; every section cites the Halide file and symbol it is based on.
