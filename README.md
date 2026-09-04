# HIP/ROCm Backend for Halide

<p align="center">
  <img src="docs/assets/mamdga-banner.webp" alt="MAMDGA banner" width="720"/>
</p>


> **MAMDGA context:** this is the new backend/compiler GPU porting repo: a native HIP/ROCm backend for Halide, the image-processing compiler used in production media pipelines including Photoshop-class workloads. The first mission packages patches, design docs, verification scripts, and an AMD/Halide upstream handoff brief.


An autonomous agent whose job is to take open-source software that "should not run on AMD GPUs" and make it run: pick a target that has **no working AMD path anywhere** (verified by a prior-art search), build the port with a team of coding agents, verify it as far as possible without hardware, and hand AMD a review-ready package (code, tests, CI, benchmarks, and a brief that states exactly what is and is not verified).

It exists because the research in `research/` shows AMD's ecosystem effort concentrates on Instinct datacenter parts while the long tail of developer tools, consumer GPUs, and "NVIDIA-only" projects is where developers actually get stuck.

## Missions

| # | Target | Why it is a first | State |
|---|---|---|---|
| 001 | **Halide native ROCm/HIP GPU backend** | Halide (the image-processing compiler behind Photoshop, Pixel HDR+, YouTube) has never had a native AMD backend. AMD's own engineer tried in 2018 (PR #2734), it stalled on code-object linking, and the remnants were deleted in 2024. | in progress — see `missions/001-halide-hip/STATUS.md` |

Rejected candidates and why (full evidence in `research/`): CTranslate2/faster-whisper (upstream HIP since v4.7.0, Feb 2026), ExLlamaV3 (four independent ROCm ports since Jun 2026), SageAttention RDNA4 / PyG / Mamba / gsplat / candle / Warp (AMD-employee or community ports already exist).

## Layout

```
README.md            this file
AGENT.md             operating model and guardrails
research/            research notes with sources (ecosystem gaps, community pain, AMD programs, prior art)
missions/001-halide-hip/
  BRIEF.md           handoff brief for AMD / Halide reviewers
  DESIGN.md          backend design grounded in Halide main (file/symbol level)
  STATUS.md          living checklist: verified vs. unverified
  patches/           git format-patch series against Halide main
  scripts/           wsl-setup.sh, compile-verify.sh, hardware-test.sh
  ci/                GitHub Actions workflow (compile-only, no GPU needed)
  results/           compile-verify and hardware results
```

The Halide checkout itself is a nested git repo (`missions/001-halide-hip/Halide`, ignored here; during development it lives in WSL at `~/halide-hip/Halide` for build speed). The patch series in `patches/` is the canonical artifact.

## Authorship

Code in this repository and the Halide patch series was written by an AI agent team (Claude Fable 5.1, orchestrated in Claude Code) under human direction. Halide's contributing policy permits AI-authored contributions with a `Co-authored-by` trailer; every Halide commit carries one. Nothing is claimed as verified unless a command was run and its output checked; see each mission's STATUS.md.
