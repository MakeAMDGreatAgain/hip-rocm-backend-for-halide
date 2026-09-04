# Diffusion/media + HPC on AMD (Sep 2026) — key findings
MEDIA
- ComfyUI: first-class Linux (AMD docs+Docker, ROCm 7.1/7.2); Windows community forks only (ComfyUI-Zluda patientx, ROCm-Windows-Native). ~25-40% slower than 4090 tier.
- A1111/Forge stalled. InvokeAI rocky (#7006 container GPU, #8883 Ryzen AI Max fails, Windows-ROCm 7.x VAE decode 30s+ regression). SD.Next most AMD-friendly (ZLUDA).
- Amuse (TensorStack+AMD): ONNX/DirectML, not ROCm.
- faster-whisper/CTranslate2: NO ROCm backend; community hipified forks only (claim 60% faster than whisper.cpp). whisper.cpp HIP upstream OK.
- TTS (Coqui, Kokoro, Chatterbox, F5): community incantations; Kokoro-FastAPI "experimental" ROCm; MIOpen cache fix guides.
- Video gen (Wan, Hunyuan, LTX, CogVideoX): "unstable ROCm as of Apr 2026"; flash-attn-2 made Wan ~50% SLOWER on 7900XTX; Kijai wrapper de facto path.
- SageAttention upstream CLOSED AMD as not-planned (#125); EmbeddedLLM/SageAttention-rocm fork; crashes RDNA4.
- xformers "experimental" on ROCm 7.1; CK lacks RDNA3 support. Official FA2 excludes RDNA3 ("longest running RDNA3 issue").
HPC/SCI
- RAPIDS: no drop-in; AMD ROCm-DS (hipDF/hipML) separate fork at RAPIDS 25.02 parity. Polars GPU engine NVIDIA-only.
- CuPy: experimental label since v9; cupy-rocm-7-0 Aug 2026. Numba: in-tree ROC unmaintained -> ROCm/numba-hip experimental, pinned 0.58-0.61.
- Taichi: AMD request open since Jan 2020 (#6434). NVIDIA Warp: NVIDIA's own HIP shims PR #1770 WIP Aug 2026.
- GROMACS HIP full support (2026 manual), LAMMPS-KOKKOS official, Kokkos first-class (Frontier). OpenMM HIP under-documented. AMBER pmemd HIP beta ("check outputs carefully").
- PyG/torch-scatter: NO ROCm wheels. DGL: community PR #7922 in progress (May 2026).
- SYCL: AdaptiveCpp OK, "less mature". Julia AMDGPU.jl 73 open issues. Rust: wgpu/burn OK; candle NO ROCm; cudarc CUDA-only, no HIP equivalent. Go/Zig: no bindings.
- Vulkan compute = durable escape hatch; Kompute low activity. OpenCL quietly neglected.
