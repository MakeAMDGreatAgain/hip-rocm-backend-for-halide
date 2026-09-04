# AMD's own programs (Sep 2026) — key findings
ACCESS/MONEY
- AMD Developer Cloud (Jun 12 2025): MI300X 1x/8x, MI350/355X; DigitalOcean infra; ~$1.99/hr; $100 credits (~50h) via AI Developer Program, 30-day expiry, form approval. Fireworks $50 credit; AI Endpoint APIs 25M free tokens (DevDay 2026). One-click HF notebooks. India 100k free hours.
- AI Developer Program: Discord w/ AMD engineers, DeepLearning.AI, sweepstakes. NO ambassador tier, NO bounty program.
- ROCm Certification (Jul 24 2026): Associate now; Pro later 2026; Expert 2027; waitlists.
- Contests: GPU MODE Inference Sprint $150k (2025), Distributed kernels $150k (60k submissions; "more AMD kernel data than internet"), E2E Model Speedrun $1.1M (Mar-May 2026, MXFP4 MoE/MLA/GEMM MI355X), HF Robotics hackathons $54k, Pervasive AI $160k+700 hw grants, DevMaster $30k, lablab hackathon w/ "public documentation" track, Modular kernel hackathon. => Instinct kernel leaderboards SATURATED.
OPEN SOURCE
- "100% open source except firmware" (Anush). MES firmware source promised, NOT confirmed landed. MES faults live issue class (#6658 #6630 Aug 2026).
- Monorepos rocm-libraries + rocm-systems (Apr 2025); TheRock production w/ ROCm 7.14 (Jul 15 2026); ROCm/ROCm renamed legacy-rocm-build (Aug 21 2026). ROCm 10.0 released Aug 27 2026 (6-week cadence, Core SDK + expansion SDKs, Windows HIP SDK folded in, repo.amd.com).
- Windows: PyTorch preview only; "entire ROCm stack not yet fully supported on Windows"; native installers "later in 2026". TheRock SUPPORTED_GPUS: RDNA1-4 release-ready; gfx1153/gfx950/CDNA2/1 only "build passing" ("may fail at runtime").
- Client: amd/gaia (752 open issues), lemonade (5.6k★, AMD-sponsored community), Ryzen AI SW 1.8 Windows-first.
- ROCm.ai agentic layer (GA w/ ROCm 10): ROCm CLI (tech preview), amd/skills (MIT, Apr 2026, 327★, skills for Claude Code/Cursor/Codex; federation only accepts AMD-org repos), AMD-AGI/Hyperloom (Instinct-only kernel tuning, Claude backend; consumer arch mapped to None), AMD-AGI/Apex (RL env for kernel agents), TraceLens, Magpie.
PEOPLE/DEALS
- Anush Elangovan CVP AI SW: runs devrel personally; SemiAnalysis Apr 2025 "skeleton crew", only 1 FT devrel, recommend 20+. Vamsi Boppana SVP AI. Sharon Zhou VP AI.
- Acquisitions: Nod.ai, Mipsology, Silo AI, Brium (Jun 2025 -> Triton/WAVE/IREE/MX FP4), Untether team, Enosemi, Taalas (Aug 2026).
- Deals: OpenAI 6GW (Oct 2025; Codex writes Triton/Gluon), Oracle 50k MI450, Meta 6GW (Feb 2026), ★ANTHROPIC 2GW (Jul 22 2026; up to $5B equity; "Claude to optimize workloads for Instinct" + "accelerate ROCm development"; AMD adopts Claude internally), Cerebras, HUMAIN, xAI, Cohere.
- Staffed: PyTorch (MI300 upstream CI), vLLM/SGLang/AITER, Triton, HF, IREE/MLIR. Medium: JAX, llama.cpp. Weak: Vulkan. NIXL ROCm merged Jun 4 2026.
ADMITTED GAPS (SemiAnalysis Jul 2026, undisputed)
- No stable internal GPU clusters for SW teams; MI455X vLLM/SGLang nightly CI missed -> Oct 2026; gfx1250 PyTorch ships w/ NO CI; K8s + Pollara NIC inference CI 0% parity; feature composition (disagg+FP4+WideEP+DP-attn+spec-decode) "broke the stack"; MoRI on 5-6 engineers; no Python kernel DSL (FlyDSL new, Instinct-only); RCCL behind NCCL; TorchInductor dynamic-shape 77% vs 90%+.
- Device wishlist poll (Jan 2025, 1000 votes): Strix/Halo 29%, RDNA2 27%.
CONTRIBUTION MECHANICS
- CONTRIBUTING thin; no CLA; "Maintainers appointed by AMD"; top-40 contributors all AMD/bots; 0 of 53 recently merged rocm-libraries PRs from outsiders; outsider RDNA4 PRs stuck since Mar 2026 (#5243 generic Tensile target, #5462 MIOpen CK gfx1201, #6365 hipBLASLt FP8 gfx120x). Hyperloom #1196: outsider RDNA4 llama.cpp runner -> "low priority", stale-bot. TheRock: 2 good-first-issue, 0 help-wanted. 273 open consumer issues on legacy tracker.
SEAMS (welcome/unstaffed): consumer-GPU Hyperloom/Apex runner; land stuck RDNA4 PRs + gfx120x CI evidence; Windows full stack; unshipped skills (ROCm Doctor, HIP replay, APU memory tuner, serving-llms-on-ryzen #203); K8s/Pollara CI; composition test grid; RCCL/MoRI tooling; consumer issue triage/repro service; docs + migration guides (7.x->10, HIP SDK->Core SDK); Python-first layers, JAX/llama.cpp/Vulkan backends.
