# RS1: a free diagonal that cannot climb gets a ramp staircase (D196, D200, DESIGN-N3 §11)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-rs1 <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- HN1 (merged) gives a Height failure a `JunctionNodeHeightFault` (part, index, row, x, tiles; working tiles). Over the near-test ridge at offset (0,0) the fault is part Diagonal, index 2, tiles (44,9) (44,8) (43,9): path 4's free diagonal must cross ramp row 9, and only a lone straight Y piece can climb a row whose incline is along Y.
- Design (DESIGN-N3 §11, the "Shape change geometry" bullet; agreed with the engineer): a diagonal's two ends are fixed in the layout, so m straight Y steps up the ramp (along Y, not X) must be balanced by m straight X steps on other rows: diagonal -> straight X -> diagonal -> straight Y over the ramp rows -> diagonal (or Y first, then X). Both straights become free links between new curve points when the layout is traced, so the solve places them. The insert is a layout operation like InsertJunctionDetour: no facts; it only gives a valid starting shape.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md; DESIGN-N3.md §11; DECISIONS.md D196, D198, D199, D200.
- src/junction_route.h and the move grammar in src/junction_route.cpp (RasterizeJunctionRoute: what F, L, R do from an axis heading and from a diagonal heading).
- src/junction_nodes.h: JunctionNodeDiagonal, JunctionNodeHeightFault, InsertJunctionDetour's declaration.
- src/junction_nodes.cpp: NodeDiagonalRaster (~:712), InsertJunctionDetour (~:1534, whole: the pattern to follow), NodeState and SearchJunctionNodes (the failure branch at the end, ~:1725-1760).
- src/tests/junction_nodes.cpp: the InsertJunctionDetour tests (grep) and "a free diagonal over a ramp row is a Height failure"; src/tests/junction_game.cpp: the ridge test [tmapnear] and the [nodeoracle] test.

## Design (settled; implement exactly)
1. **Insert.** Declared beside InsertJunctionDetour:
   ```cpp
   /**
    * The layout with a free diagonal's interior replaced by a ramp staircase (DESIGN-N3 §11, RS1): the diagonal turns to a straight Y run that covers rows [row_lo, row_hi]
    * and to a straight X run of the same length on other rows, so the diagonal's ends stay where they are; nullopt (and why) when none fits.
    * @param diagonal Index into tpl.diagonals. Rows are in the layout's (and template's) frame.
    */
   std::optional<JunctionLayout> InsertJunctionRamp(const JunctionLayout &layout, const JunctionNodeTemplate &tpl, uint32_t diagonal, int row_lo, int row_hi, const JunctionSpec &spec, std::string *why);
   ```
   - p, q = the diagonal link's from and to points; the program starts at `p.tile, p.trackdir` (as NodeDiagonalRaster) and must end on `q.tile` with `q.trackdir`.
   - The turns: from p's diagonal heading, find by probing (as InsertJunctionDetour probes its S-bend hand) which of 'L'/'R' gives a Y-axis heading (call it ty) and which gives an X-axis heading (tx); the turn back to the diagonal heading is the opposite letter of each.
   - Two orders: Y-first `F^a ty F^m back(ty) F^c tx F^b back(tx) F^e` and X-first `F^a tx F^b back(tx) F^c ty F^m back(ty) F^e`. Try Y-first, then X-first. Within an order, the first (in this loop order) that passes: `m` from (row_hi - row_lo + 1) up to the diagonal's `halves`; `b` from 0 up to `halves`; `a` from 0 up to `halves`; `c` from 0 up to `halves`; `e` is then fixed by ending on q (compute it as InsertJunctionDetour computes n3: rasterise without the tail, then the remaining length; reject when negative or when the full raster does not end exactly on q with q.trackdir).
   - It passes when: the raster has no error; every row in [row_lo, row_hi] holds a piece of the Y run (pieces between the ty turn and its turn back); no piece of the X run lies on a row in [row_lo, row_hi]; and `build` succeeds as in InsertJunctionDetour (remove the diagonal's interior pieces, not p and q; no collision with other tracks; the normalised layout's template has no error). Removing the interior: the diagonal's flat raster `NodeDiagonalRaster(tpl, diag, diag.halves)` minus its first and last piece; remove exactly those tracks from their tiles.
   - Bounds: if no (order, m, b, a, c) passes, fail with the last reason ("no buildable staircase: ..."), as InsertJunctionDetour.
