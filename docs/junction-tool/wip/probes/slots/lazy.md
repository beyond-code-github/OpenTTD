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


# lazy-3: T2 through the aligned Slots path

`SlotsOptions` now sets `corridor = true` (D421); the one-off line in the lazy test is gone. Stage A solves with `SlotsOptions(X1Fixture(v), DEFAULT_RULES)` (grid on, port zero with no reference = every port line ordinary) with `node_gaps` off (no reference yet); stage B as lazy-2. On every Infeasible round the test now runs the deletion (families of the first conflict, then tags of the last needed family, at most 30 solves). Exactness tests: `[planardomain]` 41 of 41 as before (the two "failed as expected" are the `[!shouldfail]` D384 test); gate: one SIGSEGV in "BR1 is exact under the default rule set" (known intermittent load crash), the test passes alone 3 of 3.

### lazy-3 T2 offers 1 d443 off (flex pairs 99)

| stage | round | status | ms | pairs with rows | binaries | failing pairs |
|---|---|---|---|---|---|---|
| A | 0 | Optimal | 2430 | 0 | 0 | 51 |
| A | 1 | Error | 15019 | 51 | 204 | - |
| B | 0 | Infeasible | 37 | 28 | 112 | - |

- Stage A: 2 rounds, not converged (round 1 hit the 15 s cap with 51 pairs, 204 binaries), 51 of 1945 pairs with rows.
- Stage B starts from stage A round 0's drawing (no rows; round 1 errored). Its contracted variant: per-track turns hold, node turns (F6/F8) FAIL. The listing solve (no keep-apart rows) is Infeasible (25 ms); round 0 with the 28 seeded pairs is Infeasible; the all-rows control is Infeasible (552 ms, 2952 binaries).
- Conflict of stage B with no keep-apart rows (49 rows, 7 families; 10 solves): families needed [piece] [cross] [end] [deck] ([port], [node], [order] drop out). First conflict rows: pieces of CD1, CD2, AF1, A2 2-5, B1 5-7, C2 0/1/4, D1 0-2, D2 0-2, E2 0-1, F1 3-5, F2 2-4; port gaps CD2, AF1, AF2; nodes 2, 3, 5, 6, 7; crossings 5 (B1/F1), 6 (B1/F2), 11 (C2/D1), 14 (D1/F2), 15 (D2/F1), 16 (D2/F2), 17 (E2/F1), 18 (prio:B1/C2); `order D1 3`; spans 2 and 3 (`end 0/2/3`, `deck 2`, `deck 3`). Tag deletion in [deck]: dropping `deck 2` and `deck 3` one after the other stays Infeasible while dropping the whole family is Optimal, so the first conflict's tags do not isolate the deck rows (another deck row takes their place); no tag-level minimum was reached in 10 solves.
- Reading: the conflict is the spans (decks 2 and 3 with their end runs) of the tracks F1 and F2 that B1, D1, D2, E2 (and A2) cross, with the crossing pieces of those tracks as fixed by stage A's contracted writing (piece headings from the S-bend choice) and the nodes at their ends; with those pieces as written the crossers do not fit on the spans. The per-track turn check holds and the node-turn check fails, so stage A's contracted writing is not drawable; the failing node is not named by `NodeTurnsHold` (not isolated).

### lazy-3 T2 offers 3 d443 on (flex pairs 345)

| stage | round | status | ms | pairs with rows | binaries | failing pairs |
|---|---|---|---|---|---|---|
| A | 0 | Infeasible | 227 | 0 | 0 | - |

- Stage A Infeasible with no keep-apart rows (still, with the aligned options). Conflict (39 rows, 5 families; 10 solves): needed [piece] [corridor] [cross]; within [cross]: `cross 12` and `cross 19` (D1 x E2, and D1 x prio:E2, the corridor copy beside E2), with E2 piece 0 and prio:E2 piece 0, the corridor open/close rows of prio:B1 and prio:E2, and D1 pieces 25-36 / C2 pieces 24-35. (`cross 2`, `cross 18` = B1/C2 and prio:B1/C2 drop out once the D1/E2 side is in.) Reading: D1 has to cross both E2 and E2's corridor copy within their single fixed piece each, and the corridor rows keep the copy at the fixed side offset along the parents' chain, so the two crossings cannot both be placed; this is the unchanged D443-on geometry, independent of lazy rows.
- No stage B (stage A never Optimal).

Stops: the brief's step 5 (the side of each added pair fixed from the previous round's drawing) is not run. `sides_from` fixes the side of every pair (not only the added ones), writes plain rows for all of them, ignores `only_pairs` and is incompatible with `keep_apart_out`, so the brief's mechanism does not exist in the options as named; a probe would need a production option (fix only the pairs in a list). Question for the orchestrator: add that option, or run `sides_from` over all pairs as the nearest reading.
