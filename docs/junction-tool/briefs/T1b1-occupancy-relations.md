# T1b-1: the two checked missing relations (nested in-fans, bridge head clear of rows) and the X stretch gate (DESIGN-G1 §14.3; RECON-stretch C1, C3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-t1b1 <HEAD>` (given at launch). Never use git stash.
- RECON-stretch.md (read it whole): stretching placement relations finds two missing relations, both CHECKED by experiment (seeds unchanged in 6 cells, their violations gone, none new): C1 nested in-fans of one heading (X), C3 a single bridge's head under a doubled leg's rows (Y). Their scratch code is in /home/pete/OpenTTD/docs/junction-tool/wip/st1a-experiment.patch (the `props & 1` and `props & 2` blocks): you make them permanent, always on, without the switch.
- Terrain will push points later than the least solution (T2). This step's gate pushes them by hand: a test seam that T2's facts replace.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md (`[topology]`, `[topologysweep]` rows); RECON-stretch.md; the patch's two blocks (~lines 50-100).
- src/junction_topology.h PlaceJunctionTrackGraph (both overloads); src/junction_topology.cpp the placement helper: `why` / reasons, the waiting-room block (C1 goes before it), the single bridge Heads block (C3), `solve`, the fixed branch.
- src/tests/junction_topology.cpp the `[topologysweep]` tests, SamePlacement.

## Tests (record each red)
1. **Outer, new tag `[topologystretch]` "topology: pushing any point later keeps the seed legal"**: T and LL_RR at the test spec. Seed p = CutJunctionTrackBridges' placement with its cut structure. For every placement point v (0 < v < point count) and s in {1, 2}: q = the fixed-choices overload with x_floors {(v, p's column of v + s)}. If q fails, count it (INFO the reason chain's first entry; a failure means an edit is needed, not a bug). If q succeeds: render, signal tokens and prio element with the test spec, CheckJunctionRules: REQUIRE empty (INFO cell, v, s and each violation). Report counts: runs, solve failures, clean. Red: RECON-stretch's C1 class (nested in-fans, LL_RR e0/e1 or e5/e6) violates before the change (record which v, s).
2. **`[topology]` "topology: the relations keep the seed"**: T and LL_RR at the test spec: the placement with the change equals the stored seed numbers already pinned by the existing placement tests (they stay green unedited), and the circuit file regenerates byte-identical.
- Every existing test stays green unedited; `[topologysweep]` stays green.

## Changes
1. **The seam:** the fixed-choices overload of PlaceJunctionTrackGraph gains a last-but-error parameter `const std::vector<std::pair<int, int>> &x_floors = {}` (doc: `(point, least column)` pairs added as lower bounds in the X phase, reason "x floor"; a test seam until terrain facts (DESIGN-G1 §14.4)). Points are the placement's internal ids, as `choices.hug` uses. Add `int points = 0; ///< The number of placement points (for x_floors).` to JunctionTrackPlacement, set by the helper.
2. **C1 (X phase, before the waiting room):** the patch's `props & 1` block, unconditional, reason "in fan order" with (edge eb, row rb).
3. **C3 (Y phase, the single bridge Heads block):** the patch's `props & 2` block, unconditional, reason "bridge head clear of rows" with (edge e, row r). It assumes the doubled leg's rows lie north of the head: if for some (head, doubled leg) the seed has the head north of the leg's rows, return a placement failure with build_why "a bridge head north of a doubled leg is not ruled yet" (the existing "not ruled yet" pattern) instead of the mirrored relation; if that fires on T or LL_RR, STOP.
4. TESTS.md: a `[topologystretch]` row (not in fast or per-step runs; gate tag; its Release time; budget 60 s Release), `[topology]` row "T1b-1" and the count.

## Do not
- No C2, C4, bridge group or hug change; no Y stretch test (RECON-stretch: Y stretches are row edits and have open classes C2, C4 and prio, a later step).
- No change to the rounds, the floors, the render, the checker.

## Stop conditions
- Test 1 finds a violation after the change (report each distinct one: v, its reason chain, rule, tiles, edges) — this is a finding, do not add relations for it; any seed number changes; C3's north case fires; an existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test "[topologysweep]"` and `build-rel/openttd_test "[topologystretch]"` (outputs to files); `tools/junction/jt gate build-rel` (416 before; report the count).
- One commit: `Add: Junction placement relations for nested in-fans and bridge heads clear of doubled rows (RECON-stretch C1, C3)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus test 1's counts (runs, solve failures, clean) per topology and its red (v, s, violation); `[topologystretch]` Release time. When you have reported, stop.
