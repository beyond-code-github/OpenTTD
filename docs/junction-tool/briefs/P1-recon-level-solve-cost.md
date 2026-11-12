# P1 (recon by experiment): why the level solve costs ~7 ms, and how to cut the level solves per hover (F1's stop)

Recon by experiment: you may change code in your worktree to measure, but commit nothing. Keep your context small: send command output to files and report only what matters. Never use git stash. Mark anything not measured or read as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2, branch wt2-f1, with F1's UNCOMMITTED edits (PlanJunctionHoverNear, the HOVERNEAR sweep pass; saved as /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/f1-wip.patch). Keep those edits; at the end `git -C /home/pete/OpenTTD/.claude/nodes2 diff` must equal that patch again (remove all your experiment code; save your experiments as a separate patch in the same scratchpad). Another agent works in /home/pete/OpenTTD/.claude/nodes: never touch it. Use absolute paths.
- F1 (briefs/F1-near-frame-search.md and its amendment): the near search (offsets +/-8 columns, +/-4 rows, nearest first) lifts real fixtures 19 -> 31 planned but takes up to 451 ms (limit 100). Per offset: placement 0.3 ms; render+signals 0.4 ms; SolveJunctionLevels (src/junction_levels.cpp:445) 6-8.5 ms whenever the placement succeeds, pass or fail, and it is 80-90% of the time. temperate-hilly-15 runs 51 level solves before it plans at offset (-8,-4); temperate-mountainous-6 runs 25 and never plans. A passing hover at the cursor (~9 ms) is also mostly the level solve.

## Read first (once each)
src/junction_levels.{h,cpp} whole (570 lines); docs/junction-tool/DESIGN-G1.md §15 (heights); docs/junction-tool/RECON-joint-levels.md conclusions; src/junction_terraform.h (PropagateJunctionTerraform).

## Experiments (Release, on the real fixtures with F1's search)
1. Profile SolveJunctionLevels on temperate-hilly-15's 51 calls: time per phase (greedy start, each coordinate-descent sweep, terraform propagation, cost evaluation), number of sweeps, calls to Evaluate and to PropagateJunctionTerraform per solve. Name the hot spot(s) with file:line.
2. For the failing level solves (all fixtures): the failure reason classes (from `why`), and at what point the failure becomes certain (e.g. a Blocked tile/corner the track must change, a corner outside the snapshot, a fixed-corner conflict found in the greedy start). For each class, is there a check that is SOUND (never rejects an offset that would plan) and cheap (say its cost), run before or early inside the solve? Prove soundness from the code, or show it holds on all 48 fixtures x all offsets (a pre-check rejection where the full solve plans is a counterexample: report it).
3. Speed-ups of the solve that keep its result identical (incremental cost evaluation, avoiding repeated propagation, caching per tile): try the one or two the profile points at; report the time and that every fixture x offset gives the identical plan (levels, terraform, cost).
4. With the best combination of 2 and 3: HOVERNEAR planned / ms_mean / ms_max, and the slowest 5 fixtures.

## Output
docs/junction-tool/RECON-level-solve-cost.md (untracked, ~60 lines) and the experiment patch path.

## Report
Conclusions in 10 lines: the hot spot, the sound early rejects (with their reject rate and cost), the identical-result speed-ups (with the speed-up), HOVERNEAR numbers with them, and what remains over 100 ms. `git -C /home/pete/OpenTTD/.claude/nodes2 diff | diff - <f1-wip.patch>` (empty). When you have reported, stop.
