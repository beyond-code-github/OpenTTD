# BRIEF H12b: Shape builds convex cells by rule (Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch `wt-cells`, head {HEAD} (H12a: `PlanarShapeDummy`, `PlanarShapeVariant::dummies`, `CheckPlanarShapeCells`, `Oracle::CheckH12`). Builds: Debug `build`, Release `build-rel` in that worktree (reuse them).
- Implements SPEC-shape §8.7's H12b rule (DECISIONS D385, D386). H13 (the realizability certificate) is NOT in this step. Real code only; never touch `planar_slots_probe.cpp`.
- Read once: /home/pete/OpenTTD/docs/junction-tool/wip/SPEC-shape.md §8.1 and §8.7; DECISIONS rows D385, D386 (grep `^| D385 ` / `^| D386 ` in /home/pete/OpenTTD/docs/junction-tool/DECISIONS.md); /home/pete/OpenTTD/CODINGSTYLE.md (C++); /home/pete/OpenTTD/src/junction/CLAUDE.md "Working rules for agents"; /home/pete/OpenTTD/src/tests/junction/CLAUDE.md; /home/pete/OpenTTD/.claude/ng1/src/junction/planar/CLAUDE.md (Shape section).
- Code to build on: `Oracle::CheckH12` in /home/pete/OpenTTD/.claude/ng1/src/junction/planar/planar_shape_layer.cpp (its piece-level walk, its split at a dummy, its convex check, H7's sign convention: inner faces total -8, a reflex corner has turn > 0).

## Changes (planar_shape_layer.h / .cpp)
1. **Shared helpers, file-local.** Move the piece-level face walk and the split-at-a-dummy logic out of `Oracle::CheckH12` into two functions in the .cpp's anonymous namespace, used by both the oracle and the builder: `CellWalls(...)` (a face's piece-level walk as the oracle builds it) and `SplitCell(...)` (one cell and one dummy -> two cells, or failure with the reason the oracle reports). Keep the oracle's messages and behaviour byte-identical (the H12a tests prove it). Name any local wall struct the oracle already has; do not add another type.
2. **The builder.** Public, declared in the header with a doc comment: `bool RefinePlanarShapeCells(PlanarShapeVariant &v, std::string &why);` It clears `v.dummies`, then for every inner face (the faces the oracle checks):
   - Cells start as the face's walk. Repeat until no cell has a corner with turn > 0: take the first such corner in the first such cell (cells in creation order, corners in walk order).
   - The corner must be a bend inside one track: the walls before and after it are consecutive pieces of the same track. Else return false with `why = "no cells: face <f> reflex corner at <wall> -> <wall> is not a bend"`.
   - Let h be the incoming wall's lifted walked heading. Candidates d, in this order: h, h - 3, h - 1, h - 2. For each d, the landing wall is the first wall after the corner in the cell's walk, skipping the outgoing wall, whose walked heading lifted relative to d gives c = heading - d in -3..-1 (use the same `Norm` lifting as the oracle). A candidate is valid when `SplitCell` succeeds and the corner's turn is <= 0 in both new cells.
   - Choose the first valid candidate whose two cells both have 4 or more walls; if none, the first valid candidate; if none is valid, return false with `why = "no cells: face <f> no dummy resolves <wall> -> <wall>"`.
   - Record the dummy: `face`; `track` and `piece` = the bend as the vertex between `piece` and `piece + 1` of that track (the lower piece index, whichever way the face walks it); `heading` = d mod 8 in 0..7; landing `land_track`/`land_piece` = the landing wall's track and piece, or `UINT32_MAX` and the `PlanarSide` value for a box edge. Replace the cell by its two cells.
   - Return true.
