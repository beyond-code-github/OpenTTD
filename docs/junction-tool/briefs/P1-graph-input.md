# P1: the node template's input built from the graph, with tags (DESIGN-G1 §13, §13.8; D201, D213, D214)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

**Source rule (D213, the reason for this step):** the new function reads graph data only (topology, lane paths, graph, programs, placement, routes). It must not take, build, read or trace a JunctionLayout. RenderJunctionTrackPlacement still draws a layout as a side effect; you use only its routes.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-p1 <HEAD>` (given at launch). Never use git stash.
- RECON-graph-input.md (RP2, read it whole) proved by a diag that a graph-built input gives an empty DiffJunctionNodeTemplates against the traced template for both the T and LL_RR, and gives the recipe per field (Q1 order and roles, Q4 events incl. the doubled-leg lane split/merge, Q5 slots, Q6 bridges in route-appearance order, Q7 where edge/token/copy are known in the render's route loop). This step implements that recipe.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` row); DESIGN-G1.md §13.8 (end of file); RECON-graph-input.md (whole).
- src/junction_nodes.h JunctionNodeInput and TraceJunctionNodeInput; junction_nodes.cpp TraceJunctionNodeInput.
- src/junction_topology.h :195-285 (programs, placement, JunctionTrackRoute, RenderJunctionTrackPlacement); junction_topology.cpp RenderJunctionTrackPlacement's route loop (RP2 Q7 lines).
- src/junction_rules.cpp :110-170 (TRoleOfSlot, IsThroughPath, IsExitPath); src/junction_rules_internal.h.
- src/tests/junction_topology.cpp: RenderOf, SignalledOf, NP1/NP2 tests.

## Tests (tag `[topology]`; record each red)
1. **Outer, "topology: the graph builds the traced template"**: for the T and for LL_RR (two SECTIONs or a loop): from RenderOf's outputs (paths, graph, programs, placement via the out-params, routes) build `JunctionNodeInputOfGraph(...)`; `DiffJunctionNodeTemplates(BuildJunctionNodeTemplate(graph_input, spec), BuildJunctionNodeTemplate(TraceJunctionNodeInput(render, spec, topology), spec))` is empty (INFO the diff). Use the same spec in both (RenderOf's spec: MakeTestJunctionSpec with max_bridge_middle 64). Red: the function first returns an empty input; record the failure.
2. **"topology: route pieces carry their graph tags"**: for both topologies, every route has one tag per piece; every tag's edge is an edge of the route's lane path (graph.path_edges) and its copy the route's copy; for every port edge's Fan token t with a non-empty span, the pieces tagged with token t start at the span's first tile and end at its last tile. Red: tags empty.
- Every existing `[nodes]`, `[nodespeed]`, `[topology]` test stays green and unedited.

## Changes
1. **Tag type and routes** (junction_topology.h, before JunctionTrackRoute):
```cpp
/** Which graph element a route piece renders (DESIGN-G1 §13.2). */
struct JunctionTrackPieceTag {
	static constexpr uint16_t RUN = UINT16_MAX; ///< `token` of a piece on a run between tokens.
	uint32_t edge; ///< Graph edge.
	uint16_t token; ///< Index into the edge's program (JunctionTrackPrograms::edges or ::copies by `copy`), or RUN.
	uint16_t run; ///< The number of the edge's tokens before this piece (run k lies between tokens k - 1 and k).
	uint8_t copy;
	bool operator==(const JunctionTrackPieceTag &) const = default;
};
```
   JunctionTrackRoute gains `std::vector<JunctionTrackPieceTag> tags; ///< Per piece.` Fill it in RenderJunctionTrackPlacement's route loop as RP2 Q7 says (record the move count at each token's appends, convert to piece indices; Under tokens emit no pieces; handle the D110 pop_back). Nothing else in the render changes.
2. **Roles:** make TRoleOfSlot non-static, declare it in junction_rules_internal.h with a doc comment "The T's port role of a slot (JunctionTTopology slot order); EIn beyond.".
3. **JunctionNodeInput** (junction_nodes.h): remove `used` (RP2 Q3: the builder never reads it); TraceJunctionNodeInput keeps a local `used` for its SwitchEventsOf calls. Add `std::vector<std::vector<JunctionTrackPieceTag>> tags; ///< Per path and piece: its graph element; empty when traced from a layout.` (include junction_topology.h if needed; it does not include junction_nodes.h).
4. **The function** (junction_nodes.h after TraceJunctionNodeInput; junction_nodes.cpp):
```cpp
/**
 * The template input built from the graph (D201, D213): no layout is read. Paths in the checker's order (RECON-graph-input Q1), events from the graph's nodes and the doubled legs' lane split/merge (Q4), slots by the geometric rule (Q5), bridges in route-appearance order (Q6), every piece tagged.
 * @param routes RenderJunctionTrackPlacement's routes.
 */
JunctionNodeInput JunctionNodeInputOfGraph(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionTrackPlacement &placement, const std::vector<JunctionTrackRoute> &routes);
```
   Per RP2: each input path = {from role, to role (TRoleOfSlot when &topology == &JunctionTTopology(), else EIn), route pieces, from_slot, to_slot, movement}; through/exit from topology.movements and groups' `main` (the same tests as IsThroughPath/IsExitPath); sort routes by Q1's key; events per Q4; slots per Q5 (no signals: geometric rule); bridges per Q6 (z 0); tags from the route. If any recipe item needs a layout read, STOP.

## Do not
- Do not change the builder, the solve, the search, the signal pass, TraceJunctionNodeInput's results, the anchor, or GenerateJunctionNear. Do not split the render into routes/drawing (later step).

## Stop conditions
- Test 1 not empty for either topology after the change (report the diff and which recipe item); any recipe item needing a layout; an existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[nodes]' 5`; `tools/junction/jt gate build-rel` (391 before; report the count).
- One commit: `Add: Junction node template input built from the graph, with tags`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "P1" and the new count.

## Report
The standard report (references read; commit; red evidence; types and members added or removed; suite results; stops; `git status --porcelain`, `git log -1`), plus Release time of JunctionNodeInputOfGraph on LL_RR (median of 5). When you have reported, stop.
