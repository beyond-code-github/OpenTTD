# NP4: the node search by a topology; LL_RR builds around a house (projection of the G1 seed, step 4)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-np4 <HEAD>` (given at launch). Never use git stash.
- NP1-NP3 (merged): TraceJunctionNodeInput(flat, spec, topology); SolveJunctionNodes(tpl, spec, facts, offset, topology) signals with SignalJunctionTrackRoutes; LL_RR solves on flat ground to its seed, checker clean. The search (SearchJunctionNodes: solve, heights check, detours on failure) still uses the T: BuildJunctionNodeTemplate(layout, spec) (legacy trace), the 4-argument solve, NodeAnchorShift (EIn->SOut roles), JunctionNodeHeightsOk (T movements), InsertJunctionDetour (builds the child's template by the legacy trace). This step threads a topology through all of them, the same way NP3 did for the solve (a file-static implementation taking `const JunctionTopology *`, nullptr = today's T behaviour).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` row).
- src/junction_nodes.h :200-245 (JunctionNodeHeightsOk, InsertJunctionDetour, JunctionNodeSearch, SearchJunctionNodes).
- src/junction_nodes.cpp: JunctionNodeHeightsOk (whole), InsertJunctionDetour (whole; its BuildJunctionNodeTemplate call), NodeAnchorShift (whole), SearchJunctionNodes (whole), and NP3's SolveJunctionNodesWith pattern.
- src/tests/junction_nodes.cpp :225-240 (MakeMapTestTerrain, BlockMapTestTile), :552-590 (SearchFlatT, the house search tests). src/tests/junction_topology.cpp: NP3's LL_RR flat-solve test (its flat terrain/facts).

## Tests (tag `[topology]`; record each red)
1. **Outer, "topology: the LL_RR search builds around a house on a W in-fan"**: seed = SignalledOf on LL_RR; terrain flat as in NP3's test; block (as BlockMapTestTile does) the tile, plus the test's offset, of the second piece of the first free diagonal of the template's path 0 (RP1: the W in-fan rising from (2,4); compute it from the template, do not hard-code). search = SearchJunctionNodes(seed, spec, terrain, offset, 16, topology) with the offset NP3's test uses. REQUIRE search.layout; the blocked tile holds no track of the layout (at its offset); CheckJunctionRules(layout, spec, topology) is empty (INFO each); JunctionNodeHeightsOk(layout, spec, terrain, offset, &why, topology) is true. Print RenderJunctionAscii of the result to docs/junction-tool/wip/np4-llrr-house.txt. Red: first add the overloads as stubs that call the legacy functions; record the failure.
2. **"topology: the T search by its topology keeps the flat T"**: SearchJunctionNodes on the T seed (SignalledOf) on flat terrain with JunctionTTopology(): layout tracks equal the seed's; checker clean.
- Every existing `[nodes]`, `[nodespeed]`, `[topology]`, `[game]`, `[projection]` test stays green and unedited.

## Changes (junction_nodes.{h,cpp})
1. Overloads, each declared right after its legacy declaration, doc comment "As above, for a layout of a topology (ports, movements and signals from it)":
   - `bool JunctionNodeHeightsOk(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, std::string *why, const JunctionTopology &topology);` its context: MakeJunctionTopologyCheckContext(layout, spec, topology, FindJunctionPorts(layout, topology)).
   - `std::optional<JunctionLayout> InsertJunctionDetour(..., std::string *why, const JunctionTopology &topology);` the child's template: BuildJunctionNodeTemplate(TraceJunctionNodeInput(normalised, spec, topology), spec).
   - `JunctionNodeSearch SearchJunctionNodes(const JunctionLayout &flat, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, int max_states, const JunctionTopology &topology);` every template build by the topology trace, the solve overload, the heights overload, the detour overload, and NodeAnchorShift with the topology.
   Each legacy function becomes a call of the file-static implementation with nullptr; behaviour unchanged.
2. **NodeAnchorShift** takes `const JunctionTopology *topology`: nullptr keeps today's code; otherwise its context is the topology context, the template's anchor path is the first template path whose input path is an exit (as NP1's anchor: rebuild the same choice from the context: the first ctx path with IsExitPath(ctx, path)), and the layout path is the first ctx path with IsExitPath. If the two "first exit" paths are not the same lane path (from_slot/to_slot), return nullopt.

## Do not
- Do not change any legacy behaviour, the template rules, the solve, the render, GenerateJunctionNear or the GUI.
- Do not add the D212 fan preference (NP5).

## Stop conditions
- Test 1: the search finds no layout (report search.log's last 30 lines to a file and the failure kinds), or the result has checker violations (report each), or heights fail (report why). Do not fix them.
- An existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[nodes]' 5`; `tools/junction/jt gate build-rel` (report the count).
- One commit: `Add: Junction node search by a topology; LL_RR builds around a house`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "NP4" and the new count.

## Report
The standard report, plus: search states and Release time for test 1 (median of 5), and where the in-fan moved (its first diagonal's first tile before and after). When you have reported, stop.
