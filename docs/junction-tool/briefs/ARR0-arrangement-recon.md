# ARR0: which arrangement facts the seeds obey (recon for REVIEW-joint-loop Q6 step 1)

Read-only recon. Commit nothing; throwaway test code only, reverted at the end. Keep your context small: send command output to files and report only what you found.

## Goal
REVIEW-joint-loop (docs/junction-tool/REVIEW-joint-loop.md, read the Verdict, F3, F4, Q6 and E2-E4) replaces the joint loop by an arrangement taken from the order sweep: static occupancy, hug and clearance rows over it, and one MILP. Before step 1 is briefed, the orchestrator must define the arrangement exactly. This recon measures, at real seeds, which candidate definitions hold. You measure and report; you do not choose.

The engineer ruled (D253) that the seed may take seconds; only the hover is time-critical.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-arr0 <HEAD given at launch>`. `build-rel` is configured with HiGHS; delete stale `.pch` files if the build complains.
- The reviewer's probes are in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/review/` (`probe2_test.cpp`, `dump_test.cpp`, `probe_test.cpp`, and their logs). Start from `probe2_test.cpp` (it evaluates rows at the seeds via `MakeLoopStart`, `RecordJunctionPlacementSystem` and `sys.solved`). Its helper `ReviewStaticOrder` lives in `probe_test.cpp`.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/arr0/`.

## Read first (offset/limit)
- `src/junction/topology/junction_topology.h` lines 120-240 (`JunctionTrackOrder`, row items, band events, stack items, `Step`);
- `src/junction/placement/junction_placement.h`: `JunctionPlacementSystem`, `JunctionSystemElement`, `JunctionSystemBend`;
- the reviewer's probe sources above.

## Seeds
T (L_R x L_R), LL_RR x L_R and L_R x LL_RR, at the seed `MakeLoopStart` finds (the same seeds as E4). Evaluate everything at each seed's solved points (rows `sys.solved.y`, columns `sys.solved.x`) and at its rendered tiles where a fact is about tiles.

## Measure (one table per item, per seed: held / broken, with each broken instance named by edge, token, row, column)
1. **Row occupants.** For each row r, `order.rows[r]` west to east. At the seed, take every chain piece whose tiles lie on row r's y. Does the west-to-east order of the pieces on that row (by column) match `order.rows[r]`, read with each crossing edge as one item? Report any piece on row r that belongs to an edge not in `order.rows[r]` (a bend tail or head overlapping the row: DESIGN-G1 §10.7 F6).
2. **Band events.** For each band b, `order.bands[b]`. At the seed, are the events' y in sweep order: (a) strictly, (b) non-strictly (equal allowed), (c) only within each layer? Report the weakest form that holds on all three seeds, and the broken instances of each stronger form.
3. **The piece under a row bridge.** For each Under token (crosser e under row edge x's doubled bridge copy c): which piece of e's chain occupies the column range strictly between copy c's heads on copy c's head row: the run before or after the Under token, the tail of the previous bend (its kind: Corner, SBend, Fan), or the head of the next bend? Tabulate the counts per piece kind, and list each instance on T.
4. **Clearance sides.** For each clearance relation the recording emits (grep `plan_clearance` / "clearance" reasons in `junction_placement.cpp`), is its side (west/east, north/south) the side implied by `order.rows` / `order.bands`? Count held / broken.
5. **Partial order check.** Combine 1-3: is there one consistent rule "a piece on row r is either a row occupant in `order.rows[r]` order, or a bend tail/head of an edge adjacent in that order"? Count instances outside it.

Also report, for LL_RR x LL_RR at its all-floor recording (no seed exists): the numbers of row occupants, band events and Under tokens, so step 1's sizes are known.

## Do not
- Change any committed code, or commit.
- Run the gate or sweep; run only your throwaway test with `timeout 600`.
- Touch any worktree other than ng1. No `git stash`, no `pkill` or broad `kill`, no game client, no background processes left.

## Report (short, about 40 lines)
- the five tables (summarised: held/broken counts per seed, plus every broken instance on T);
- for item 2, the weakest form that holds;
- for item 3, the piece-kind counts;
- for item 5, the count outside the rule and the instances;
- the LL_RR x LL_RR sizes;
- `git -C /home/pete/OpenTTD/.claude/ng1 status --porcelain` (must be clean) and the path of the kept probe source in the arr0 folder.

When you have reported, stop.