3. **Shape calls it.** At the end of `ShapePlanarStructureVariant`, after the variant is otherwise complete: `if (!RefinePlanarShapeCells(out, why)) return false;` (the variant is refused with that reason, as any other Shape failure).
4. **H12 becomes a promise.** Call the H12 check from `CheckPlanarShapePromise` (after H7), as the oracle's messages. Keep `CheckPlanarShapeCells` public (the H12 tests use it).
5. **CLAUDE.md** (/home/pete/OpenTTD/.claude/ng1/src/junction/planar/CLAUDE.md, Shape section), in the same commit: add a bullet after "It marks flex points": `  - It refines every inner face into convex cells by dummy edges from each reflex bend, by rule (heading order h, h-3, h-1, h-2; landing on the first facing wall; two cells of 4+ walls preferred), D385; the dummies are part of its output.`; and in Shape's "Promise:" bullet append `; every face's cells, split by its dummies, turn one way and wind once (H12)`.

## Tests (outside-in, planar_shape_layer.cpp tests)
1. **Outer:** the hidden test `"planar shape layer: H12 outer every variant's cells are convex on T2 T3 LL2"` becomes visible with tags `[junction][planar][planarshape][slow]` (like the existing `BuildPlanarShape` outer test). It must pass: every variant of T2, T3, LL2 shaped, no H12 message. Run it once in Release before the builder exists to record red (expected: T2 208, T3 340, LL2 120 messages, as H12a measured).
2. **Inner (fast `[planarshape]`):**
   - `"planar shape layer: H12b the rule refines the fray"`: `MakeFray()`, then `RefinePlanarShapeCells`: true, exactly one dummy, equal to {face 2, B1, piece 0, heading 2, UINT32_MAX, the E side} (the H12a hand dummy; SPEC-shape §8.7 derives it), and `CheckPlanarShapeCells` empty. Red first (the function does not exist: write it as a stub returning false, record the failure, then implement).
   - `"planar shape layer: H12b the rule refines T1"`: `MakeT1()`, `RefinePlanarShapeCells` true, `CheckPlanarShapeCells` empty. Report the dummies it chose for faces 3, 5, 6, 9 (track, piece, heading, landing).
   - The existing `"the oracle accepts the two valid hand variants"`: now that the promise includes H12, the hand variants need their dummies: call `RefinePlanarShapeCells` on each before the check (and in any other existing test that feeds a hand variant to `CheckPlanarShapePromise` expecting no message; list them). Tests that expect a specific Hn message from a hand variant must still get only that id: if one now also gets an H12 message, call `RefinePlanarShapeCells` first; if it then cannot refine, stop and report.
- Also report, for T2's first variant (o3t2f1m150): the dummies count, how many cells are triangles (3 walls), and for each triangle whether one of its walls is a beside track (a copy or prio row, `map.part_of[t] == UINT32_MAX`) or its parent at the shared run (the SPEC-shape §8.7 parity check). Print it from the outer test with `INFO`/`WARN` or a plain `std::cout` line tagged `H12B T2`.

## Do not
- Write H13 (no LP, no lengths); change H1-H11 or their messages; touch the probe; add types beyond the two named helpers.

## Stop conditions
- A reflex corner that is not a bend inside one track, or a corner no candidate resolves, on any T2/T3/LL2 variant: report the face's walk (the oracle's wall texts with turns) and stop after committing what is green (the outer test then stays red; say so).
- The H12a tests change behaviour after the helper move: stop and report.

## Gates
- Debug: build `build`, `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build '[planarshape]'`.
- Release: build `build-rel`, `/home/pete/OpenTTD/tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel`, then the outer test by name in Release (`timeout 300`, output to `build-rel/h12b-outer.txt`).
- Shape's time: report the `BuildPlanarShape` outer test's time before and after (it was the slow one: LL2 about 8.8 s); the rule adds no solve.
- One commit: `Add: Junction planar Shape H12b: convex cells by rule (SPEC-shape 8.7, D385, D386)`. One heavy job at a time. Catch test names must not contain commas.

## Time box
- About 2 hours. No rerun loops.

## Report (short)
- References read; commit; red evidence per new test; types/members/functions added; suites (fast count/time, gate count/time, outer result and time, BuildPlanarShape time before/after); T1 dummies; T2 first variant: dummies, triangles, triangles touching beside tracks; stops and undone; `git status --porcelain`, `git log -1`.
When you have reported, stop.
