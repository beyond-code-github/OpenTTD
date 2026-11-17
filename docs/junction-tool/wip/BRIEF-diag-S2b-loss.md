# Diagnosis: why S2b loses T2's 862.25 and T3's 1933.25

Diagnosis only: commit nothing. Time box 40 minutes. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`. ONE heavy job at a time (the machine has a known CPU instability). Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/S2bloss/`.

## State
Worktree `/home/pete/OpenTTD/.claude/ng1` at d632dc357c with S2b UNCOMMITTED in the tree (= `/home/pete/OpenTTD/docs/junction-tool/wip/S2b-wip.patch`). Read `wip/BRIEF-S2b-shape-rules.md`, `planar_shape.{h,cpp}` (as in the tree), `planar_regions` `CrossingFrom`. Leave the tree exactly as found at the end (`git diff` byte-identical to the patch).

## Facts
Default mode (score default + program shape), before -> after S2b: T2 best 862.25 (o1t3f1s0) -> 906.25; o1t3f1s0 862.25 -> 918.25, o1t2f1s0 866.25 -> 921.25. T3 1933.25 (o1t3f1s0) -> 1941.25, one variant newly failing.

## Questions (single variants via `PLANAR_ONLY=<tag> PLANAR_SHAPE=program` on the T2 / T3 mapped test; seconds each)
1. T2 o1t3f1s0 and o1t2f1s0: print the program shape before S2b (`git apply -R` the patch, build) and after (re-apply, build): per channel heading lists, and per channel crossing: under, over, H_under - H_over mod 8 and CrossingFrom's s. Which crossings change, and is each change a side change (sign) or an angle change (|a| 1/3 <-> 2)?
2. Separate the two S2b changes: build the tree with `SHAPE_CROSSING_ANGLE_COST = 0` (temporary edit, revert after): area of T2 o1t3f1s0, o1t2f1s0 and T3 o1t3f1s0 with the side rule only.
3. If the 862.25 shape breaks a crossing side by CrossingFrom yet solved legally (it passed the lazy loop's topology sides): say which crossing, and check the layout: does the under line really pass the over from the side opposite to CrossingFrom's answer? (Compute from the solved coordinates.) This would mean either CrossingFrom is wrong for that crossing or the side is not fixed by the topology there; say which, with evidence.
4. T3: which variant newly fails and why (reason; IIS first 20 lines if base infeasible).

## Report
Findings under 30 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/DIAG-S2b-loss.md`; report under 12 lines; confirm `git diff` equals the patch. When you have reported, stop.
