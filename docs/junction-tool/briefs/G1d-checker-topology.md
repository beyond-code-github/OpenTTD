# G1d: the checker's ports and movements from a topology (RECON-G1d-roles §5; DESIGN-G1 §12.0)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-g1d <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- The rule checker (src/junction_rules.{h,cpp}) knows only the T: six fixed port roles, two port tiles per box edge, and "is a main line path" written as a role expression at about 12 sites. The new pipeline renders LL_RR (10 port slots), which the checker cannot trace. This step makes the checker read ports and movements from a JunctionTopology. The T must behave byte-identically: every legacy entry point becomes the generic one called with JunctionTTopology(), and every existing test stays green untouched. The signal pass, junction_nodes, throughput and the generator are NOT changed (they keep calling the legacy entry points).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` and `[rules]` rows).
- docs/junction-tool/RECON-G1d-roles.md (whole; it lists every site).
- src/junction_rules.h (whole, ~90 lines); src/junction_rules_internal.h :20-40.
- src/junction_rules.cpp: :25-60, :100-280 (RoleName, IsInRole, EdgeTiles, FindJunctionPorts, TraceFrom, MakeJunctionCheckContext), :336-460 (PortTile, CheckConnectivity, CheckOrder), :505-525, :690-712, :840-890, :1090-1120, :1300-1310, :1540-1590, :1680-1732.
- src/junction_topology.h :15-75 (JunctionSide, JunctionLaneGroup, JunctionTopology, JunctionPortSlot, JunctionPortSlots, JunctionLanePath, ExpandJunctionLanes) and src/junction_topology.cpp :21-37 (slot order) and :1771-1802 (both topologies).
- src/tests/junction_topology.cpp: RenderOf and the render tests (from "the T render traces like the flat T" to the end).

## Design (settled)
### Types (junction_rules.h; include "junction_topology.h")
- `JunctionPort`: add `uint16_t slot = UINT16_MAX; ///< Index into JunctionPortSlots of the context's topology.` Keep `role`, re-commented `///< The T's role; meaningful only when the topology is JunctionTTopology().`
- `JunctionPath`: add `uint16_t from_slot = UINT16_MAX; uint16_t to_slot = UINT16_MAX; uint16_t movement = UINT16_MAX; ///< Index into the topology's movements; UINT16_MAX when no expected lane path has these slots.` Keep `from`/`to`, re-commented as T-only like `role`.
- `JunctionCheckContext`: add `const JunctionTopology *topology; ///< Never null.` and `std::vector<JunctionLanePath> lanes; ///< The lane paths that must exist (ExpandJunctionLanes).` Keep `expected` (filled only by the legacy maker; the checker no longer reads it). Put the new members after `spec` and update every aggregate initialisation of the context accordingly (there is one, in MakeJunctionCheckContext).

### Functions
1. `std::vector<JunctionPort> FindJunctionPorts(const JunctionLayout &layout, const JunctionTopology &topology);` For each box edge in the order W (x == 0, the NE edge), E (x == size.x - 1, the SW edge), S (y == size.y - 1, the SE edge): the tiles from EdgeTiles; the side's slots (JunctionPortSlots entries whose group's side is that side, in slot order); if the tile count differs from the slot count, or any tile's bits are not exactly the axis track (X for W/E, Y for S), the edge gives no ports (as today). Otherwise the tiles in ascending `along` order are matched to the side's slots in slot order for W and S, and in reverse slot order for E (slots run south to north on E). Each port: `slot`; `trackdir` W in X_SW, W out X_NE, E in X_NE, E out X_SW, S in Y_NW, S out Y_SE ("in" = the slot's group.in); `role` = the T role of the slot when `&topology == &JunctionTTopology()` (slot 0 EIn, 1 WOut, 2 NIn, 3 SOut, 4 WIn, 5 EOut), else EIn. Ports are pushed in tile order (edge order W, E, S, ascending along), which for the T is exactly today's order. A group on the N side: no ports for it (the checker has no N edge; RECON §2).
   The legacy `FindJunctionPorts(layout)` becomes `return FindJunctionPorts(layout, JunctionTTopology());`.
2. `JunctionCheckContext MakeJunctionCheckContext(const JunctionLayout &layout, const JunctionSpec &spec, const JunctionTopology &topology, std::vector<JunctionPort> ports);` lanes = ExpandJunctionLanes(topology, nullptr); expected empty; traces from every port whose slot's group is `in`.
   The legacy `MakeJunctionCheckContext(layout, spec, ports, expected)` keeps its signature: topology = &JunctionTTopology(); every port with slot UINT16_MAX gets the slot of its role (the table above); lanes = for each expected role pair in order, `{slot(from), slot(to), m}` with m = the index of the movement (slot(from), slot(to)) in JunctionTTopology().movements (T groups have one lane, so slot == group); expected kept as given; then traces exactly as the generic maker.
3. TraceFrom takes the in port's slot instead of its role; at an exit it matches an out port (group not in) by tile and trackdir as today and pushes `{from role, to role, pieces, from_slot, to_slot, movement}` where the roles are the ports' `role` fields and movement = the `movement` of the lane in ctx.lanes with these slots, else UINT16_MAX. (Keep the member order of JunctionPath as declared; use designated or positional initialisation consistently with CODINGSTYLE.)
4. File-local helpers in junction_rules.cpp, used at every site below and nowhere else:
   - `static bool IsThroughPath(const JunctionCheckContext &ctx, const JunctionPath &path)`: movement != UINT16_MAX and both groups of topology.movements[movement] are `main`.
   - `static bool IsExitPath(const JunctionCheckContext &ctx, const JunctionPath &path)`: movement != UINT16_MAX, the in group is main and the out group is not.
   - `static bool SameLanePath(const JunctionPath &a, const JunctionPath &b)`: a.from_slot == b.from_slot && a.to_slot == b.to_slot. (Same lane path, not same movement: the doubled routes of one lane, which is what "same from/to" meant on the T. Two lanes of one movement are different lane paths.)
   - `static std::string PortName(const JunctionCheckContext &ctx, uint16_t slot)`: on the T topology RoleName of the slot's role (today's text); otherwise `fmt::format("{}{} {}", side letter W/S/E/N, lane + 1, in ? "in" : "out")` from JunctionPortSlots.
   - `PortTile(ctx, slot)` takes a slot.
