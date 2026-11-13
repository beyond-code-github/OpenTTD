# Design pass: DESIGN-G4, the static placement model over the arrangement (2026-11-14)

Run as `junction-reviewer` with model opus and effort xhigh (engineer approved, 2026-11-14). This is a design task, read-only on code: commit nothing and change no committed code. Throwaway probes in the free worktree are allowed and reverted. Write the design to `docs/junction-tool/DESIGN-G4-static.md` (untracked), at most about 400 lines: a one-page summary first, then the detail.

## Where we are
- `docs/junction-tool/REVIEW-joint-loop.md` (your earlier review; read it all): keep the joint MILP, drop the loop; arrangement first, one static model (Q6).
- Decisions since (grep `docs/junction-tool/DECISIONS.md` for D253-D256, the last rows):
  - D253: the seed may take seconds (background, cached); the hover stays well under 100 ms.
  - D254: the direction is approved.
  - D255: the arrangement's definition.
  - D256: band edges pass under a single bridge **square**: Corner(hand), Under/Above, Corner(-hand). The condition is that the Corners obey every curve rule (JunctionCurveGaps, CL from the train, no 90-degree bend), with the run between them as long as those rules need.
- ARR1 (merged): `JunctionTrackArrangement` / `ArrangeJunctionTrackGraph` in `src/junction/topology/junction_topology.{h,cpp}` hold the row pairs, band pairs and unders. All facts hold at the T, LL_RR x L_R and L_R x LL_RR seeds.
- Recons (reports below; probes and outputs in the scratchpad):
  - **ARR0** (`/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/arr0/`):
    - Rows follow `order.rows`, with 0 reversed.
    - Band touching-layer pairs: 0 broken.
    - The piece under a row bridge: run before 18, run after 0, previous bend tail 4, next bend head 10.
    - Clearance sides: 72 of 72 implied.
    - Off-order pieces on a row are only band edges at the row's nodes (B) and port stubs of another row (A).
  - **ARR2** (`.../scratchpad/arr2/`, `arr2_out.txt`, `arr2_probe.patch`):
    - The sweep's working line `items` per band state, west to east. Adjacent pairs are never reversed at the 3 seeds. There are some equal-column overlaps, mostly sibling edges at a node.
    - Per-token slices break (4, 8, 8), because S-bend crossings sit inside bends and Under tokens have no tiles. D256 is expected to fix this.
    - 1, 9 and 14 adjacent pairs share no tile row in their state's range.
    - LL_RR x LL_RR: 34 line states, 69 distinct adjacent pairs.
  - **F5 probe:** placement refuses Corner/Under/Corner with "no crossed sbend" (jp:1833).

## The hard question this design must answer
Occupancy is a per-tile-row separation. Today `add_occupancy` (jp:1216-1424) builds the occupants per actual tile row y at a point, and separates neighbours in the flat x order (tile forms affine in L). With the order fixed by the arrangement, the order is no longer the problem. **Which tiles share a row** is: the rows are variables of the joint model.

Options to weigh (add others):
- (a) separate whole piece extents (over-strict for staggered parallel diagonals, such as adjacent lanes S-bending side by side);
- (b) one binary per adjacent pair and slice: x-separated, or y-disjoint in the direction the order implies;
- (c) group parallel same-shape bends (equal L, fixed pitch), which the programs may already imply;
- (d) the tile row as an affine function of the point's y and L, with per-tile-row pairs where both pieces' y's are tied (same row item or same band state).

For each option, measure on the seeds where you can: does it cut off the seed (the seed must stay feasible, or each re-pin must be listed as a rule change), how many binaries it needs on LL_RR x LL_RR and LLL_RRR x LLL_RRR, and the solve time measured or estimated (D253: seconds are fine).

## The design must give
1. **Inputs:** graph, order, structure, programs and arrangement. State what the arrangement must still gain, for example the band line states (ARR2), and exactly how they are emitted.
2. **Every family** of today's `build()` (jp:1508 onward), detect, detect_y and plan_clearance, mapped to a static row form over the arrangement: kept as is, rewritten, or deleted. This includes occupancy (the hard question), the hug (per under-piece, with its binary), clearance (sides from the order), port fans (R3b), the port offset (D252), series bridges (G2c-1), the prio element footprint, the lane split as a floor (F8, patch `docs/junction-tool/wip/f8-lane-split-floor.patch`), and the curve gaps.
3. **The square crossing (D256):** the programs change, and the placement family "bridge over a horizontal run" (the bridge's column strictly inside the run, the run's row strictly between the heads, the curve rules on both Corners). It must also cover what retires: G3.4a-2's t, D248's before/after, and the over-S-bend rows.
4. **Variables:** x and y continuous with box bounds, L integer, O integer, and the binaries with their counts per hub. Give the tight M for each conditional row.
5. **The hover:** it stays a fixed-length difference system. Name what it reads, and confirm that it gets no detection.
6. **What is deleted:** the loop, the at-mode, the kept clashes, the order binaries, the flat-solve detection, the D250 fallback and the rounds. Give the order of deletion.
7. **Sub-steps for briefs, outside-in, each with its red test:** at least 2a (square crossing) and 2b (static model, loop deleted), split further if a step would exceed one agent's scope. Each sub-step names its identity and re-pin expectations: T 27300 and LL_RR x L_R 86350 today, and D235 allows re-pins case by case with each change listed.
8. **Decisions for the engineer:** only those that are genuinely his, as decision cards (context, options, recommendation, reversible?). Everything else is your recommendation.

## Read
- `REVIEW-joint-loop.md`; `DESIGN-G3-vectors.md` (§G3.2, §G3.6); `DECISIONS.md` D213, D215, D216, D217, D235, D236, D240, D248-D256;
- `src/junction/CLAUDE.md`, `src/junction/topology/CLAUDE.md`, `src/junction/placement/CLAUDE.md`;
- code: `junction_placement.cpp` (build, add_occupancy, hug_heads, add_clearance_x, detect_y, plan_clearance, the over-S-bend block ~1812-1860, record_system); `junction_joint.cpp`; `junction_topology.cpp` (the sweep ~555-700, the band programs ~1150-1250, ArrangeJunctionTrackGraph); `junction_pipeline.cpp` (the cut search, MakeJunctionGraphSeed).

## Worktree
You may build and run probes in `/home/pete/OpenTTD/.claude/ng1`: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-g4 <HEAD given at launch>`. `build` and `build-rel` exist (HiGHS configured); delete stale `.pch` files if the build complains. Throwaway only, reverted at the end. Outputs go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g4/`.

Never touch other worktrees. Never use any form of git stash, pkill or a broad kill. Never start a game client. Use `timeout` on every run. Leave no background processes.

## Output
`docs/junction-tool/DESIGN-G4-static.md` in the main checkout. Report back a 15-line summary, the decision cards' titles and the path. When you have reported, stop.
