# Probe: prio coverage over a doubled bridge (D347), T2/T3

Throwaway probe: nothing committed in ng1. Time box: 60 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. One heavy job at a time (no parallel test runs: the machine is unstable under load). Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/probe-pb/`; read code with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `0958786f36`, Release `build-rel`.
- Read first (short): `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-prio-stagger.md`, `wip/RECON-prio-bridge.md`, DECISIONS.md rows D108, D342, D345, D347; `src/junction/CLAUDE.md` "Rule checker" and `src/junction/planar/CLAUDE.md` "Component solver".

## Why
A multi-lane prio join chains its lanes' coverages: lane j's full 2P+2 straight starts only after joiner j-1's doubled bridge. D347 lets a coverage contain that doubled bridge (coop prio over a bridge), judged by G9b. Measure what that gains and whether it builds checker-clean.

## Changes (all behind env `PLANAR_PRIOBRIDGE=1`; unset = today exactly)
1. **Checker G9c** (`src/junction/junction_rules.cpp` ~1455-1475, `SwitchEventsOf` ~433): do not count a switch event that is the split into, or the rejoin of, doubled copies of one lane (G1 copies: two paths of one movement that differ only over a doubled stretch). Find how the checker already knows copies (G1, D24 rejoin, D322); use exactly that. If no existing marking lets you tell a copy split/rejoin from a real split, stop and report what exists. Every other switch still counts; clauses 1, 4, 5 and CheckPrioReach (G9b) unchanged.
2. **Composer, near lanes' coverage** (`src/junction/planar/planar_components.cpp`): in the 2-lane `Join` (~383, `M.AddSeg(S_near, h, 2 * this->P + 2)`) and the n-lane `Join` (~998, `M.AddSeg(S[j], h, 2 * this->P + 2)`), make that last segment `AddSeg(.., h, 2)` and add one row: the sum of the lengths of every segment of that strand (from its start node to the merge, as the Block227 R5 loop sums `sg.L` ~270) is at least `2 * this->P + 2`. Lane 0 (far lane), Block227 and the prio rows unchanged.
3. Nothing else. My own naive try (step 2 without the sum row) gave "no variant solves: o1t3f1s0: a clash is not separable". If you get that, rerun that variant with `PLANAR_ONLY=<tag> PLANAR_IIS=1 PLANAR_DEBUG=1` and report the clash pair (strands, segments) and the IIS rows; do not add any rule to fix it.

## Runs (sequential, via `/home/pete/OpenTTD/tools/junction/jt` where a tag fits, see `docs/junction-tool/TESTS.md`; else the binary with the test name)
- Baseline then probe, for T2 and T3 mapped (`planar components: T2/T3 from its topology text (mapped)...` tests: full variant sets) and for the end-to-end checker on the mapped plans (find the test that runs signals + `CheckJunctionRules` on mapped T2/T3 plans in TESTS.md / `src/tests/junction/planar/`; name it).
- Per hub: best variant and area/size before and after, every variant's area before/after, checker result (clean, or each violation text), signal pass outcome (prio placed by the straight pattern or by search, or "does not fit").
- Then T3 best variant only: `PLANAR_ONLY=<tag>` ASCII picture if any test or helper prints one (do not add one).

## Report (findings under 35 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-prio-bridge.md`, committed alone on branch feature/junction-tool in `/home/pete/OpenTTD`, message "Doc: Junction probe of prio over a doubled bridge", ending "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>")
Then in ng1: `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-prio-bridge.patch` (commit that file with the findings), `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
