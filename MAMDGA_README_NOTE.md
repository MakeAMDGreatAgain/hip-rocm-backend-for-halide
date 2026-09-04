# MAMDGA note

This repository is the substantial backend/compiler porting work referenced as the “GPU backend for Photoshop/media software” effort.

The concrete target is **Halide native HIP/ROCm GPU backend**. Halide is the image-processing compiler used in production pipelines behind tools such as Photoshop, Pixel HDR+, and YouTube-scale media processing. The mission package contains a review-ready patch series, design notes, verification scripts, compile-only results, and an upstream handoff brief for Halide/AMD reviewers.

Important status: the port is **compile-verified and structurally validated without AMD hardware**, with GPU execution validation still pending. Nothing in this repo claims hardware correctness until the supplied `hardware-test.sh` / e2e scripts are run on a ROCm-capable AMD GPU.
