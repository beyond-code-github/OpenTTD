# CS1: an edge that continues routed track inherits its curve state (D178)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
D178 splits the hub into edges routed one at a time. A train's path runs through several edges in a row (B's lane edges; A then A->S after its split; the N port edge then N->A), but the line router starts every request with a fresh curve state (curve spacing, S-bend window). PH1's test found the result: two S-bends in consecutive edges of one lane, one tile apart; each edge passed the router alone, and the checker (which walks whole paths) reported "curve speed limit 111 below 160". The rule: an edge that starts on routed track inherits the curve state of the track leading into its start piece.

## State
- Worktree `/home/pete/OpenTTD/.claude/cs1`, branch `wt-cs1`, head given at launch (feature/junction-tool with E3b merged). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only. If a build fails because a precompiled header is older than a system header, delete that .pch and rebuild.
- Tests only through `tools/junction/jt` (read `docs/junction-tool/TESTS.md`; tags in brackets): `tools/junction/jt fast build '[approach]' 5` (new tag), `tools/junction/jt fast build-rel '[linerouter]' 5`, `tools/junction/jt fast build-rel '[projection]' 5`, `tools/junction/jt fast build '[junctionmap]' 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop.

## Read first (once each)
`CODINGSTYLE.md`; the glossary and the routing-order notes in `docs/junction-tool/DESIGN-O4.md`; `JunctionLineRouteRequest` (`prefix`, `curve_spacing`, `s_bend_window`, `starts`) in `src/junction_line_router.h`; in `src/junction_line_router.cpp` how the root nodes are built from `starts` and how the prefix's pieces set the curve state (D150) without the search moving them; `route_line` and every routed edge's `starts` in `ConstructJunctionTMap` (`src/junction_generator.cpp`); the checker's curve-speed rule (rule 3) in `src/junction_rules.cpp`, only to see how it walks a path across pieces.

## Tests, red first
In `src/tests/junction_line_router.cpp`, tag `[junction][approach]`:
1. "an approach's S-bend counts against the next one": a one-track request on flat ground whose start is the end of an `approach` (pieces of an S-bend `LR` ending one tile before the start), and whose cheapest route without the approach would S-bend at once (goal one row over, close ahead, so an immediate `LR` is cheapest). With the approach the route keeps the spacing the router requires after an S-bend (compare: the same request where the approach's S-bend is part of the `prefix` gives the same route). Red: the member does not exist (declare it first, unused, so the red is an assertion).
2. "an empty approach changes nothing": a few existing requests with an empty approach give identical routes (the golden trace test covers the T map).
In `src/tests/junction_map.cpp`, `[projection]`: no new test here; PH1's test 4 is the end-to-end case and lands with PH1.

## Changes
1. `src/junction_line_router.h`: `JunctionLineRouteRequest` gains `std::function<std::vector<JunctionPiece>(const JunctionRouteEnd &start)> approach; ///< For a start on routed track: the pieces of that track leading into the start piece, oldest first (at most what the curve state looks back over). They set the start's curve and S-bend state as a prefix would, and are not laid, occupied, priced or checked (CS1). Unset: a fresh state.`
2. `src/junction_line_router.cpp`: when building each root, if `approach` is set, feed its pieces for that start through the same curve-state update the prefix uses, before the start piece, without laying or checking them. Reuse the prefix's update; do not duplicate the curve logic. If the prefix path cannot take pieces (only moves), stop and report what it needs.
3. `src/junction_generator.cpp`, `ConstructJunctionTMap`, line-router path: `route_line` sets `approach` to a lambda that finds the routed raster (of the edges already in the layout being built, which `route_line` can reach through a list the function keeps of every routed raster: add that list as a local vector of rasters, appended where each edge's raster is added) containing a piece equal (tile and trackdir) to the start, and returns the pieces before it in that raster, at most `s_bend_window` of them (or the curve window the router uses, whichever is longer: say which). No raster found: empty. Comment citing this brief.

## Do not
No change to the router's search beyond the root's curve state, the checker, the old router, decisions, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- The flat identity (`[junctionmap]`) or any building `[projection]` test changes outcome or layout: report which (a changed layout means an edge's curves were wrong before; report the before and after pieces, do not accept it silently).
- The golden trace changes: report the changed lines (expansion counts may change only where an approach now applies; route strings changing is a stop).
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-cs1`: `Change: Junction edges inherit the curve state of the track they continue`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; which window length the approach uses; jt lines with times; golden trace changes if any; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
