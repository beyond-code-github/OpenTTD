# S2: S branch fans, with "S edge" per port (D230; REVIEW-port-fans F2, F3, F7; Q4 steps S1 and S2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. Run `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-s2 <head>` with the head given in your launch prompt. Builds `build` (Debug), `build-rel` (Release). Never use git stash.
- Rule: D230 (DECISIONS.md, restated after the review). Review: docs/junction-tool/REVIEW-port-fans.md, F2, F3, F7, Q4.
- Starting point: docs/junction-tool/wip/sf1-stopped.patch (untracked in the main worktree; copy it from /home/pete/OpenTTD/docs/junction-tool/wip/sf1-stopped.patch). It implements briefs/SF1-south-fans.md (read that brief: its Programs and Placement changes, tests 1 and 2 and CLAUDE.md lines stand except where this brief corrects them), with one agreed deviation: the empty-fan join in placement item 4 only applies at the port end (`!edge.from.node` / `!edge.to.node`). Apply it with `git apply` (plain apply worked before); if it no longer applies after S0 (commit a52d47203a changed the same files), apply it by hand to the same effect and say so.
- S0 (heading fix) is in. With S0 and the SF1 patch, both topologies fail on "S edge" (F2): "... S-bend size (edge 12) <- S edge (node 7) <- S edge (node 6) <- ..." and "chain run (edge 3) <- S edge (node 3) <- S port row <- ...".
- The hidden outer test "placement: S branch groups of several lanes place (D230, S0-S2)" (tags `[junction][topology][.]`) exists from S0b.

## Read first
src/junction/CLAUDE.md, src/junction/placement/CLAUDE.md, src/junction/topology/CLAUDE.md, src/tests/junction/CLAUDE.md, D230, REVIEW-port-fans.md (F2, F3, F7), briefs/SF1-south-fans.md, the patch, and junction_placement.cpp lines 239-290 (band chain builder), 1280-1300 ("S edge"), the SBend relations (~1470-1500) and the rounds (~2028-2125).

## Changes on top of the patch
1. **F2, "S edge" per port** (junction_placement.cpp ~1289-1293): replace the two `eq` lines by one floor per layer-1 node: `ge(1, se_pt, node_pt[layer1[i]], JunctionRunFloor(FloorKind::Node, FloorKind::Port, spec));` and drop the "same row as layer1[0]" equality. Comment: "Every layer-1 node of band R lies at least a port stub above the S edge; a node behind a fan lies further, by its chain (D115a, D230)." The chain's exact port runs carry the stub where there is no fan.
2. **F3, an out fan that is its edge's first token**: in the band chain builder, the line that gives an SBend at `t == 0` from a node that is not a band split a new first point (~line 267) also applies to a Fan token: a Fan never starts on its node's point.
3. **F7, fan X floor**: the fan's X relation without `fixed` is `ge` with `el.m` (not 0), as every free S-bend; `el.m` starts at 0 for a fan, so an empty fan is still allowed and m never shrinks between rounds. Keep the rounds' and fixed choices' size without `std::max(1, ...)` for fans.
4. **Unhide** the outer test: remove `[.]` from "placement: S branch groups of several lanes place (D230, S0-S2)" and its "Hidden until S2" comment.
5. Keep the out-port fan hands as the patch has them (halves, same as in ports). Do not flip them.

## Tests
- The patch's two tests (SF1 tests 1 and 2) and the unhidden outer test. Red evidence: the outer test's failure at the current head (record the two texts: they are the "S edge" texts above or close to them).
- Add to the patch's test 1 (L_R x LL_RR): every layer-1 band-R node whose port edge has a fan of m >= 1 columns (from the result spans of the Fan token) lies at least 4 + (m - 1) rows above the S edge row (REVIEW-port-fans S1 check). If no fan has m >= 1, say so.
- Report-only probe: `MakeJunctionGraphSeed` with `MakeTestJunctionSpec()` on L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR (any): error text or "seeded" each. No crash.

## Do not
- Do not change the run floor table, the S port spacing, the bridge-over-S-bend pin, or any row-edge fan code; no fan objective (that is S4).
- Do not change any existing test other than unhiding the outer test.

## CLAUDE.md (same commit)
- The two lines from briefs/SF1-south-fans.md, with the topology line worded: "An S branch port edge starts (in) or ends (out) with a Fan token, on in and out ports alike: the west half of its group opens west, the east half east, the middle lane of an odd group (a one-lane group) has none, so the port block lies between its lanes' inside ends (D230, G1 §10.2)."
- src/junction/placement/CLAUDE.md: add "Every layer-1 node of band R lies at least a port stub above the S edge (D115a)."

## Stop conditions
- Any existing test or identity number changes (one-lane S groups get no fan, and the exact port runs imply the S edge floor, so none may move).
- The outer test still fails after the change: report each error verbatim and stop; do not add further changes.
- Placement does not settle ("the placement does not settle"), or any crash or assert.

## Gates
- Before: `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` (baseline after S0: gate 499 cases, 3042023 assertions; identity 4 cases, 255 assertions).
- After: Debug and Release build with no new warnings (known: `unused variable 'n'` in junction_topology.cpp, `unused function 'TravelY'` in the test file, legacy/junction_nodes.cpp warnings); gate passes with 3 cases added (2 new + the unhidden one); identity unchanged; `tools/junction/jt fast build '[topology]'` within budget.
- One commit: `Add: Junction S branch lanes open up by fans inside the box (D230)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Commit hash and subject; red evidence; counts; the fan sizes found in test 1; the probe's three results; stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
