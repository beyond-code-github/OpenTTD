# NP3: the node solve signals by a topology; LL_RR solves on flat ground (projection of the G1 seed, step 3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-np3 <HEAD>` (given at launch). Never use git stash.
- NP1/NP2 (merged): the LL_RR seed builds a node template. SolveJunctionNodes (junction_nodes.cpp) solves a template against terrain facts, renders the tracks, then signals them with the legacy T pass `PlaceJunctionSignals(layout, spec)` (T port roles). The G1 seeds are signalled by `SignalJunctionTrackRoutes(layout, routes, topology, lane_paths, spec)` (junction_topology.h :250-280), which needs routes: per lane path and copy, its pieces. This step gives the solve a topology overload that signals with SignalJunctionTrackRoutes, and makes LL_RR solve on flat ground.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` row); RECON-seed-projection.md §1, §4.
- src/junction_nodes.h :160-215 (RenderJunctionNodeTracks, JunctionNodeSolve, SolveJunctionNodes); junction_nodes.cpp SolveJunctionNodes from its start to its end (the "6. Render." block at its end).
- src/junction_topology.h :45-65 (JunctionLanePath, ExpandJunctionLanes), :245-285 (JunctionTrackRoute, SignalJunctionTrackRoutes).
- src/junction_rules.h :35-62 (JunctionPath from_slot/to_slot/movement; MakeJunctionTopologyCheckContext; FindJunctionPorts).
- src/tests/junction_topology.cpp: RenderOf, SignalledOf, NP1/NP2's LL_RR template test. src/tests/junction_nodes.cpp: find how existing tests build flat JunctionFacts / JunctionTerrain for a flat solve at offset 0 (grep `SolveJunctionNodes(`), and copy that pattern.

## Tests (tag `[topology]`; record each red)
1. **Outer, "topology: the LL_RR seed solves on flat ground"**: seed = SignalledOf on LL_RR; tpl = BuildJunctionNodeTemplate(TraceJunctionNodeInput(seed, spec, topology), spec); flat facts covering the seed's box with a margin of 8 tiles each way (copy the [nodes] pattern); `SolveJunctionNodes(tpl, spec, facts, {0, 0}, topology)`. REQUIRE no failure and a layout; the layout's track bits per tile equal the seed's (after both are normalised: same tile set, same TrackBits per tile; bridges equal as sets); `CheckJunctionRules(*layout, spec, topology)` is empty (INFO each violation). Red: write it first against a stub overload that calls the 4-argument solve (legacy signals); record the failure (expected: violations from the T signal pass, or a solve failure: record which).
2. **"topology: the T seed solves the same by its topology"**: the same on the T seed with JunctionTTopology(): tracks equal the seed's, checker clean (CheckJunctionRules(layout, spec, JunctionTTopology())).
3. **"topology: routes of a layout"** (inner): on the T seed, JunctionRoutesOfLayout(seed, spec, topology, lane_paths) gives, per route from SignalledOf's `routes` (matched by path and copy), the same pieces (tile and trackdir per piece). Red: the stub returns an empty vector.
- Every existing `[nodes]`, `[nodespeed]`, `[topology]`, `[game]` test stays green and unedited.

## Changes
1. **Routes from a layout** (junction_topology.h, declared after SignalJunctionTrackRoutes; defined in junction_topology.cpp):
```cpp
/**
 * The routes of a laid layout, traced by its topology: per checker path, the lane path with the same from and to slots; the copy is 1 for the second checker path of the same lane path, else 0.
 * @param paths ExpandJunctionLanes(topology).
 * @return The routes in checker path order; empty when a checker path matches no lane path.
 */
std::vector<JunctionTrackRoute> JunctionRoutesOfLayout(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths);
```
   Body: ctx = MakeJunctionTopologyCheckContext(layout, spec, topology, FindJunctionPorts(layout, topology)); per ctx path: the lane path index i with paths[i].from == path.from_slot and paths[i].to == path.to_slot (none: return {}); copy = the number of earlier ctx paths with the same slots (more than 1 earlier: return {}); the pieces copied. If test 3 shows the copies in the other order than the seed's routes, STOP and report (do not reorder by geometry).
2. **The solve overload** (junction_nodes.h, after SolveJunctionNodes):
```cpp
/** SolveJunctionNodes for a layout of a topology: its signals from SignalJunctionTrackRoutes (DESIGN-G1 §12) instead of the T's pass. */
JunctionNodeSolve SolveJunctionNodes(const JunctionNodeTemplate &tpl, const JunctionSpec &spec, const JunctionFacts &facts, TileIndexDiffC offset, const JunctionTopology &topology);
```
   Implementation: move the body of the 4-argument solve into a file-static `SolveJunctionNodesWith(tpl, spec, facts, offset, const JunctionTopology *topology)`; in "6. Render.", when topology is null call `PlaceJunctionSignals(layout, spec)` as today, else: `std::string error; const std::vector<JunctionLanePath> lanes = ExpandJunctionLanes(*topology, &error); const std::vector<JunctionTrackRoute> routes = JunctionRoutesOfLayout(layout, spec, *topology, lanes); SignalJunctionTrackRoutes(layout, routes, *topology, lanes, spec);` (if routes is empty, set `result.failure = JunctionNodeFailure{Kind::Template}`, clear values and return, as the other failures do). Then NormaliseJunctionLayout as today. The 4-argument solve calls SolveJunctionNodesWith(..., nullptr); the overload passes &topology. Include junction_topology.h in junction_nodes.cpp (check for an include cycle: if junction_topology.h includes junction_nodes.h, STOP and report).

## Do not
- Do not change the 4-argument solve's behaviour, the legacy signal pass, the template, the search, GenerateJunctionNear, heights or the anchor.
- Do not add terrain to these tests (later steps).

## Stop conditions
- Test 1: the solve fails (report the JunctionNodeFailure kind and fields), the tracks differ (report the tile diff count and the first 10 differing tiles with both TrackBits), or checker violations remain after the overload (report each, RenderJunctionAscii of the solved layout to docs/junction-tool/wip/np3-llrr.txt). Do not fix any of them.
- An existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[nodes]' 5`; `tools/junction/jt gate build-rel` (report the count).
- One commit: `Add: Junction node solve signalled by a topology; LL_RR solves on flat ground`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "NP3" and the new count.

## Report
The standard report, plus the Release time of the LL_RR template build and solve (median of 5). When you have reported, stop.
