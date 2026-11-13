# S2b: commit S2 as far as it goes, then S-bend tokens name the node they serve (DESIGN-G2 §G2.6, RECON-s2-cycle)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes, branch wt-s2 at ff645a3e61, with an UNCOMMITTED change: the S2 step (briefs/S2-south-fans.md; diff saved at /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/s2-stopped.patch). Check `git diff` matches that patch before you start; if not, stop. Never git stash.
- First rebase the work onto the feature head given in your launch prompt: commit nothing yet; run `git -C /home/pete/OpenTTD/.claude/nodes diff > /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/s2-wip.patch`, `git checkout -- .`, `git checkout -B wt-s2b <head>`, `git apply` the saved diff (by hand to the same effect if it does not apply; say so). The feature head has two commits since: S0b's tests and HH3 (levels, terrain; no overlap expected).
- Read: DESIGN-G2-general-graph.md §G2.6, docs/junction-tool/RECON-s2-cycle.md (whole), src/junction/CLAUDE.md, src/junction/placement/CLAUDE.md, src/junction/topology/CLAUDE.md, src/tests/junction/CLAUDE.md, junction_topology.h `JunctionTrackStep` (~line 205), junction_topology.cpp `ProgramJunctionTrackGraph` band part ("a. Start" to "d. End", ~1138-1200), junction_placement.cpp band chain builder (~255-280).

## Part 1: commit S2 as far as it goes
- In the test file, re-hide the two tests that need placement to succeed (the outer test "placement: S branch groups of several lanes place (D230, S0-S2)" and SF1's test 1 for L_R x LL_RR): add `[.]` to their tags and the comment "Hidden until the S placement steps land (RECON-s2-cycle)." Nothing else changes.
- Gates: Release gate passes (baseline at the feature head: record it before applying, `jt gate build-rel`; expect about 500 cases), identity unchanged (4 cases, 255 assertions), `jt fast build '[topology]'` within budget, Debug and Release build without new warnings.
- Commit: `Add: Junction S branch lanes open up by fans inside the box (D230)` with the Co-Authored-By line below. If a gate fails, stop.

## Part 2: S-bend roles (one more commit)
### Tests (red first; `[junction][topology]`)
1. "programs: a band edge's S-bends name the node they serve (DESIGN-G2 §G2.6)": `MakeJunctionHubTopology(ParseLineNotation("L_R").spec, ParseLineNotation("L_RR").spec)`, build to programs as `PlacementOf` does. Assert: every SBend pushed by "a. Start" (the diverge) has role `Diverge`; every SBend pushed by "c. Arrival on a row" and by "d. End" has role `Converge`; crossing S-bends ("b. Crossings") have role `None`. Concretely for the RECON-s2-cycle edge e12 (straight at split n4, one SBend into merge n7; identify it as the edge from the E in split whose program is exactly one SBend and which ends at a merge): its one token has role `Converge`. Red: does not compile until the roles exist; then with the roles unset record the first failure.
2. "placement: a lone converge S-bend leaves a run after its split (DESIGN-G2 §G2.6)": same topology through `PlacementOf(t, 64, &error)`; assert `error` does not contain "S-bend size (edge 12)" together with "bridge over S-bend (edge 6)" (the +5 cycle of RECON-s2-cycle). Confirm the edge numbers match the recon before writing it (edge 12 = the lone-S-bend edge, edge 6 = the W-in leg bridging e11); if not, use the recon's edges by role and say so. Red: the S2 error text (it contains both).

### Change
1. `JunctionTrackStep::Role` gains `Diverge, ///< SBend: leaves the split the edge starts at; starts on its point (DESIGN-G2 §G2.6).` and `Converge, ///< SBend: joins the merge the edge ends at (a joiner's arrival included); ends on its point (DESIGN-G2 §G2.6).`
2. Programs (band part): the SBend pushed in "a. Start" (`step(StepKind::SBend, side_hand, Step::NO_ROW)`) gets `0, Step::Role::Diverge` as its copy and role; the SBend in "c. Arrival on a row" and the SBend in "d. End" get `Step::Role::Converge`. Crossing S-bends unchanged (None). Nothing else.
3. Placement band chain builder (~line 270): `el.first` is the start point only when `t == 0 && edge.from.node` and the token is not an SBend with role `Converge` (keep the existing exceptions after it: an SBend at t == 0 from a node that is no band split, and the Fan, get a new point). `el.last` is the stop point only when it is the last token, `edge.to.node && to_merge`, an SBend, and its role is not `Diverge`. Comment: "An S-bend is pinned to the node it serves, by its role, never by its position (DESIGN-G2 §G2.6)."
4. Do not change any other use of roles (LaneSplit/LaneMerge lookups compare by equality and are unaffected).

### Probe (report only)
After the change, run the two hidden placing tests explicitly and `MakeJunctionGraphSeed` with `MakeTestJunctionSpec()` for L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR (any); report each error verbatim or "placed"/"seeded". RECON-s2-cycle's hack saw L_R x LL_RR reach "curve gap (edge 17) <- S-bend size (edge 9)" and L_R x L_RR throw `basic_string::_M_replace_aux`; if the exception appears, report the stack or the throwing call (run under gdb with `catch throw`, output to a file) but do not fix it.

### CLAUDE.md (same commit)
- src/junction/topology/CLAUDE.md, after the line beginning "A row change sits next to the feature it serves": "A band edge's S-bend names the node it serves: Diverge (the split it leaves), Converge (the merge it joins, a joiner's arrival included), None (a crossing's) (DESIGN-G2 §G2.6)."
- src/junction/placement/CLAUDE.md, after the band chain heading line: "An S-bend is pinned to a node only by its role (Diverge to its split, Converge to its merge), never by its token position (DESIGN-G2 §G2.6)."

### Stop conditions
- Identity moves: stop and list every changed case with the edge whose S-bend was pinned to both ends (that would be a placed layout that had a rigid lone S-bend; the orchestrator judges it).
- An existing test changes, except a test that asserts exact band programs: such a test may be updated only by adding the new roles to the expected S-bend tokens; list each one.
- A crash in the gate.

### Gates
As Part 1, with 2 cases added. Commit: `Fix: Junction S-bends are pinned to the node they serve, by role`.

Both commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Both commit hashes and subjects; red evidence for both tests; counts per part; identity; the probe results; stops; `git status --porcelain` and `git log -2`.

When you have reported, stop.
