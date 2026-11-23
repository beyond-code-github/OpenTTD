# lazy-1: lazy keep-apart rows on T1 and T2 (D444 trial)

Test `[.][lazymeasure]` (planar_domain_encoding.cpp), Release build, one solve at a time, 15 s cap per solve, 12 rounds max. Option `PlanarDrawOptions::only_pairs_on` + `only_pairs` (off by default). Rules: slots-1's (DEFAULT_RULES as the judge builds them). Stage A: uncontracted, integer, grid on, no node gaps, inserts may be 0. Stage B: contracted from stage A's last Optimal drawing (slots-1's mechanism: node gaps and grid on, coord_max from the drawing). "Failing pairs": pairs without rows whose every candidate fails on the round's points; they get rows next round. "Pairs with rows" and "binaries" are the round's (before adding).

Stage B's seed: stage A's pairs mapped to stage B's tags by (track, piece) with merged pieces mapped to the surviving piece, found by one listing solve (no rows, not counted below). A pair that merged with another may seed a few more pairs than A had.


### LAZY T1 offers 1 d443 off (flex pairs 41)

| stage | round | status | ms | pairs with rows | binaries | failing pairs |
|---|---|---|---|---|---|---|
| A | 0 | Optimal | 341 | 0 | 0 | 1 |
| A | 1 | Optimal | 504 | 1 | 4 | 0 |
| B | 0 | Optimal | 8 | 1 | 4 | 12 |
| B | 1 | Optimal | 16 | 13 | 52 | 2 |
| B | 2 | Optimal | 173 | 15 | 60 | 0 |

- LAZY T1 offers 1 d443 off stage A: rounds 2 converged true stop '' pairs with rows 1 of 811 total ms 845
- stage B seed: 1 of stage A's 1 pairs map to stage B tags (listing solve status Optimal, 8 ms, not counted)
- LAZY T1 offers 1 d443 off stage B: rounds 3 converged true stop '' pairs with rows 15 of 245 total ms 197
- judge refuse [crossing piece] [end run] [grid] [keep apart] [length] [node] (without interface rows: judge refuse [crossing piece] [end run] [grid] [keep apart] [length] [node]); domain refuse notes 1 breaks 6 [grid] [node] [turn]
- control, stage B with all rows: status Optimal ms 1396 binaries 980 judge refuse [crossing piece] [end run] [grid] [keep apart] [length] [node]

### LAZY T2 offers 1 d443 off (flex pairs 99)

| stage | round | status | ms | pairs with rows | binaries | failing pairs |
|---|---|---|---|---|---|---|
| A | 0 | Optimal | 2433 | 0 | 0 | 44 |
| A | 1 | Error | 15007 | 44 | 176 | - |
| B | 0 | Infeasible | 30 | 25 | 100 | - |

- LAZY T2 offers 1 d443 off stage A: rounds 2 converged false stop 'Error' pairs with rows 44 of 1945 total ms 17441
- stage B seed: 25 of stage A's 44 pairs map to stage B tags (listing solve status Infeasible, 21 ms, not counted)
- LAZY T2 offers 1 d443 off stage B: rounds 1 converged false stop 'Infeasible' pairs with rows 25 of 667 total ms 30
- control, stage B with all rows: status Infeasible ms 1211 binaries 2668

### LAZY T1 offers 3 d443 on (flex pairs 123)

| stage | round | status | ms | pairs with rows | binaries | failing pairs |
|---|---|---|---|---|---|---|
| A | 0 | Optimal | 1114 | 0 | 0 | 1 |
| A | 1 | Optimal | 1319 | 1 | 4 | 0 |
| B | 0 | Optimal | 8 | 1 | 4 | 9 |
| B | 1 | Optimal | 17 | 10 | 40 | 2 |
| B | 2 | Optimal | 188 | 12 | 48 | 0 |

- LAZY T1 offers 3 d443 on stage A: rounds 2 converged true stop '' pairs with rows 1 of 5279 total ms 2434
- stage B seed: 1 of stage A's 1 pairs map to stage B tags (listing solve status Optimal, 8 ms, not counted)
- LAZY T1 offers 3 d443 on stage B: rounds 3 converged true stop '' pairs with rows 12 of 224 total ms 215
- judge refuse [grid] [keep apart] [length] [node] (without interface rows: judge refuse [grid] [keep apart] [length] [node]); domain refuse notes 0 breaks 3 [grid] [node] [turn]
- control, stage B with all rows: status Optimal ms 1245 binaries 896 judge refuse [crossing piece] [end run] [grid] [keep apart] [length] [node]

### LAZY T2 offers 3 d443 on (flex pairs 345)

| stage | round | status | ms | pairs with rows | binaries | failing pairs |
|---|---|---|---|---|---|---|
| A | 0 | Infeasible | 221 | 0 | 0 | - |

- LAZY T2 offers 3 d443 on stage A: rounds 1 converged false stop 'Infeasible' pairs with rows 0 of 13193 total ms 221
- no stage B (stage A never Optimal)

## Reading

1. Convergence: T1 converges both configs in 2 rounds (A) + 3 rounds (B), 0.9 s + 0.2 s (offers 1, d443 off) and 2.4 s + 0.2 s (offers 3, d443 on). T2 does not: with offers 1, stage A round 1 (44 pairs, 176 binaries) hits the 15 s cap (Error), and stage B started from round 0's no-rows drawing is Infeasible even with no rows (its listing solve), as is the all-rows control; with offers 3 / d443 on, stage A is Infeasible already with no rows (221 ms), so no lazy loop starts.
2. Pairs with rows vs all: T1 stage A 1 of 811 (offers 1) and 1 of 5279 (offers 3); stage B 15 of 245 and 12 of 224. Few pairs ever come too close, so lazy rows cut the binaries from 980/896 (all rows) to 60/48 on T1 stage B and the solve from 1.3-1.4 s to 0.2 s. On T2 offers 1, 44 failing pairs after the first round were already too many for stage A to solve within 15 s (176 binaries): the pairs that fail are the ones whose axis is hard, which is where the binaries cost.
3. Legality: not legal by the judge. On T1 stage B the converged drawing is refused by `DrawingProgramJudge(DEFAULT_RULES)` and the domain, but so is the control (stage B with all rows, same options): the refusals ([grid] [node] [length] [keep apart], plus [crossing piece] [end run] in some, and the domain's [turn]) do not come from lazy rows; the judge was not applied to a free slots drawing before (slots-1 never reached stage 2), and its rows are not the options this test solves with (the judge's keep apart rule set is built from the fixture's options, with the drawing's port zero and along references). Judging without interface rows gives the same. The lazy loop's own check (every written pair holds on its points) passes at convergence by construction; the judge mismatch needs its own look before any legality claim.

Stops: none against the brief. Notes: stage B for T2 offers 1 used stage A's last Optimal drawing (round 0, no rows) because round 1 errored; the judge's verdict is the only place the stage-B result is compared with anything outside the run.


# lazy-2: Slots solves with the judge's rule set (T1)

Stage B now solves with `SlotsOptions(fixture, DEFAULT_RULES)` (the options `DrawingProgramJudge` builds its keep-apart read-out from: the fixture's options plus the rule set's lead, endnode, crosspiece, grid, portzero, strict span; strict zero and crossing flanks off) and the references the judge reads from stage A's contracted drawing: `port_zero_from` = `node_gap_from` = `PortZeroFrom(points)` (travel per track), `along_from` = `AlongFrom`. Stage B solves on the variant the judge reads (steps cleared). `coord_max` is the solve's bound (from the drawing), not a rule. Stage A unchanged. Before the solve the contracted variant is checked with the judge's turn functions (per-track one-eighth, `NodeTurnsHold`): both hold on T1 in both configs, so stage A's S-bends make no illegal writing.

Options the judge uses: clearance, clear_rows, pairwise, pairwise_all_faces, strict_span (strict_zero, crossing_flanks off), node_gaps, bays off, curves off (X1Fixture); node_lead, end_node, cross_piece, grid, port_zero (rule set); references port_zero_from, node_gap_from (both the travel per track), along_from. Not set: corridor (the fixture's choice), tidy (default on), elastic (off), least_extra (0).

Finding: `X1Fixture(vc)` has no corridor family. Every `DrawnFixture` (L1, Y1, P1, ...) sets `options.corridor = true`, and T1 has copies (B1#12/#14, E1#13/#15). With the plain X1Fixture options the solve puts the copies on their parents' node points; the judge refuses [node turn] (F8: B1#12 and B1#14 both leave node 4 at heading 6) and the domain refuses [connection] [node] (11 breaks; first: "a track ends at the node (2, 9) without being listed there", "track 14 is listed in `in` but does not end at (2, 9)"). The same holds for the all-rows control. With `corridor = true` (as the DrawnFixture family), as kept in the test, both agree below.

### lazy-2 T1 offers 1 d443 off

| stage | round | status | ms | pairs with rows | binaries | failing pairs |
|---|---|---|---|---|---|---|
| A | 0 | Optimal | 353 | 0 | 0 | 1 |
| A | 1 | Optimal | 512 | 1 | 4 | 0 |
| B | 0 | Optimal | 6 | 1 | 4 | 0 |

Stage B: 1 round, 1 of 318 pairs with rows, 6 ms. Judge accept (also without interface rows); domain accept (0 breaks). Control (all rows, 1272 binaries): Optimal 251 ms, judge accept. Without the corridor option: judge refuse [node turn], domain refuse [connection] [node].

### lazy-2 T1 offers 3 d443 on

| stage | round | status | ms | pairs with rows | binaries | failing pairs |
|---|---|---|---|---|---|---|
| A | 0 | Optimal | 1119 | 0 | 0 | 1 |
| A | 1 | Optimal | 1330 | 1 | 4 | 0 |
| B | 0 | Optimal | 5 | 1 | 4 | 4 |
| B | 1 | Optimal | 11 | 5 | 20 | 0 |

Stage B: 2 rounds, 5 of 290 pairs with rows, 17 ms. Judge accept (also without interface rows); domain accept (0 breaks). Control (all rows, 1160 binaries): Optimal 234 ms, judge accept. Without the corridor option: judge refuse [node turn], domain refuse [connection] [node].

Reading: the lazy-1 refusals ([grid] [keep apart] [length] [node] [crossing piece] [end run] [turn]) came from the option mismatch (no strict span, no lead/end-node/cross-piece rows, node gaps from piece lengths instead of travel, no port zero, no along references) and, with these options, from the missing corridor. No real gap remains on T1: lazy rows (1 and 5 pairs) give a drawing the judge and the domain both accept. T2 not re-run (not in this brief).
