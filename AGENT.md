# AGENT.md — operating model and guardrails

Derived from the precedent research in `research/` (CodeMender, KernelFalcon, OpenHands resolver; the curl / LLVM / Linux / GitHub-cap backlash against AI PR volume; Halide's own contributing policy).

## What the agent does
1. **Pick a first.** A target qualifies only if a prior-art search (forks, PRs, issues, wheels, blogs, Discord/Reddit claims) finds no working AMD path. The search and its links go into the mission BRIEF so reviewers can check the claim.
2. **Build with a team.** An orchestrator decomposes the port into file-owned tasks and runs sub-agents in parallel; a separate reviewer agent audits the result against the reference (CUDA) implementation before anything is called done.
3. **Verify without hardware first.** Compile for every target architecture, validate produced artifacts structurally (ELF headers, kernel metadata, disassembly), run every test that does not need a device, and stage a hardware script that a reviewer can run unmodified.
4. **Hand off honestly.** The BRIEF separates "verified (command + output)" from "unverified (needs hardware)". Losses, regressions, and dead ends are reported, not hidden.

## Guardrails
- **Disclosure.** All code is labeled AI-authored. Halide commits carry `Co-authored-by: Claude Fable 5.1 <noreply@anthropic.com>`. Any upstream PR body states the tooling used.
- **Human sponsor.** No upstream PR, issue, or public post is created without the human owner's explicit go. The agent prepares; the human submits.
- **No volume.** At most one draft PR per upstream repo at a time; never touch "good first issue" queues; never cold-submit to projects whose policy bans AI code (Gentoo, NetBSD, QEMU, GCC, LLVM-proper).
- **Small, validated diffs.** New backends are isolated in new files behind a target feature; existing behavior for other targets must be byte-identical (regression suite run before/after).
- **No secrets, no accounts.** The agent never creates accounts or handles credentials; cloud access is set up by the human.
- **Provenance of numbers.** Every performance or correctness number in a BRIEF links to the script and raw output that produced it.

## Loop
Each tick: read STATUS.md → dispatch ≤4 independent tasks → review diffs → build + verify → update STATUS.md and patches → schedule next tick. Quiet ticks are recorded as quiet.
