# ST2: the prio element (D211 exception; DESIGN-G1 §12.4; RECON-prio-element)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-st2 <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- ST1 (merged) places signal tokens on the rendered routes: SignalJunctionTrackRoutes (junction_topology.cpp), phases anchors, bays, fill. The T is clean under the checker except its 4 Prio violations and the footprint (row 0 empty: it is the E prio's row). This step adds the prio element: per ML merge with a joiner of another movement, one element that draws its own joins, row, row signals and the ML signals inside its coverage, and makes the joiner's last signal an Entry. Plus one placement floor the recon found short.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` row).
- docs/junction-tool/RECON-prio-element.md (whole, 60 lines): the conditions (§1), the fixed pattern (§3) and the room (§4). This brief implements §3.
- docs/junction-tool/DESIGN-G1.md §12 (end of file).
- src/junction_topology.h :250-end; src/junction_topology.cpp SignalJunctionTrackRoutes (from its definition to its end) and placement :1255-1290 (the two `prio + 1` floors).
- src/junction_signal_pass.cpp :217-245 (TrackTouches, Corner, AddTracks) and :277-304 (GeometryOfPrio: how the side, h1 and h2 are chosen).
- src/tests/junction_topology.cpp: the T placement test ("the T's placement") and the ST1 tests (SignalledOf to the end).

## Design (settled)
### Placement floor (junction_topology.cpp :1271 and :1287)
Let `even_prio = prio + (prio & 1)` (n in the recon: p rounded up to even), declared next to `prio` (:973) with the comment `/* The prio element's coverage in tiles: joins at even offsets 0..even_prio upstream of the joiner merge (DESIGN-G1 §12.4, RECON-prio-element §3). */`.
- Undoubled row: `along(heading, njm, nex, even_prio + 1, false);` (the exit split stays outside the element; same as today for even p, D148a's +1 for odd p).
- Doubled row: `along(heading, njm, rp.lane_merge, even_prio + 2, false);` (the row's far end, step n + 1, must not meet the lane merge's S-bend: RECON §3-4).
Each with a one-line comment citing the element. Nothing else in placement changes.

### Reason
Add `Prio` at the end of JunctionSignalReason, commented `///< Inside a prio element: its ML signals and its row signals (D211: the prio places its own).`

### Phase 0, the elements (in SignalJunctionTrackRoutes, after `used` is built and before phase 1)
For each route r whose path is a through movement (as phase 2 decides it), for each merge event (from SwitchEventsOf, as phase 1 computes it) at piece j with tile M:
1. Joiner: the first route r2 (in route order) whose path's movement differs from route r's path's movement and which has a non-bridge piece at index k > 0 on tile M. None: no element. If M already holds an unused-by-routes track (a track not in `used` at M), another route made this element: skip.
2. Geometry exactly as GeometryOfPrio, with the merge piece P[j] and the joiner's previous piece r2.pieces[k - 1]: P[j] must be an axis (diagonal) trackdir; side edge = the edge not touched by the axis on the side opposite the joiner's approach; dir = TileIndexDiffCByDiagDir(exit edge of P[j].trackdir); side_diff; h1 = Corner(side, exit), h2 = Corner(reverse side, reverse exit). Write file-local `TrackTouches`/`Corner`/`AddTracks` equivalents in junction_topology.cpp (named JunctionTrackTouches, JunctionCornerTrack, AddJunctionTracks, static) since the signal pass's are file-local; do not change the signal pass.
3. Offsets: `ml(s) = M - s * dir`, `row(s) = ml(s) + side_diff`. Requirements, all checked before anything is written: for s = 1..n the route's piece j - s exists, is not on a bridge, is on tile ml(s) with P[j]'s trackdir, and is not a switch index of route r; for s = 0..n row(s) holds no track; row(n + 1) holds no track touching the edge toward row(n). If any fails: no element (the checker then reports the missing prio).
4. Write: h1 on ml(s) for even s in 0..n; row tracks: row(0) {h2}, row(s) {axis, h2} for even s in 2..n, {axis} for odd s; ML signals at ml(s) for odd s in 1..n-1, facing P[j].trackdir, spec.signal_type; row signals at row(s) for odd s in 1..n-1, facing ReverseTrackdir(P[j].trackdir), SignalType::Exit at s = n - 1, Combo otherwise; all SignalVariant::Electric. One Prio token per signal (route r for the ML and row signals). Remember (r2, k) for phase 4.
Phases 1-3 then run unchanged: `used` was built before phase 0, so the prio tracks never make switch events; join tiles hold overlapping tracks, so no anchor or fill signal lands on them; the element's ML signals are existing signals the later phases respect.

### Phase 4, Entry (after phase 3)
For each remembered (r2, k): the largest i < k with IsSignalledPiece(layout, r2.pieces[i]) and no switch index of r2 in (i, k); set that layout signal's type to SignalType::Entry. None: nothing (the checker reports it).

## Tests (tag `[topology]`; record each red)
1. **Outer, edit ST1's outer test:** restore CheckFootprint, and add CheckPrio and CheckPrioReach, so the test asserts `CheckJunctionRules(layout, spec, JunctionTTopology())` is empty (INFO each violation). Remove the "footprint waits for the prio element" comment there. Red before the code: the 4 Prio violations (record them).
2. **The pattern on the T:** for each element of the T (two: the W and E joiner merges, placement.nodes of the merge nodes), the joins, row tracks, ML signals and row signal types are as phase 0 says (check from the layout: h1 present on ml(even s), row(0) == {h2}, row signal at row(n - 1) is Exit, at row(1) Combo). Red: no prio track.
3. **The T placement test:** the W floor grows by one tile. Update the T placement test's expected numbers to the new solve, and report every changed number old -> new. If any y value, item row or size.y changes, STOP. The flat-T template oracle ("the T render traces like the flat T") must stay green unedited (the render is now the trace's length on that run, DESIGN-G1 §11.10).
4. **LL_RR (hidden `[.][diag]`, not committed):** write CheckJunctionRules(layout, spec, JunctionLLRRSidelineTopology()) and RenderJunctionAscii to docs/junction-tool/wip/st2-llrr.txt.
- Every other existing test stays green and unedited (ST1's tests 2 and 3 count Prio tokens as signals: if they need the new reason accepted, that is the only allowed edit there). If another fails, STOP.

## Do not
- Do not change the old signal pass, the checker, programs, or any placement rule other than the two floors.
- Do not move a token of phases 1-3 or remove a signal; do not add a fill rule for the row.
- Do not handle a prio on a bridge, a curved coverage or a merge whose coverage holds a switch: no element there, the checker reports it.

## Stop conditions
- Test 1 not clean on the T: report each violation, RenderJunctionAscii to a file you name, and the element's offsets.
- Test 3's y/size.y condition; any other existing test fails.
- Anything not covered here.

## Gates
- `tools/junction/jt fast .claude/nodes/build '[topology]' 5`; then `tools/junction/jt gate .claude/nodes/build-rel` green (386 before; report the count).
- One commit: `Add: Junction prio element on the seed render (D211, DESIGN-G1 §12.4)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "ST2" and the new test count.

## Report
The standard report, plus: the T placement number changes, the T and LL_RR element counts, and LL_RR's violation counts per rule from the findings file. When you have reported, stop.