2. **Search.** In SearchJunctionNodes' failure branch: when `failure.kind == Kind::Height` and `failure.height` is set with `part == Diagonal`: the ramp rows are the rows of the fault's tiles whose lone Y straight climbs, i.e. `facts.Straight(t.x, t.y, Axis::Y, entry, exit)` is true and `entry != exit`; none -> log `"  ramp diagonal {}: no ramp row\n"` and no child. Else `row_lo/row_hi` = min/max of those rows minus `state.offset.y`; `InsertJunctionRamp(state.layout, tpl, index, row_lo, row_hi, spec, &why)`; on failure log `"  ramp diagonal {}: {}\n"`; on success normalise, compute the child offset, check `visited`, and push exactly as the detour children are pushed (`detours + 1`). Log `"  ramp diagonal {} rows {}..{}\n"` for a pushed child.
3. Nothing else: no change to the solve, the facts, the oracle, the detour, or the Height fault.

## Tests (outermost first; record each red)
1. Outer, `[tmapnear]`: the ridge test. Replace its WARN about (0,0) by `CHECK(near.offset.x == 0); CHECK(near.offset.y == 0);` and check its layout with JunctionNodeHeightsOk as the test already checks layouts (read it). Record the BENCH line. Red now: offset is not (0,0).
2. Inner, `[nodes]` `"a ramp staircase keeps the diagonal's ends"`: the test-spec T template (FlatT); for the diagonal at index 2 (REQUIRE it exists) choose rows row_lo = row_hi = the y of the middle piece of its flat raster; `InsertJunctionRamp`; REQUIRE a layout; its template (after NormaliseJunctionLayout) has no error and has the same number of paths and the same ports (FindJunctionPorts) as FlatT; and the layout holds, on the traced path that contained the diagonal, a run of lone Y pieces covering that row and a run of lone X pieces of the same length on other rows. Red: does not compile, then (stub returning nullopt) REQUIRE fails.
3. Inner, `[nodes]` `"a ramp staircase is refused without room"`: same template, rows far outside the diagonal's span (row_lo = row_hi = flat min y - 50): nullopt with a non-empty why.
4. `[nodes]`, `[nodespeed]`, `[tmapnear]`, `[facts]`, `[nodeoracle]` stay green; [nodeoracle] keeps heights=0 fits=0; report its accepted count before and after. If an existing expectation flips, STOP and report.

## Do not
- Do not change the solve, facts, oracle, detour, state cap, queue key, or HN1's fault. No terraform. No children for Group or Column faults.

## Stop conditions
- The template of a staircase layout has an error for every candidate (the tracer cannot make free links of the new runs): report the error text for the first three candidates and stop.
- Test 1 still red with a staircase child pushed: report the ridge search log at (0,0) (scratch print, removed after) and stop without committing.
- `[nodes]` Release over 1 s, `[nodeoracle]` Release over 5 s, the ridge BENCH line over 50 ms.

## Gates
`tools/junction/jt fast build "[nodes]" 5`, `tools/junction/jt fast build-rel "[nodes]" 1`, `tools/junction/jt fast build-rel "[nodespeed]" 5`, `tools/junction/jt fast build-rel "[tmapnear]" 5`, `tools/junction/jt gate build-rel`. Update TESTS.md times. Commit: `Feature: Junction node search gives a diagonal a ramp staircase`, body with the ridge result (offset, states, BENCH micros) and the ORACLE line before and after, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence per test; types/functions added; the staircase found for test 2 (order, m, b, a, c, e); ridge result; suites with times; ORACLE lines; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
