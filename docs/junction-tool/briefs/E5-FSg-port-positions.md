# Brief E5-FS-g: port lanes carry their positions (D410)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}`. Work and commit only there.
- Ruling D410: the last row of `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (main worktree).

## 2. Read first (once each)
- `CODINGSTYLE.md` (C++ part); D410.
- `src/junction/planar/planar_topology.h` (`PlanarPort`) and `planar_topology.cpp` (`ParsePort`, around lines 130-165).
- `src/junction/planar/planar_draw.cpp`, the port rows ("The box edge of a side and the order of the port lines", around lines 260-307).
- How `map.expanded.ports` is built from the topology's ports (`planar_regions.cpp`, `BuildPlanarTrackMap`): find where ports are copied and whether port lines are replaced by parts.
- `src/junction/planar/CLAUDE.md`, "Topology" and "Drawing program".
- `tools/junction/topologies/psg197.txt` (the `port` lines); the topology tests (`src/tests/junction/planar/planar_topology.cpp`, any port parse test); `src/tests/junction/planar/planar_draw.cpp`: `Shaped197`, "PSG 197 zero-length steps, faces and whole".

## 3. Tests (outside-in)
1. **Outer (hidden measurement) `planar draw: PSG 197 with V's port positions, faces and whole`, `[.][planarkeepapart]`.** Same as "PSG 197 zero-length steps, faces and whole" (faces alone with `tidy = false`, 15 s each, the loop stops at 15 minutes; whole at 60 s; elastic for every non-feasible face and for the whole hub if not feasible), on `Shaped197()` after the psg197.txt change below. Summary prefix `FSg 197 faces:`. Run once after the change and report.
2. **Inner, topology `[topology]` tag group as the existing parse tests use:** `planar topology: port positions`.
   - `port E out E @ 164 166 167: A1, A2, A3` parses with `at == {164, 166, 167}`.
   - A count mismatch (`@ 1 2` with three lines) is an error naming "positions".
   - A port without `@` has empty `at`.
   - Red: write the test first and record the parse error the current parser gives.
3. **Inner, drawing `[junction][planar][planardraw]`: `planar draw: port positions set the lane spacing`.** On `ShapeFirst(PlanarTText(1), 1)`: give every port on one side positions whose consecutive differences are 3 tiles (set `at` on the topology's ports before Structure, or on `v.structure.map.expanded.ports` directly if that is where the drawing program reads them; say which). With `{.clearance = false}` the drawing is feasible, and in `r.points` consecutive port lines on that side are exactly 6 half tiles apart along it. With positions out of reading order, the result is `unsupported` naming "port positions".
   - Red: record the failure before the change (spacing 2, or not unsupported).

## 4. Changes
1. `PlanarPort`: `std::vector<int> at; ///< Optional (D410): per line, in `lines` order, its position along the side in tiles (N and S: x, W and E: y); empty: uniform spacing (2 half tiles within a port, 2 x side_gap between ports).`
2. `ParsePort`: after the heading, an optional `@` followed by integers before the `:`. The count must equal the number of lines, else the error "a port's positions must give one per line". The words check accepts 3 words, or 3 words + `@` + integers.
3. The expansion keeps `at` aligned with `lines` in `map.expanded.ports`. If a port's lines are replaced or renumbered there, `at` follows the same order. If the expansion adds or removes port lines, stop and report.
4. Drawing program, port rows: for a side where every port has a non-empty `at`, the gap between consecutive lines in reading order is `2 * (at(line) - at(previous line))` half tiles, whatever port they belong to. A gap < 2 (out of reading order, or equal positions) returns `unsupported("port positions out of reading order on side X")`. If only some ports on a side have positions, return `unsupported("port positions on side X are partial")`. Without positions: as today.
5. `tools/junction/topologies/psg197.txt`, V's lane positions (from `hubs.tsv`, listed left to right across travel as the lines are):
   - `port N out N @ 84 85: NO1=..., NO2=...`
   - `port N in S @ 91 90: AB1, AB2`
   - `port E out E @ 164 166 167: EO1=..., EO2=..., EO3=...`
   - `port E in W @ 174 173 169: CD1, CD2, CD3`
   - `port S in N @ 81 82 83: EF1, EF2, EF3`
   - `port S out S @ 90 89 88: SO1=..., SO2=..., SO3=...`
   - Keep each line's route list exactly as it is.
   - If the drawing program reports these out of reading order, stop and report (the frame or the left-to-right order differs from this brief's reading).
6. `src/junction/planar/CLAUDE.md`:
   - "Topology": add the `@` positions to the port syntax sentence (D410).
   - "Drawing program", first bullet: add "with port positions (D410) consecutive port lines on a side are 2 x their position difference apart".

## 5. Do not
- Change `side_gap`, Structure, Shape, or any default behaviour without positions. Touch other topologies.

## 6. Stop conditions
- Expansion adds or removes port lines; 197's positions are reported out of order; any existing test fails; anything not covered.

## 7. Gates
- Debug `build` -j 4 (to a file); `tools/junction/jt fast build '[topology]'`, `'[planardraw]'`, `'[planarshape]'`. Release `build-rel` -j 4, target `openttd_test` only (a GUI client runs from `build-rel/openttd`: do not rebuild or kill it), `JT_JOBS=4 tools/junction/jt gate build-rel` (about 31 s, OVER BUDGET at 30 s known; `FAILED:` lines inside passing cases are not new). Also run the hidden 197 Shape test (`[planarpsg197]`) once to confirm Shape still shapes 197 with the edited topology (report its dummy line).
- Commit: `Change: Junction topology: port lanes carry their positions (D410)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- Then alone: `build-rel/openttd_test "planar draw: PSG 197 with V's port positions\, faces and whole" > /tmp/claude-1000/fsg-197.log 2>&1` (budget 20 minutes).

## 8. Report (short)
- References read; commit; red evidence; members added
- Suite results per gate (count, time)
- 197: the summary line; per non-feasible face its status and elastic stretched entries; the whole hub's status and, if infeasible, its elastic families and top tags
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
