# PROBE: the elastic joint on LL_RR x LL_RR (Probe E, REVIEW-G4-overprescribed Q3)

Test-only probe at 06f04a8cce. Patch: `docs/junction-tool/wip/probe-ll2-elastic.patch` (test file `src/tests/junction/placement/junction_probe_ll2.cpp`, one line in `src/tests/junction/placement/CMakeLists.txt`). Per-instance result files, layout JSONs and the summary lines: `docs/junction-tool/wip/probe-ll2/`. Run: `PROBE_INSTANCE=<1-5> openttd_test "[.probe]"` from `build-rel` (a hidden test; all five take under 3 minutes in all, mostly the seed starts).

## Method
- The solve is a test-local copy of `SolveJunctionJoint` (the joint's options struct has no flag, and a seam would need a public header). The copy gives one nonnegative continuous slack to each recorded relation row (group = reason up to " ("), each clash row ("occupancy"), each bridge-over-S-bend t row ("bridge over S-bend t") and each curve-gap counter row ("curve gap"). Phase 1 minimises the slack sum with every other cost zero; phase 2 caps the sum at phase 1's value (+1e-6) and minimises the flat cost.
- Rows the joint skips stay skipped (occupancy relations when clashes exist, nonlinear curve gaps, order pins). Link, form and big-M rows have no slack.
- Rounds follow `SolveJunctionJointLoop` (kept clashes, forms kept, the next point is the recording's own or the joint's). The next round is recorded at the regular joint's solution when that is Optimal, else at the elastic one's; so a round 1 after a round 0 with slack is recorded at a point that breaks rules, and its rows are not the loop's own.
- "Flat cost" is the objective less 0.001 sum L. Span = columns x rows of the joint's points.
- Zero slack: place at the joint's choices (`PlaceJunctionTrackGraph` with choices), `RenderJunctionTrackPlacement`, `CheckJunctionRules(layout, spec, topology)`. The render is track only (no signals), so the checker's signal-dependent families are not meaningful; they are listed anyway.
- The drop-one-group variant of the brief (tjj:1297-1298) does not exist at this HEAD (the tjj line numbers differ); it was not run.

## Summary
| Instance | Round | Total slack | Groups with slack (rows, sum) | Span | Places | Checker violations |
|---|---|---|---|---|---|---|
| 1 LL² any, 100100000, port 24 | 0 | 0 | - | 128 x 88 | no (rules contradict, edges 4,5,7,8) | - |
| 1 | 1 | 3 | lane split run (1, 1); occupancy (2, 2) | 128 x 93 | - | - |
| 2 LL² keep (all-floor start) | 0 | 6 | bridge over S-bend (1, 5); bridge heads hug crossers (1, 1) | 54 x 29 | - | - |
| 2 | 1 | 2 | bridge middle clearance (2, 2) | 5 x 31 | - | - |
| 3 keep, gap 7 | 0 | 0 | - | 41 x 14 | yes, 41 x 14 | 82: Signals 72, WaitingRoom 4, Prio 4, Deadlock 1, Footprint 1 |
| 3 | 1 | 0 | - | 41 x 14 | yes | same 82 |
| 4 keep, gap 7, prio_tiles 9 | 0 | 0 | - | 33 x 14 | yes, 33 x 14 | 75: Signals 65, WaitingRoom 4, Prio 4, Deadlock 1, Footprint 1 |
| 4 | 1 | 0 | - | 33 x 14 | yes | same 75 |
| 5 keep, gap 7, single bridge as tunnel | 0 | 0 | - | 41 x 14 | yes, 41 x 14 | 82, as 3 |
| 5 | 1 | 0 | - | 41 x 14 | yes | same 82 |

## Instance 1: LL² any, form 100100000, port offset 24
- Round 0 (relations 1977, no clashes): regular joint Optimal 268600.024 (2.2 s); elastic slack 0 (1.0 s + 2.0 s), same cost. The joint's choices do not place: "S-bend size (edge 4) <- chain run (edge 4) <- S port spacing <- chain run (edge 7) <- ... <- bridge over S-bend (edge 8) <- bridge over S-bend (edge 5) <- chain run (edge 5) <- Corner size (edge 5) ..." (full chain in `i1-any-r0.txt`). So the joint at round 0 is looser than the fixed path.
- Round 1 (recorded at the round-0 joint point; 2878 relations, 460 clashes; the recording's own X solve fails with the same chain): the regular joint is Infeasible (67 ms). Elastic: total slack 3 (14 s + 44 s): "lane split run (edge 44, node 24, row 1)" 1; "occupancy (edge 44)" twice, 1 each. Phase-2 flat cost 275400.024, span 128 x 93.
- Outcome: (b) for round 1 (the slack is in point-recorded occupancy plus one lane split run, small), and round 0 shows the (d) pattern: zero slack in the joint but no placement.

## Instance 2: LL² keep (LL_RR x LL_RR, lane keeps lane)
- The seed attempt fails on this topology ("the placement rules contradict each other"), so the start is the all-floor choices.
- Round 0: regular joint Infeasible (conflict: bridge over S-bend (edge 13) <- lane split run (edge 15, node 4, row 3) x2 <- bridge heads hug crossers (edge 15, row 3) x2). Elastic total slack 6: "bridge over S-bend (edge 13)" 5 (one row), "bridge heads hug crossers (edge 15, row 3)" 1. Flat cost 89350.014, span 54 x 29.
- Round 1 (at the elastic point, which breaks those rows): total slack 2, "bridge middle clearance (edge 15)" 2 rows of 1 each; span 5 x 31 is the degenerate point of that recording (598 relations), not a layout.
- Outcome: (c) at gap 5: the static group "bridge over S-bend" (edge 13, the crossed edge of edge 15's bridge) with the hug of the heads at row 3 binds at all sizes tried (slack 5 at the S-bend's t bound). Instance 3 shows the same topology with gap 7 has no slack, so the contradiction depends on the gap.

## Instances 3, 4, 5: keep at LL_______RR on both lines (gap 7)
- 415 relations, 29 clashes kept, regular and elastic both Optimal (15 ms; elastic 54 ms + 15 ms), slack 0 in rounds 0 and 1 (the round-1 layout is byte-identical to round 0's).
- 3: flat cost 27300.001, span 41 x 14, placed 41 x 14, 82 checker violations. 4 (`prio_tiles = 9`): cost 23500.001, 33 x 14, 75 violations. 5 (the one single bridge set to a tunnel in the cut, programs rebuilt): cost 26700.001 (the tunnel has no price), 41 x 14, 82 violations. Only one single bridge exists at this hub.
- Violations by rule (instance 3): Signals 72 (50 "signals too far apart", 8 "no signal before a merge", 4 before a split, 4 after a merge, 3 before a bridge, ...), WaitingRoom 4 ("no signal between the split and the merge", "no signal before the queue merge"), Prio 4 ("no joiner signal before the ML merge", "missing prio"), Deadlock 1 ("a signal block holds both a split and a merge"), Footprint 1 ("the minimum offsets must be 0" at 0,1). Connectivity, Geometry, CurveSpeed, Order and Bridges: none. The first four families are what a track-only render without signals must show; the Footprint line is the only one that is not about signals and is unexplained here (it may be an origin offset of the track-only render).
- ASCII layouts are in the result files; layout JSON for a rig build: `docs/junction-tool/wip/probe-ll2/i3-keep-gap7-r0-layout.json`, `i4-keep-gap7-prio9-r0-layout.json`, `i5-keep-gap7-tunnels-r0-layout.json` (track only).
- Outcome: (a) on the structural families: a legal-looking placement exists and the joint and the fixed path agree. Whether it is fully clean cannot be read from a track-only render; the signal pass would have to run. (d) is not shown by the structural families.

## Outcomes
| Instance | Outcome |
|---|---|
| 1 | round 0 zero slack but the choices do not place (d-like); round 1 slack 3 in occupancy and one lane split run (b) |
| 2 | (c): static bridge over S-bend (edge 13) with the hug of edge 15's heads, slack 5 and 1 |
| 3 | (a) on structure; remaining violations are signal families of a track-only render, plus one Footprint line |
| 4 | as 3, prio_tiles 9 narrows the box to 33 x 14 |
| 5 | as 3, the tunnel only lowers the cost (600); the one single bridge exists |

## Notes for the engineer
- Gap 5 keep and gap 7 keep differ in more than the gap's size; the all-floor start of instance 2 (the seed fails) is not the seed's start of instance 3. A fair comparison of the gap needs the same start kind.
- The slacks are in coordinate units (columns, rows, bridge t); curve-gap rows are in raster units.

## Correction (orchestrator, 2026-11-14): instances 3-5 rerun at a real gap 7
`ParseLineNotation` refuses a gap over 3 and returns an empty spec, so the first run of instances 3-5 ("LL_______RR") silently built a degenerate hub (flat cost 27300, the T's objective; a two-track branch in the render). Their "zero slack, places" rows above are void. The probe now parses "LL_RR" and sets the gap on the spec (patch updated; worktree commit "real gap 7"). Rerun:

| Instance | Round | Total slack | Slack groups | Span |
|---|---|---|---|---|
| 3 keep, gap 7 | 0 | 6 | bridge over S-bend (edge 13) 5; bridge heads hug crossers (edge 15, row 3) 1 | 54 x 29 |
| 3 | 1 | 2 | bridge middle clearance 2 (recorded at the elastic point) | - |
| 4 keep, gap 7, prio 9 | 0 | 6 | bridge over S-bend 6; lane split run 0 | 47 x 29 |
| 5 keep, gap 7, 4 single bridges as tunnels | 0 | 6 | as 3 (the "bridge over S-bend" row stays although its bridge is a tunnel: INFERRED, the family ignores the tunnel flag) | 54 x 29 |

Reading: LL² keep, at any gap and prio tried, fails on one local conflict: the regular joint's chain is "bridge over S-bend (edge 13) <- lane split run (edge 15, node 4, row 3) <- bridge heads hug crossers (edge 15, row 3)", needing 5-6 columns on edge 13's bridge-over-S-bend row and 1 on the hug. Every other group holds with zero slack (outcome (c), one site). Rerun results: `wip/probe-ll2/rerun-gap7/`.
