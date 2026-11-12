# RW4b: row_pts removed; every chain point from new_pt in chain order (DESIGN-G1 §16.4 RW4, second part; RECON-per-row-code.md R4, R9, R11, R12)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-rw4b <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes (a read-only-ish recon): never touch it. Use absolute paths.
- RW4a merged (44ec164987): one token-driven chain builder for every row edge, still taking its points from the per-row block `row_pts` (fif/fil/fof/fol, lane_split, sb1_last, sb0_first, lane_merge, heads) so point ids stayed. Remaining per-row readers: R9 doubled-leg bridge lines by row (~:2092-2105 at 7f5bc41e06), R11 the doubled-leg block (row items pinned, lane split/head/merge runs, the D118 stagger, the prio floor on njm), R12 floor_kind by row role and Tile position. GT1 (98a0a62d69) moved the identity sweeps to `[identity]` (`tools/junction/jt identity build-rel`).
- The review's risk (RECON §4 step 5): point ids change; seeds stay identical only if the D215 objective, the ties and every tie-break key on (edge, token, copy), not on point ids.

## Read first (once each)
CODINGSTYLE.md (C++ part); RECON-per-row-code.md §1, §4; RW4a's commit (`git -C /home/pete/OpenTTD/.claude/nodes2 show 44ec164987`); src/junction_topology.cpp: row_pts and every reader, R9, R11, R12, the D215 objective and the tie/order code.

## Step 0 (findings, no code change)
List every place a point id's numeric value (not just identity) affects a result: iteration order over points, tie-breaks by id, sorting by id, the objective's order, map keys. For each, whether it would change if ids were allocated in chain order instead of row_pts order. If any would change a seed, say how to key it on (edge, token, copy) instead (one line each); if that needs a choice, stop and report.

## Changes (after step 0)
1. The chain builder takes every point from `new_pt()` in chain order; `row_pts` is removed. R9, R11 and R12 read the chain elements (by token role tag: LaneSplit/LaneMerge S-bends, HeadsPair, Tiles at nodes) instead of the row block. The D118 stagger and the prio floor on njm stay as they are, addressed through the chain elements.
2. Any id-order dependency step 0 found is keyed on (edge, token, copy).
3. Comments move with the code.

## Tests
1. **Outer (identity):** `tools/junction/jt identity build-rel` and build-rel outputs of `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` equal to launch HEAD's (times and point ids in diagnostic text stripped: say what you stripped); the LL_RR circuit byte-identical (`[.circuitllrr]` with JUNCTION_CIRCUIT, cmp); `[topology]`/`[topoproj]` unchanged except tests that assert point ids (list them; each such assertion is rewritten to name the element, not the id, with no change in what it checks).
2. A temporary assert comparing the old and new placements' solved tiles per element (not per id) on every sweep placement; report it held, then remove it with the old code.

## Stop conditions
- A seed moves; a test checks a point id in a way that cannot be rewritten to an element without changing what it checks; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Codechange: Junction chain points allocated in chain order; per-row point blocks removed`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus step 0's list, the tests rewritten, and the identity result. When you have reported, stop.
