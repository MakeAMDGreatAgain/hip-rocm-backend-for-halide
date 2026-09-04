# Draft: "CUDA-only -> runs on AMD" port candidates (from notes 01-07)

Scoring axes: Demand (D), Tractability for an agent (T), Verifiability (V: tests/benchmarks exist), Upstream receptiveness (U), Hardware reach (H: consumer+datacenter).

## Tier A — flagship ports (zero or rejected AMD path today, large user base)
1. CTranslate2 / faster-whisper ROCm backend — no ROCm backend; issue #1370; hipified forks prove feasibility; whisper users huge. D5 T4 V5 U3 H5.
2. ExLlamaV3 ROCm — CUDA-only, "ROCm support" on maintainer to-do; r/LocalLLaMA flagship. D5 T3 V4 U4 H5(consumer).
3. Marlin-class INT4 (AWQ/GPTQ) kernels for vLLM on AMD — ❌ in vLLM matrix; #31689 help-wanted; lose 2.6x/10.9x. D5 T2 V5 U4 H4.
4. SageAttention for ROCm (RDNA3/4 + CDNA) — upstream closed not-planned (#125); EmbeddedLLM fork crashes RDNA4; unlocks Wan/Hunyuan/LTX video gen. D4 T3 V4 U2(upstream)/5(fork) H5.
5. Mamba/SSM selective-scan + causal-conv1d optimized HIP kernels — CUDA-only optimized; LightOn had to write own; #671. D3 T3 V4 U4 H4.

## Tier B — "no AMD path" clean gaps
6. torch-scatter / torch-sparse / PyG ROCm wheels (+ DGL PR #7922 help). D3 T4 V5 U4 H5.
7. Rust candle HIP backend (candle has none; cudarc has no HIP twin). D3 T3 V4 U3 H5.
8. mistral.rs ROCm. D3 T3 V4 U4 H5.
9. Halide HIP backend. D2 T3 V5 U3 H5.
10. llama.cpp XDNA NPU backend (#21725 open). D4 T2 V4 U4 H(Ryzen AI only).
11. Liger Kernel ROCm — PR #1297 already open; help land it. D3 T4 V5 U4 H4.
12. FlashInfer ROCm fork: missing sampling (top-k/top-p) + norm kernels. D3 T3 V5 U3 H(Instinct).
13. HF text-embeddings-inference (TEI) ROCm — "BROKEN", PR stalled 10+ months. D4 T3 V5 U3 H5.
14. NVIDIA Warp HIP — NVIDIA's own PR #1770 WIP; assist. D2 T2 V4 U? H5.
15. Taichi AMDGPU — open since 2020 (#6434). D2 T2 V4 U2 H5.

## Tier C — "supported but broken" correctness fixes (high-visibility, small PRs)
16. llama.cpp HIP silent wrong output on gfx1151 Strix Halo (#27556) + decode slowdown >1K ctx (#27856) + gfx1201 first-matmul crash (#27670) + Windows hipblas.dll (#26996).
17. PyTorch RDNA4 correctness: expandable_segments NaN bf16 gfx1201 (#195202); RX 9060 XT 8GB VRAM cap (#184880); SDPA fails gfx1100 (#194498); AOTriton missing gfx1151 (ROCm #5404, TheRock #1364).
18. HIP graphs / torch.compile reduce-overhead broken on ROCm (#155684 #155720; AMD-Skills admits). 
19. MIOpen silent 100x fallback detector (pytorch #169857 pattern).
20. vLLM RDNA4 FP8 WMMA community patch (25-63%) — get it MERGED (#28649).
21. Stuck outsider RDNA4 PRs in rocm-libraries: #5243 generic Tensile target, #5462 MIOpen CK gfx1201, #6365 hipBLASLt FP8 gfx120x — produce CI evidence + reviews.

## Tier D — recurring loops (not one-offs)
22. Day-0 model ports: every major release w/ custom CUDA ops -> MI300X + Strix Halo repro + numbers within 24h.
23. Living "Does it run on AMD?" dashboard: automated nightly matrix of top ~100 OSS AI projects x {MI300X, RX 9070 XT, Strix Halo, Windows} with pass/fail + perf vs Vulkan/CUDA. Existing lists fragmented (awesome-rocm 33★). 
24. Consumer-GPU issue triage/repro service on legacy tracker (273 open) + TheRock.
25. Composition/accuracy test grid (disagg x FP4 x WideEP x spec-decode) — the thing SemiAnalysis says AMD lacks clusters for.

## Lane AMD has left empty (from 06): Hyperloom/Apex map consumer archs to None; no ambassador/bounty program; outsider PRs sit 6 months; 0 help-wanted issues. Anthropic 2GW deal (Jul 2026) explicitly has Claude "accelerate ROCm development".
