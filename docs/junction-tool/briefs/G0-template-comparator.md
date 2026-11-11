# G0: an exact node-template comparator (DESIGN-G1 §5, §6)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-g0 <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- Why: the node graph will be generated from a topology instead of traced from the constructed flat T (D201, D202, DESIGN-G1). Every later step must prove it changed nothing, or exactly what it meant to change. Today's only template test compares counts. This step adds the comparator and pins the traced T. No behaviour change.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md; DESIGN-G1.md §0b and §5-§6.
- src/junction_nodes.h :20-125 (JunctionNodeTemplate and every struct it holds: JunctionNodePoint, JunctionNodeLink, JunctionNodePath, JunctionNodePiece, JunctionNodeBridge, JunctionNodeJoin, JunctionNodeDiff, JunctionNodeDiagonal, JunctionNodeLateGap, JunctionNodeLateCrossing, JunctionNodePrioRow).
- src/tests/junction_nodes.cpp: FlatT, FlatToWorking, MakeMapTestTerrain, and one existing SolveJunctionNodes-on-flat test (grep `SolveJunctionNodes(tpl, spec, facts`).

## Design (settled)
1. In junction_nodes.h/.cpp, beside BuildJunctionNodeTemplate:
   ```cpp
   /**
    * The first difference between two node templates, or empty when they are equal (DESIGN-G1 G0): every member of the template and of every struct it holds, in order, element by element.
    * @return "<member>[<index>].<field>: <a> vs <b>" or "<member>: size <a> vs <b>"; empty when equal.
    */
   std::string DiffJunctionNodeTemplates(const JunctionNodeTemplate &a, const JunctionNodeTemplate &b);
   ```
   Compare every member listed in junction_nodes.h for JunctionNodeTemplate (points .. error) and every field of each element struct, exactly (no renaming, no tolerance). TileIndexDiffC by x and y; Trackdir/TrackBits/enums by value; nested vectors (e.g. JunctionNodePath's points, JunctionNodeLateGap::diagonals) element by element. If a struct gains a member later the comparator must be updated: add a `static_assert(sizeof(JunctionNodeX) == N)` per element struct next to its comparison, with N the size on this platform, and a comment "update DiffJunctionNodeTemplates when this struct changes".
2. Nothing else in src/ changes.

## Tests (record each red)
1. `[nodes]` "the template comparator finds every member": FlatT's template; `CHECK(DiffJunctionNodeTemplates(tpl, tpl).empty())`; then for every member of JunctionNodeTemplate and every field of every element struct that is non-empty in FlatT's template, a copy with that one field changed (first element; +1 for numbers, a different enum value, toggled bool, appended char for the string, one element removed for a vector): `CHECK` the diff is non-empty and starts with that member's name. One SECTION or one lambda call per field; list them all. A member that is empty in FlatT (say which) is checked by size only. Red: does not compile, then (stub returning "") every mutated CHECK fails.
2. `[nodes]` "the traced T is a fixed point": solve FlatT's template on flat facts (as the existing flat solve test does), take `solve.layout`, `BuildJunctionNodeTemplate(*solve.layout, spec)`, and `CHECK(DiffJunctionNodeTemplates(tpl, rebuilt) == "")` (print the diff in INFO). If it is not empty, do NOT loosen the comparator: STOP and report the diff (this is a finding about the render or the tracer).
3. `[nodes]`, `[nodespeed]`, `[nodeoracle]` stay green.

## Do not
- Do not change the template, the tracer, the solve, the render or any existing test. No renaming tolerance (later step).

## Stop conditions
- Test 2 not empty (report the diff, stop without committing test 2; you may commit the comparator and test 1 if green: say so).
- `[nodes]` Release over 1 s.

## Gates
`tools/junction/jt fast build "[nodes]" 5`, `tools/junction/jt fast build-rel "[nodes]" 1`, `tools/junction/jt fast build-rel "[nodespeed]" 5`, `tools/junction/jt gate build-rel`. Update TESTS.md times. Commit: `Add: Junction node template comparator`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; the fields covered per struct and those empty in FlatT; test 2 result (diff text if any); suites with times; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
