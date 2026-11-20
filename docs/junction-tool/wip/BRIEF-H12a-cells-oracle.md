# BRIEF H12a: the convex-cell oracle in real Shape (Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch `wt-cells` (made from wt-slots), head {HEAD}. Builds: Debug `/home/pete/OpenTTD/.claude/ng1/build`, Release `/home/pete/OpenTTD/.claude/ng1/build-rel` (reuse them; never make new build dirs).
- Implements SPEC-shape §8.1 (H12, convex cells) as an ORACLE ONLY: the hand-off type and the check. Shape does not build cells yet (that is H12b). Ruling: DECISIONS D385. This is real code, not the probe: never read or call anything in `planar_slots_probe.cpp`.
- Read once: /home/pete/OpenTTD/docs/junction-tool/wip/SPEC-shape.md §2.1 ("Faces"), §4 (H7), §8; /home/pete/OpenTTD/docs/junction-tool/DECISIONS.md row D385 (grep `^| D385 `); /home/pete/OpenTTD/CODINGSTYLE.md (C++ part); /home/pete/OpenTTD/src/junction/CLAUDE.md "Working rules for agents"; /home/pete/OpenTTD/src/tests/junction/CLAUDE.md.
- Code to match: `CheckPlanarShapePromise` and its H7 check in /home/pete/OpenTTD/.claude/ng1/src/junction/planar/planar_shape_layer.cpp (search `H7: face`), `WalkFace`, `FaceWalk`, `FaceTerm`, `WalkedStretch` in the same file; the tests in /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_shape_layer.cpp (`MakeFray`, `MakeT1`, `RequireAllOf`, `SetTracks`).

## The type (planar_shape_layer.h)
Add, above `PlanarShapeVariant`, exactly:
```cpp
/** A dummy edge that refines a face into convex cells (SPEC-shape §8.1, D385, H12). */
struct PlanarShapeDummy {
	uint32_t face; ///< The face it lies in (index into the map's faces).
	uint32_t track; ///< The track whose bend it starts at.
	uint32_t piece; ///< The bend: the vertex between this piece and the next of the track.
	int heading; ///< Its heading, 0..7.
	uint32_t land_track; ///< The track of the wall it ends on, or UINT32_MAX for a box edge.
	uint32_t land_piece; ///< The piece of that track it ends on, or the PlanarSide of the box edge.
};
```
and in `PlanarShapeVariant`, after `structures`: `std::vector<PlanarShapeDummy> dummies; ///< The dummy edges that make every face's cells convex (H12).`

## The check (H12, in CheckPlanarShapePromise, after H7)
For every inner face (the faces H7 checks as inner, same skip rules):
1. **Piece-level walk.** From `WalkFace`, expand each walked stretch into its pieces in walk order (a stretch walked backwards gives its pieces reversed, each heading + 4), and each box term between two port ends into the box-edge walls it passes along (heading per side: W edge 6, N edge 0, E edge 2, S edge 4; as walked along the box with the face on the same side as the H7 walk). Lift headings so each corner turn is in -3..4 (cusp corners are 4 as in H7) and use H7's corner turns where H7 has them (node and crossing corners): the turn total of the walk must equal H7's total for the face; if it does not, add the message `H12: face <f> piece walk total <x> differs from H7's <y>` and skip the face.
2. **Split by dummies.** Apply the face's dummies one at a time; each splits one current cell into two at its start vertex (the bend after `piece` of `track`, which must be a corner of the cell) and its landing wall (which must be a wall of the same cell): the cell's walk from the start vertex to the landing wall (that wall's first part), then the dummy reversed back to the start vertex; and the other side. Corner turns at the new corners from the lifted headings (the dummy's heading, reversed as `heading + 4` on the way back). Messages: `H12: face <f> dummy at <track name> piece <p> does not start at a corner of its cell`, `... lands on a wall not in its cell`.
3. **Convex.** Each cell: every corner turn has the sign of the cell's total (zero allowed) and the total equals the face's sign times 8. Else `H12: face <f> cell <c> turns both ways at <wall text>` naming the first offending corner's two walls (track name and piece, or `box <side>`).
- No other check (no lengths, no landing geometry, no zero-length rule: SPEC-shape §8.3 (c) needs bend pairs, which do not exist yet).

## Tests (outside-in; planar_shape_layer.cpp tests file)
1. **Outer, hidden until H12b:** `"planar shape layer: H12 outer every variant's cells are convex on T2 T3 LL2"`, tags `[junction][planar][planarshape][.h12]`. For T2, T3 and LL2 (as the existing T2 test `planar parent chain` builds them, keep 16), every variant: `CheckPlanarShapePromise` has no `H12` message. It is RED now (Shape builds no dummies). Run it once; record the count of H12 messages per hub and, for T2's first variant (tag o3t2f1m150), the list of faces flagged. Compare that list with the probe's evidence: faces 1, 2, 4, 10, 11, 12, 13, 20, 22, 24, 27, 33, 34 (wip/PROOF-T2-cells.md); report any difference face by face (this is the check of the probe against the real code).
2. **Inner, in `[planarshape]` (fast tag):**
   - `"planar shape layer: H12 a convex face passes"`: the existing valid hand variants (`MakeT1()`, `MakeFray()`) give no H12 message. If one of them DOES have a non-convex face, stop and report that face's piece-level walk with turns (the orchestrator gives its dummy; do not invent one).
   - `"planar shape layer: H12 a reflex bend without a dummy"`: a hand variant with a face that has a reflex bend (build it from `MakeFray` by changing one track's pieces so a bend turns away from a face, keeping H1-H7 clean; if no such change keeps H1-H7 clean, stop and report) gives exactly the `turns both ways` message for that face. Red first: before the check exists this test fails on `REQUIRE(!messages.empty())` style assertions; record the failure text.
   - `"planar shape layer: H12 a dummy that lands on another cell's wall"`: the same variant with one dummy whose landing wall is not in the cell: the `lands on a wall not in its cell` message.
   - A test that a correct dummy makes that face pass: only if the orchestrator has given the dummy; otherwise list it under "left undone" (it needs a chosen landing wall: a design choice).
- The existing test `"the oracle accepts the two valid hand variants"` must stay green; the gate must stay green (the outer test is hidden).

## Do not
- Build dummies in `ShapePlanarStructureVariant` or anywhere in Shape (H12b); change H1-H11; touch the probe; update any CLAUDE.md (H12 is not yet a promise Shape keeps: it becomes one in H12b).

## Gates
- `cmake --build /home/pete/OpenTTD/.claude/ng1/build --target openttd_test -j8 > /home/pete/OpenTTD/.claude/ng1/build/h12a-build.txt 2>&1` (grep `error`), then `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build '[planarshape]'`.
- Release: build `build-rel` the same way, then `/home/pete/OpenTTD/tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel`.
- The outer test once in Release: `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 300 ./openttd_test "planar shape layer: H12 outer every variant's cells are convex on T2 T3 LL2" > h12a-outer.txt 2>&1` (expect failures; extract the H12 messages per hub with grep, report counts and T2's first-variant faces).
- One commit: `Add: Junction planar Shape H12 oracle: convex cells (SPEC-shape 8, D385)`. One heavy job at a time. Catch test names must not contain commas.

## Time box
- About 1.5 hours. No rerun loops: if a test fails for a reason you cannot name after two looks, stop and report.

## Report (short)
- References read; commit; red evidence per new test (failure text before the code); types and members added; suite results (fast tag count and time, gate count and time); the outer test's H12 counts per hub and T2 first-variant face list vs the probe list; stops and anything left undone; `git status --porcelain`, `git log -1`.
When you have reported, stop.
