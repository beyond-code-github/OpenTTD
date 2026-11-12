# FN1: in-fans keep their port row as long as they can (D212)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`; run tools/junction/jt from the worktree root with `build` / `build-rel`). First: `git checkout -B wt-fn1 <HEAD>` (given at launch). Never use git stash.
- D212 (DECISIONS.md, read it): a lane runs on its port row as long as it can and changes rows next to the feature that needs the other row. Out-fans already do (placement pins them exactly SHIFT_BACK after the joiner merge, the port 2 after). In-fans do not: the X solve takes least x, so on rows heading east (x+) the in-fan sits 2 tiles after the port; the floor to the exit split is `along(heading, nex, rp.fil, d_min, false)` (junction_topology.cpp, the "In: port, fan, exit split." block, CS1). Making it exact was tried: 3 render tests fail (the late fan meets other tracks on its rows). So the fan slides toward its split only as far as the tiles are free.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` row); DECISIONS.md row D212.
- src/junction_topology.h :220-end (JunctionTrackPlacement, Cut, Render, routes, signal tokens).
- src/junction_topology.cpp: the in chain block with d_min (grep `int d_min = 2;`, ~30 lines), the placement output section where `result.edges` spans are filled (grep `result.edges`), RenderJunctionTrackPlacement's Fan handling (grep `StepKind::Fan` inside it) and its route loop.
- src/tests/junction_topology.cpp: RenderOf, SignalledOf, the render tests, "the LL_RR seed is clean"; src/tests/junction_design.cpp the LL_RR circuit test's pipeline calls.

## Design (settled)
1. **Placement exports the slack.** JunctionTrackPlacement gains `std::vector<int> fan_slide; ///< Per graph edge: how many tiles an in-fan may move toward its exit split before its curve gap binds (D212); 0 for every other edge.` In PlaceJunctionTrackGraph, after the X solve, for each in row r with m_in > 0: fan_slide[in_edge[r]] = the along-heading distance from the solved fil to (solved nex - d_min), which is >= 0 (keep d_min per row from the in chain block for this). All else 0. Placement's solve is unchanged (all existing placement numbers stay).
2. **The slide** (new function, declared after RenderJunctionTrackPlacement):
```cpp
/**
 * Slide every in-fan toward its exit split as far as its curve gap and the free tiles allow (D212): the lane keeps its port row as long as it can.
 * @param placement Updated in place: the Fan tokens' spans of the in edges.
 */
void SlideJunctionTrackFans(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, JunctionTrackPlacement &placement);
```
   - Render once with routes (RenderJunctionTrackPlacement). If it fails, return unchanged.
   - For each graph In edge e with fan_slide[e] > 0 and a Fan token t (programs.edges[e]) whose span's first and last tiles differ in the cross-heading coordinate (the fan moves rows): D = the unit step of the edge's heading (from its in port's side, as the render's step 1). Own tiles = the pieces of every route of a path through e, from the route's first piece up to and including the first piece on the edge's node tile (placement.nodes of the edge's end node). Other tiles = every tile of every route piece (bridge pieces included) not in own tiles, plus every bridge head of placement.bridges.
   - For s = fan_slide[e] down to 1: the candidate tiles are the fan's pieces shifted by s * D (take the fan's pieces from one of those routes: the pieces from the tile span.first to span.last) plus the s port-row tiles from the fan's old first piece's port-row axis tile onward (old axis tile before the fan, + 1*D ... + s*D). If none of the candidate tiles is in other tiles, apply: placement.edges[e][t] span first and last += s * D; stop. If no s fits, leave it.
   - Every In edge independently; the order is graph edge order.
3. **Callers.** RenderOf (tests) calls SlideJunctionTrackFans after CutJunctionTrackBridges and before rendering; the LL_RR circuit test (junction_design.cpp) likewise. PlacementOf/CutOf do not (the placement tests keep their numbers).

## Tests (tag `[topology]`; record each red)
1. **Outer, "topology: LL_RR's in-fans keep their port rows" (D212):** with the slide, every In edge e with a row-moving Fan has its fan's last tile exactly d_min before its exit split (fan_slide fully used) OR the shift one further would put a candidate tile on another route's tile (check by recomputing other tiles from a render with the slid placement). And at least the two lanes entering at the W side (heading x+) moved (shift > 0). Red: before the slide the W-side fans have shift 0.
2. **The T:** RenderOf(T) is unchanged by the slide (the T's fans do not move rows), i.e. the flat-T oracle and every T test stay green unedited.
3. **All existing render, signal and "LL_RR seed is clean" tests stay green** with the slide in RenderOf (they then test the slid layout). If one fails, STOP and report it with RenderJunctionAscii.

## Do not
- Do not change the X solve, any floor, the out-fans, programs, the signal tokens or the checker.
- Do not slide anything other than in-edge Fan tokens.

## Stop conditions
- An existing test fails; the slide makes a checker violation on LL_RR; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt gate build-rel` (388 before).
- Regenerate tools/ottd/layouts/circuit-llrr-TL4.json with the Release build (`JUNCTION_CIRCUIT=$PWD/tools/ottd/layouts/circuit-llrr-TL4.json build-rel/openttd_test "[.circuitllrr]"`) and commit it.
- One commit: `Change: Junction in-fans keep their port row as long as they can (D212)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "FN1" and the count.

## Report
The standard report, plus per moved fan: edge, shift s, fan_slide; and RenderJunctionAscii of LL_RR before and after to docs/junction-tool/wip/fn1-llrr.txt. When you have reported, stop.
