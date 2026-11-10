# CS2: an edge that ends on routed track keeps the curve rule over what continues from its goal (D178, CS1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
CS1 gave each edge the curve state of the track leading into its start (`approach`). The mirror case remains: an edge routed later that ends on routed track (a merge goal) never sees the curves that already continue from its goal piece. Diagnosis (`/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/DIAG-ph1-test4.md`, read it): in PH1's test 4 an edge from the N port merges into a W-row straight at frame (17,3); the next lane edge's `LR` jog was already routed from that piece; the merging edge's own `LR` plus that jog put L, R, L within the S-bend window (8 units), so the checker's rule 3 fails (111 below 160) while each edge passed the router alone. Router and checker count the same thing; the router lacked the information.

## State
- Worktree `/home/pete/OpenTTD/.claude/cs2`, branch `wt-cs2`, head given at launch (feature/junction-tool with CS1 merged). Create `build` (Debug) and `build-rel` (Release) like the main ones; build `openttd_test` only. If a build fails because a precompiled header is older than a system header, delete that .pch and rebuild.
- Tests only through `tools/junction/jt` (bracketed tags; TESTS.md): `tools/junction/jt fast build '[continuation]' 5` (new tag), `tools/junction/jt fast build '[approach]' 5`, `tools/junction/jt fast build-rel '[linerouter]' 5`, `tools/junction/jt fast build-rel '[projection]' 5`, `tools/junction/jt fast build '[junctionmap]' 5`, final `tools/junction/jt gate build-rel`. OVER BUDGET is a stop.

## Read first (once each)
`CODINGSTYLE.md`; the diagnosis file above; `docs/junction-tool/briefs/CS1-curve-state-across-edges.md`; `JunctionLineRouteRequest::approach` in `src/junction_line_router.h`; in `src/junction_line_router.cpp` the root's approach replay (CS1), `AdvanceState`, `TurnAllowed`, and where a goal is recognised (`is_goal_piece`, the goal test before a node is accepted as the route's end); `route_line`'s `routed_rasters` and `approach` lambda in `src/junction_generator.cpp`.

## Tests, red first (`src/tests/junction_line_router.cpp`, tag `[junction][continuation]`)
1. "a goal's continuation S-bend counts against the route's last one": a one-track request on flat ground whose goal is followed by a `continuation` that S-bends at once (`L`, `R` pieces right after the goal), and whose cheapest route without the continuation would end with an S-bend just before the goal. With the continuation, the route ends with the spacing the router requires before the continuation's S-bend (or reaches another goal), and passing the route's pieces followed by the continuation's through `AdvanceState` shows no refused turn. Red: the member does not exist (declare it first, unused, so the red is an assertion).
2. "an empty continuation changes nothing": as CS1's test 2, for a few existing requests.

## Changes
1. `src/junction_line_router.h`: `JunctionLineRouteRequest` gains `std::function<std::vector<JunctionPiece>(const JunctionRouteEnd &goal)> continuation; ///< For a goal on routed track: the pieces of that track continuing from the goal piece, in travel order (at most what the curve state looks ahead over). A route may end at the goal only if its curve state, carried through these pieces with the same update as the route's own pieces, refuses no turn; they are not laid, occupied, priced or checked otherwise (CS2). Unset: no check.`
2. `src/junction_line_router.cpp`: where a node is accepted as reaching a goal, if `continuation` is set, carry track 0's curve state through the goal's continuation pieces with `AdvanceState`/`TurnAllowed` as the route's own pieces are; if any turn is refused, this node does not end the route (the search continues). Reuse CS1's replay helper if there is one; do not duplicate the curve logic.
3. `src/junction_generator.cpp`, `route_line`: set `continuation` to a lambda over `routed_rasters` (newest first, like `approach`) that finds the raster holding a piece equal (tile and trackdir) to the goal and returns the pieces after it, at most `max(curve_spacing, s_bend_window)` (CS1's window), in travel order. None found: empty. Comment citing this brief and the diagnosis.

## Then
Merge this branch's result into nothing; the orchestrator merges it. Report whether PH1's test 4 would now be clean: build the `wt-ph1` branch's test 4 against your router only if it is cheap (cherry-pick into a scratch worktree is NOT allowed); otherwise say "not checked".

## Do not
No change to the checker, the search beyond goal acceptance, the old router, decisions, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- The flat identity, the golden trace (route strings), or any building test changes outcome or layout: report which with before and after pieces.
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-cs2`: `Change: Junction edges keep the curve rule over the track their goal continues into`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; jt lines with times; golden trace changes if any; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
