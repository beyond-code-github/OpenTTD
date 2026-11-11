# G1a: the template builder takes paths and per-piece facts, not a layout (DESIGN-G1 §5, §6)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-g1a <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- G0 (merged) added `DiffJunctionNodeTemplates` and pinned the traced T (comparator tests, "the traced T is a fixed point"). This step separates the rule builder from the tracer so that later steps can feed it generated paths (D201, D202). **No behaviour change**: every template must be identical (the comparator proves it).
- Measured: NodeBuilder (src/junction_nodes.cpp ~:53) holds `const JunctionCheckContext &ctx` and reads only `ctx.paths` (15 uses) and `ctx.layout` (3 uses: `UsedTracks(ctx)` ~:140, `SwitchEventsOf(layout, used, path)` ~:147, `IsLegalSignalPiece/IsSignalledPiece(ctx.layout, piece)` ~:476) plus `layout.bridges` (~:217, ~:327). BuildJunctionNodeTemplate (~:658) makes the context with `MakeJunctionCheckContext(flat, spec, FindJunctionPorts(flat), JunctionTMovements())`.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md; DESIGN-G1.md §5-§6.
- src/junction_nodes.h (BuildJunctionNodeTemplate, DiffJunctionNodeTemplates); src/junction_nodes.cpp :40-680 (NodeBuilder and every member function, BuildJunctionNodeTemplate).
- src/junction_rules.h JunctionCheckContext, JunctionPath; src/junction_rules_internal.h :35-56 (UsedTracks, SwitchEvent, SwitchEventsOf, IsSignalledPiece, IsLegalSignalPiece).
- src/tests/junction_nodes.cpp: the two G0 tests.

## Design (settled)
1. junction_nodes.h (include "junction_rules_internal.h" there if SwitchEvent is not otherwise visible):
   ```cpp
   /** What a node template is built from (DESIGN-G1 G1a): the paths and the per-piece facts the rules read. Holds no layout. */
   struct JunctionNodeInput {
   	std::vector<JunctionPath> paths; ///< In the checker's path order.
   	std::map<std::pair<int, int>, TrackBits> used; ///< Per tile, every track on it (UsedTracks).
   	std::vector<std::vector<SwitchEvent>> events; ///< Per path, its switch events (SwitchEventsOf).
   	std::vector<std::vector<bool>> slots; ///< Per path and piece: whether a signal may stand there (IsLegalSignalPiece or IsSignalledPiece).
   	std::vector<JunctionBridge> bridges; ///< Every bridge or tunnel.
   };
   /** The template input traced from a flat layout (today's only producer). */
   JunctionNodeInput TraceJunctionNodeInput(const JunctionLayout &flat, const JunctionSpec &spec);
   /** The node template of an input (DESIGN-N3 §1-§8). */
   JunctionNodeTemplate BuildJunctionNodeTemplate(const JunctionNodeInput &input, const JunctionSpec &spec);
   ```
   The existing `BuildJunctionNodeTemplate(const JunctionLayout &flat, const JunctionSpec &spec)` stays, with its doc, as `return BuildJunctionNodeTemplate(TraceJunctionNodeInput(flat, spec), spec);`.
2. TraceJunctionNodeInput: makes the context exactly as BuildJunctionNodeTemplate does today and fills each member from the same calls NodeBuilder makes today (used = UsedTracks(ctx); events[pi] = SwitchEventsOf(flat, used, ctx.paths[pi]); slots[pi][j] = IsLegalSignalPiece(flat, piece) || IsSignalledPiece(flat, piece); bridges = flat.bridges; paths = ctx.paths).
3. NodeBuilder holds `const JunctionNodeInput &input` instead of `ctx`; every `ctx.paths` becomes `input.paths`; the three layout reads and `layout.bridges` read the input's members. Any NodeBuilder comment that names the layout or the context moves with the change (say "input"). No rule logic changes.
4. Nothing else changes. Other callers of MakeJunctionCheckContext in junction_nodes.cpp (~:1498, ~:1651) are not template building: leave them.

## Tests (record each red)
1. `[nodes]` "the template from a traced input equals the template from its layout": for FlatT and for each layout produced by InsertJunctionDetour in the existing detour tests you find (at least one; name them): `CHECK(DiffJunctionNodeTemplates(BuildJunctionNodeTemplate(layout, spec), BuildJunctionNodeTemplate(TraceJunctionNodeInput(layout, spec), spec)).empty())`. Red: does not compile (record it); there is no meaningful assertion red for a pure refactor, say so.
2. `[nodes]` "the template input holds no layout": `static_assert` that JunctionNodeInput's members are exactly the five above is not expressible; instead write the test as: build an input from FlatT, then let the FlatT layout go out of scope (copy FlatT's layout into a local, trace, destroy the local) and build the template from the input; CHECK it equals FlatT's template. This proves the input owns its data.
3. The G0 tests, `[nodes]`, `[nodespeed]`, `[tmapnear]`, `[nodeoracle]` and the gate stay green with identical results; no existing expectation changes (if one would, STOP).

## Do not
- Do not change any rule, the solve, the render, the search, the checker, or the paths' tracing. No generated paths yet. Do not touch JunctionTMovements or port roles.

## Stop conditions
- NodeBuilder needs any other context member or layout query than the five listed: report which and where, stop.
- Any template differs (comparator non-empty): report the diff, stop without committing.
- `[nodes]` Release over 1 s.

## Gates
`tools/junction/jt fast build "[nodes]" 5`, `tools/junction/jt fast build-rel "[nodes]" 1`, `tools/junction/jt fast build-rel "[nodespeed]" 5`, `tools/junction/jt fast build-rel "[tmapnear]" 5`, `tools/junction/jt gate build-rel`. Report the [nodeoracle] ORACLE line (must be accepted 90, heights=0, fits=0). Update TESTS.md times. Commit: `Codechange: Junction node template builds from paths, not a layout`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; types/functions added; the detour layouts used in test 1; suites with times; ORACLE line; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