5. Sites (RECON §1 numbers), each replaced and nothing else changed:
   - is-ML expression -> IsThroughPath: CheckOrder (:435), CheckBridges (:515), JoinerSignals (:695), CheckWaitingRoom rule 6 (:883), CheckPrio (:1094), CheckPrioReach (:1544).
   - same from/to -> SameLanePath: CheckBridges (:518), JoinerSignals (:707), CheckPrio (:1117, :1307), CheckPrioReach (:1565, :1588).
   - CheckWaitingRoom rule 5 (:848): `if (!IsExitPath(ctx, path)) continue;`; its PortTile uses path.from_slot.
   - CheckConnectivity: iterate ctx.lanes (count paths with that from_slot/to_slot; messages with PortName of the slots; PortTile of from slot); "unexpected movement": a path whose movement is UINT16_MAX; "missing port": every slot named by ctx.lanes with no port of that slot, named by PortName. Message texts on the T stay exactly as today.
   - IsInRole: removed; its two uses test the port's group `in` via the topology.
   - CheckJunctionRules and JunctionRulesClean: add overloads `(layout, spec, topology)` building the context with the generic maker and FindJunctionPorts(layout, topology); the legacy two-argument versions call them with JunctionTTopology().
   - JunctionTMovements stays as is.
6. No other file changes, except junction_rules.h/.cpp, src/tests/junction_topology.cpp, TESTS.md. If anything outside junction_rules.cpp reads JunctionPath.from/to or JunctionPort.role in a way that now breaks a test, STOP.

## Tests (tag `[topology]`, in src/tests/junction_topology.cpp after the render tests; record each red)
1. **Outer: "topology: the checker traces the LL_RR render"**: render = RenderOf(LL_RR, 64); ctx = MakeJunctionCheckContext(render, MakeTestJunctionSpec(), JunctionLLRRSidelineTopology(), FindJunctionPorts(render, JunctionLLRRSidelineTopology())). REQUIRE 10 ports; CHECK each port's tile == the placement's port tile of its slot (use RenderOf's placement out-parameter); CHECK CheckConnectivity(ctx) is empty (print the violations with INFO on failure); CHECK every lane path of ctx.lanes has 1 path, or 2 when it runs through a doubled leg. Red: before the code exists the test does not compile; record the first assertion failure after stubbing the generic FindJunctionPorts to return {}.
2. **The T render, both ways:** for the T render (RenderOf(T, 64)) and the flat T (ConstructJunctionTMap(MakeTestJunctionSpec(), nullptr, std::nullopt, true)), CheckJunctionRules(layout, spec) and CheckJunctionRules(layout, spec, JunctionTTopology()) give the same violations (rule, tile, message, in order). Red with the stub: the generic one reports missing ports.
3. **LL_RR geometry rules:** on the LL_RR render, CheckGeometry(ctx) and CheckFootprint(ctx) are empty. If either is not empty, STOP and report the violations (these are design findings, do not fix the render).
4. **Findings, not asserted:** run CheckJunctionRules(render, spec, JunctionLLRRSidelineTopology()) on the LL_RR render and write every violation (rule number, tile, message) to docs/junction-tool/wip/g1d-llrr-violations.txt (not committed; the render has no signals or prios, so Signals, WaitingRoom, Deadlock, Prio and PrioReach are expected). Do this from a hidden test tagged `[.][diag]` or a one-off; do not commit any printing test.
- Every existing test (all of `[junction]`) stays green and unedited. If one fails, STOP.

## Do not
- Do not change the signal pass, junction_nodes, junction_throughput, junction_generator, junction_chain, junction_organic, or the render/placement.
- Do not remove JunctionPortRole, `role`, `from`, `to`, `expected` or JunctionTMovements.
- Do not add an N edge to the checker.
- Do not change any violation message on the T.

## Stop conditions
- An existing test fails or would need an edit.
- Test 3 finds violations; test 1's connectivity is not clean (report the violations and RenderJunctionAscii of the LL_RR render to a file you name).
- Anything not covered here.

## Gates
- `tools/junction/jt fast .claude/nodes/build '[topology]' 5` and `tools/junction/jt fast .claude/nodes/build '[rules]' 10` green (if `[rules]` is not a TESTS.md tag, use the tag TESTS.md gives the rule checker tests); then `tools/junction/jt gate .claude/nodes/build-rel` green (380 before; report the count).
- One commit: `Add: Junction checker ports and movements from a topology (G1d)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "G1d" and the new test count.

## Report
The standard report (references read, commits, red evidence per test, types and members added or removed, suite results, stops, state), plus the path of the findings file and its violation counts per rule. When you have reported, stop.
