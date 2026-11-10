# E1: A's main line is an edge of the map, with port approaches checked first (D178, D179)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
D178: every part of the hub is projected by the same `route_line`; per-part checks are deleted, not extended. D179: before any routing, every port's approach must be usable, and a failure is reported at once so the tool can say "move the cursor" in its status line (never a modal dialog). Today `ConstructJunctionTMap` lays A's main line (row 0) as a fixed straight track across all 64 frame columns and checks every tile of it, so a house anywhere on row 0 gives no layout. This step routes A between its two ports and deletes row 0 from the fixed-part check. B's lanes (W row) and the stubs stay fixed until E2 and E3.

## State
- Worktree `/home/pete/OpenTTD/.claude/e1`, branch `wt-e1`, head given at launch (feature/junction-tool with G4 and G5 merged). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only.
- Read D173, D177, D178, D179 in `docs/junction-tool/DECISIONS.md` and the section "Every part is an edge" in `docs/junction-tool/DESIGN-O4.md`.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`): `tools/junction/jt fast build-rel [projection] 5`, `tools/junction/jt fast build-rel [tmapnear] 5`, `tools/junction/jt fast build [junctionmap] 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop. Never type tag filters by hand.

## Read first (once each)
`CODINGSTYLE.md`; `ConstructJunctionTMap` (whole) and `FinishJunctionCandidate` (its crop: plain row-0 and W-row track outside the content's columns +-2 is removed) in `src/junction_generator.cpp`; `JunctionTMapResult`, `JunctionGenerateError` in `src/junction_generator.h`; `GenerateJunctionNear` in `src/junction_game.cpp`; `BuildJunctionWindow::ShowResult` in `src/junction_gui.cpp` (how an error becomes the status line); `STR_JUNCTION_ERROR_NO_LAYOUT_HERE` in `src/lang/english.txt`; the `[projection]` and `[junctionmap]` tests in `src/tests/junction_map.cpp`.

## Definitions (line-router path only)
- The flat T: D173's flat run (`ConstructJunctionTMap(spec, nullptr, std::nullopt, false)`). It now runs on the line-router path whenever `line_router` is true, also when `first_height` is given; `first_height`, when given, still sets the first height. Its layout and `hub` give A's ports.
- A's ports (frame coordinates): `x_w = frame.hub_x - flat.hub.x` and `x_e = x_w + flat.layout->size.x - 1`, both on row 0. Comment: D179, the ports come from the flat T, whose crop leaves the main line content +-2 (FinishJunctionCandidate).
- A port's approach: the port tile and the `STUB` tiles inward along row 0 (west port: x_w .. x_w + STUB; east port: x_e - STUB .. x_e).
- The plane: D177's hub plane (unchanged); every port is checked at it (a port at its own ground height is a later step: say so in a comment).

## Tests, red first
In `src/tests/junction_map.cpp`, `[projection]`:
1. The existing "an obstacle on the main line is reported" (blocked (3, 0)) changes: first assert that 3 < x_w of `SharedFlatT()` (computed as above), then expect the layout built, `clean`, equal tile for tile (tracks, bridges, signals) to the flat layout. Rename it "an obstacle on the main line outside the junction is ignored". Red: today no layout, log `fixed track on unusable tile 3,0`.
2. "a blocked port approach is reported at once": the tile (x_w + 1, 0) `Blocked`: no layout, `port_blocked` true, log contains `port approach blocked at {x_w + 1},0`, `heights_tried == 0`, and the routers were not called (`micros_route == 0`). Red: today the log says `fixed track on unusable tile`.
3. "an obstacle on the main line inside the junction is routed or reported by A": the tile (xI - 4, 0) `Blocked` (first assert it is strictly between x_w + STUB and x_e - STUB and is not a tile of the flat layout's A->S or N->A routes, using `a_to_s_tiles`/`n_to_a_tiles` of the flat line-router T). Either a clean layout whose track avoids the tile, or no layout with every height's log line `A failed`. Report which, with the log. Red: today `fixed track on unusable tile`.
In `[junctionmap]`: the existing test that the line-router T equals the old router's T (both specs) must stay green unchanged: this is the flat identity (D173).
In `src/tests/junction_game.cpp`, `[tmapnear]`:
4. "GenerateJunctionNear reports a blocked port": `MakeNearTestTerrain()` with the west port approach tile blocked at every near offset's position (block the tiles x_w - 4 .. x_w + STUB + 4 on rows -4 .. 4: compute x_w from the flat T as in test 1); `error == JunctionGenerateError::PortBlocked`, no layout. Red: today `NoLayout`.

## Changes
1. `src/junction_generator.h`: `JunctionTMapResult` gains `bool port_blocked = false; ///< D179: a port's approach is unusable; nothing was routed.` `JunctionGenerateError` gains `PortBlocked` (append it last).
2. `src/junction_generator.cpp`, `ConstructJunctionTMap`, line-router path:
   a. The flat T and A's ports as defined above. When the flat T builds no layout, return as today.
   b. When `terrain` is set, before the height loop and after the hub plane (D177): check both port approaches with `terrain->UsableAt(x, 0, plane)`; on the first unusable tile log `port approach blocked at x,0`, set `port_blocked`, set `micros`, return (no layout). Comment quoting D179.
   c. A is routed in every height before A->S, by `route_line` on a layout that does not contain row 0's straight track: start `{tile(x_w, 0), Trackdir::X_SW}`, goal `{tile(x_e, 0), Trackdir::X_SW}`, bounds `min = tile(x_w, 0)`, `max = tile(x_e, g)` (A's band: from row 0 to the row above B's W row; it may not cross B; leaving the band is a later step, comment so), the request's other fields from `base`, no blocked tiles beyond `last_row` (which `route_line` adds). On failure log `A failed` and continue with the next height. Its raster is added to `pass1` (and so to `fixed`) instead of `base_layout`'s row-0 straight track. `base_layout` gains a flag or a sibling so the line-router path lays only the stubs; the old-router path is unchanged.
   d. A->S's starts: the pieces of A's raster with track exactly `Track::X` and `2 <= x <= xI - 2` (any row), as `{piece.tile, Trackdir::X_SW}`, in place of the row-0 `TracksAt` scan. N->A's goals: the plain pieces of A's raster from `std::max(xO, x_s + ...)` (the existing lower bound) to `std::min(frame.width - 3, x_e)`, in place of `PlainRowPieces(pass1, 0, ...)`, keeping the same trackdir. If a helper is needed, make it a static function next to `PlainRowPieces` with a doc comment.
   e. The fixed-part check: row 0 is removed from it (A is routed). The W row's tiles are checked only for `x_w <= x <= x_e` (the same ports, until E3 routes B; comment so). The stub branch is unchanged.
3. `src/junction_game.cpp`, `GenerateJunctionNear`: when the T map's `port_blocked` is set, the converted result's `error` is `PortBlocked`.
4. `src/junction_gui.cpp`, `ShowResult`: `PortBlocked` maps to `STR_JUNCTION_ERROR_PORT_BLOCKED` beside the existing mappings; `src/lang/english.txt`: `STR_JUNCTION_ERROR_PORT_BLOCKED :{RED}A junction end is blocked here: move the cursor` next to `STR_JUNCTION_ERROR_NO_LAYOUT_HERE`. Status line only (as the other errors); no dialog, no new widget.

## Do not
No routing of B's lanes or the stubs, no change to the router, the old-router path, `GenerateJunction`, the library tier, `FinishJunctionCandidate`, decisions, skeleton, signal pass, checker, `junction_organic.*`, savegame, network or command code. No other language files. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- The flat identity test fails, or any existing test other than test 1's changes outcome (name it and its failure).
- A->S or N->A no longer routes on flat ground because of the start/goal change.
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-e1`: `Change: Junction T map routes the main line between its ports`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence (each test, the failure seen); members, enum values and strings added; test 3's outcome with the log; jt lines with times; BENCH line for the test spec (micros, micros_flat, micros_route) before and after; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
