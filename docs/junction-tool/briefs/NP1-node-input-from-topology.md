# NP1: the node template's input traced by a topology (projection of the G1 seed, step 1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-np1 <HEAD>` (given at launch). You are the only writer there. Never use git stash.
- Goal (engineer): project the G1 seeds (T, LL_RR) onto terrain with the node template and solve (junction_nodes.{h,cpp}). RECON-seed-projection.md (read §1-§2): the T seed's template equals the traced T's; LL_RR fails because TraceJunctionNodeInput uses the T's port roles, and the builder has T-role code. This step makes the input and the builder role-free. The next step (NP2) fixes "path moves both ways along y"; do not touch it.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]`, `[nodes]` rows); RECON-seed-projection.md §1-§2.
- src/junction_nodes.h :100-140 (JunctionNodePath, JunctionNodeInput, TraceJunctionNodeInput, BuildJunctionNodeTemplate).
- src/junction_nodes.cpp: IsMainLinePath (:111) and its uses (:360, :538); rule 5 (:528); TraceJunctionNodeInput and BuildJunctionNodeTemplate (:656-690).
- src/junction_rules.cpp :140-172 (IsThroughPath, IsExitPath, SameLanePath); src/junction_rules.h :35-62; src/junction_rules_internal.h (where shared internal declarations live).
- src/tests/junction_topology.cpp: SignalledOf and "the LL_RR seed is clean".

## Tests (tag `[topology]`, in src/tests/junction_topology.cpp; record each red)
1. **Outer, "topology: the LL_RR seed traces into the node template"**: layout = SignalledOf on LL_RR (as "the LL_RR seed is clean"); tpl = BuildJunctionNodeTemplate(TraceJunctionNodeInput(layout, spec, JunctionLLRRSidelineTopology()), spec). REQUIRE tpl.error starts with "path " and ends with " moves both ways along y" (the next step's frontier; INFO the error). Red: first add the new overload as a declaration plus a body that returns `TraceJunctionNodeInput(flat, spec)` (the legacy trace); the test then fails with "bridge 0: a head is not a piece of any path" (record it).
2. **"topology: the T seed traces the same by its topology"**: layout = SignalledOf on the T. DiffJunctionNodeTemplates(BuildJunctionNodeTemplate(TraceJunctionNodeInput(layout, spec, JunctionTTopology()), spec), BuildJunctionNodeTemplate(layout, spec)) is empty. (Green from the stub on; it guards the change.)
- Every existing `[nodes]`, `[nodespeed]`, `[topology]` test stays green and unedited.

## Changes
1. **Export the path helpers.** Make IsThroughPath and IsExitPath (junction_rules.cpp) non-static and declare them in src/junction_rules_internal.h with their existing doc comments (move the comments to the declarations; keep a one-line comment at the definitions only if the file's style does so elsewhere). Do not change their bodies.
2. **JunctionNodeInput** gains, after `paths`:
   - `std::vector<bool> through; ///< Per path: a through movement (IsThroughPath).`
   - `std::vector<bool> exit; ///< Per path: an exit, main line to a group that is not (IsExitPath).`
3. **The trace.** Add, declared after the legacy one:
```cpp
/** The template input traced from a flat layout whose ports and movements come from a topology (G1d). */
JunctionNodeInput TraceJunctionNodeInput(const JunctionLayout &flat, const JunctionSpec &spec, const JunctionTopology &topology);
```
   Body: the legacy body with `const JunctionCheckContext ctx = MakeJunctionTopologyCheckContext(flat, spec, topology, FindJunctionPorts(flat, topology));`, and per path `input.through.push_back(IsThroughPath(ctx, path)); input.exit.push_back(IsExitPath(ctx, path));`. The legacy overload becomes `return TraceJunctionNodeInput(flat, spec, JunctionTTopology());`. If that makes any existing test fail, STOP (report which; the legacy context and the topology context differ for the T).
4. **The builder, role-free** (every other line unchanged):
   - Prio (:360): `IsMainLinePath(path)` -> `this->input.through[pi]`; the joiner test `is_prio = this->input.paths[qi].from != path.from || this->input.paths[qi].to != path.to;` -> `is_prio = this->input.paths[qi].movement != path.movement;` (a lane merge of one movement is not a prio, RECON-prio-element §1).
   - Rule 5 (:528): the role condition -> `this->input.exit[pi]` (use the loop's path index variable).
   - Rule 6 (:538): `!IsMainLinePath(path)` -> `!this->input.through[pi]`.
   - Delete IsMainLinePath.
   - The anchor (BuildJunctionNodeTemplate, "Rule 5: the class of the last piece of the EIn->SOut path"): the anchor path becomes the first path in input order with `input.exit[i]`. Keep the comment's meaning: "the class of the last piece of the first exit path (for the T, EIn->SOut)". If test 2 then shows a different anchor for the T, STOP and report the T's path order (which exit path comes first).
   - Leave NodeAnchorShift, SearchJunctionNodes, JunctionNodeHeightsOk and the signal pass as they are (later steps).

## Do not
- Do not touch dir/dir_y or the "moves both ways" checks (NP2). Do not change JunctionNodePath, the solve, render, search, GenerateJunctionNear, or any checker rule.

## Stop conditions
- An existing test fails; test 1's error is not the y-direction one after the change (report the error); test 2 not empty (report the diff); anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[nodes]' 5`; `tools/junction/jt gate build-rel` (388 before; report the count).
- One commit: `Change: Junction node template input traced by a topology`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "NP1" and the new count (39).

## Report
The standard report (references read; commit; red evidence; types and members added or removed; suite results; stops; `git status --porcelain` and `git log -1`), plus LL_RR's error text after the change (which path). When you have reported, stop.
