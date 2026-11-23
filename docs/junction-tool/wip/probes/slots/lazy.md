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


# lazy-4 Part B: the offers 3 + D443 infeasibility is Shape's (pinned, fixed)

Diagnosis (T2, flex placed on the unflexed variant; `build-rel`, one solve at a time, stage A round 0 = no keep-apart rows):

| offers | D443 | stage A round 0 |
|---|---|---|
| 1 | off | Optimal, 2.4 s (lazy-3) |
| 3 | off | no result in 15 s (Error, 17.9 s): not Infeasible |
| 1 | on | Infeasible, 72 ms; conflict [piece] [corridor] [cross]: `cross 2` (B1 x C2), `cross 12` (D1 x E2), `cross 18` (prio:B1 x C2), `cross 19` (prio:E2 x D1); needed tags `cross 12`, `cross 19` |
| 3 | on | Infeasible, 221 ms (lazy-3) |

So D443 alone causes it, with any offers. Pieces: E2 (chain of prio:E2) and prio:E2 have one piece and no flex, as the beside-parent rule says (the chain gets none, `FreeOnePieceStretch`'s last clause holds); the chain/copy pieces still match, `CheckPlanarShapePromise` reports no H8 breach (and with D443 on it stops at H1 first, as it checks flex against the default channel clause). The flex that breaks it is on the crosser: D1 crosses E2 (crossing 12, in D1's structure) and prio:E2 (crossing 19, 12's mirror, so in the structure's copy span) with flex between them (event pieces 25 and 37 at offers 3: six flex pairs; 9 and 13 at offers 1), and the same for C2 between B1 (crossing 2) and prio:B1 (crossing 18). The stretch between a crossing and its own copy's crossing was kept un-free only by the channel clause (a track and its copy are on one channel); the structure clause tests "both in the structure" or "both in its copy span", and misses one in each. D443 turns the channel clause off, so the stretch got S-bends the corridor rows (the copy at a fixed side offset along the chain) cannot follow: the crosser would have to cross the two lanes on different pieces.

Test: "a beside track's parent chain gets no S-bend offers its copy cannot follow (D421, D442)" (`[planarshape]`, T2, offers 3, D443 on): chain and copy have no flex, and for every crosser of both a crossing and its copy's crossing at the same place, adjacent in the crosser's events, no flex between them and both on one piece. Red before the fix: C2 (B1/prio:B1) and D1 (E2/prio:E2) have 6 flex pairs each between the two crossings and sit on pieces 24 to 36 and 25 to 37; 4 of 25 assertions failed.

Fix (`FreeOnePieceStretch`): a stretch between a crossing and its mirror (its beside copy's crossing of the same crosser) is not free, whatever the channel clause says. Green; `[planarshape]` 39 of 39. Stage A round 0 on T2 offers 3 + D443: Optimal, 5.7 s, 169 pairs fail on its points (round 1 with those 169 pairs, 676 binaries, hits the 15 s cap). Stage B from that drawing is still Infeasible with no keep-apart rows (conflict families: 6; node turns F6/F8 fail on the contracted variant, as in lazy-3 offers 1): a separate, Slots-side question, not addressed here.


# lazy-4 Part A: fixed sides for lazily added pairs (D444 trial)

Option `PlanarDrawOptions::fixed_pairs` (new: `forced_sides` needs `sides_from`, which fixes every pair and ignores `only_pairs`): in binary mode a listed pair writes its one candidate (axis*2+side) as plain rows, no binary, no `sepdir`, even with `only_pairs_on` and not in `only_pairs`. The lazy loop adds a failing pair FIXED to the candidate with the largest margin on that round's points; an Infeasible round with fixed pairs frees the fixed pairs its `result.conflict` names (to binaries) and solves again (that solve is a round). Table columns: pairs fixed / free are the round's (before adding), freed = fixed pairs moved to free after an Infeasible round. Stage B's seed pairs from stage A start free. Release, one solve at a time.


### LAZY T1 offers 1 d443 off (flex pairs 41)

| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |
|---|---|---|---|---|---|---|---|---|
| A | 0 | Optimal | 177 | 0 | 0 | 0 | 1 | - |
| A | 1 | Optimal | 225 | 1 | 0 | 0 | 0 | - |
- stage B contracted variant: per-track turns hold, node turns (F6/F8) hold
| B | 0 | Optimal | 8 | 0 | 1 | 4 | 4 | - |
| B | 1 | Optimal | 23 | 4 | 1 | 4 | 0 | - |

- LAZY T1 offers 1 d443 off stage A: rounds 2 converged true stop '' pairs with rows 1 of 811 total ms 402
- stage B seed: 1 of stage A's 1 pairs map to stage B tags (listing solve status Optimal, 7 ms, not counted)
- LAZY T1 offers 1 d443 off stage B: rounds 2 converged true stop '' pairs with rows 5 of 318 total ms 32
- judge accept (without interface rows: judge accept); domain accept notes 0 breaks 0
  - node 0: AB1@end(24,15) h6 A1@start(24,15) h5 B1@start(24,15) h6
  - node 1: CD1@end(15,20) h-1 C1@start(15,20) h-1 D1@start(15,20) h0
  - node 2: EF1@end(10,3) h2 E1@start(10,3) h2 F1@start(10,3) h3
  - node 3: AF1@start(21,22) h5 A1@end(21,22) h5 F1@end(21,22) h4
  - node 4: CB1@start(8,13) h6 C1@end(8,13) h-1 B1#12@end(8,13) h6 B1#14@end(10,11) h6
  - node 5: ED1@start(22,3) h2 D1@end(22,3) h1 E1#13@end(22,3) h2 E1#15@end(20,1) h2
  - node 6: B1@end(14,13) h6 B1#12@start(14,13) h6 B1#14@start(12,11) h6
  - node 7: E1@end(16,3) h2 E1#13@start(16,3) h2 E1#15@start(18,1) h2
- control, stage B with all rows: status Optimal ms 482 binaries 1272 judge accept

### LAZY T1 offers 3 d443 on (flex pairs 123)

| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |
|---|---|---|---|---|---|---|---|---|
| A | 0 | Optimal | 870 | 0 | 0 | 0 | 0 | - |
- stage B contracted variant: per-track turns hold, node turns (F6/F8) hold
| B | 0 | Optimal | 7 | 0 | 0 | 0 | 4 | - |
| B | 1 | Infeasible | 8 | 4 | 0 | 0 | - | 2 |
| B | 2 | Infeasible | 9 | 2 | 2 | 8 | - | 1 |
| B | 3 | Optimal | 8 | 1 | 3 | 12 | 4 | - |
| B | 4 | Optimal | 8 | 5 | 3 | 12 | 0 | - |

- LAZY T1 offers 3 d443 on stage A: rounds 1 converged true stop '' pairs with rows 0 of 5279 total ms 870
- stage B seed: 0 of stage A's 0 pairs map to stage B tags (listing solve status Optimal, 7 ms, not counted)
- LAZY T1 offers 3 d443 on stage B: rounds 5 converged true stop '' pairs with rows 8 of 318 total ms 41
- judge accept (without interface rows: judge accept); domain accept notes 0 breaks 0
  - node 0: AB1@end(24,15) h6 A1@start(24,15) h5 B1@start(24,15) h6
  - node 1: CD1@end(15,20) h-1 C1@start(15,20) h-1 D1@start(15,20) h0
  - node 2: EF1@end(10,3) h2 E1@start(10,3) h2 F1@start(10,3) h3
  - node 3: AF1@start(21,22) h5 A1@end(21,22) h5 F1@end(21,22) h4
  - node 4: CB1@start(8,13) h6 C1@end(8,13) h-1 B1#12@end(8,13) h6 B1#14@end(10,11) h6
  - node 5: ED1@start(22,3) h2 D1@end(22,3) h1 E1#13@end(22,3) h2 E1#15@end(20,1) h2
  - node 6: B1@end(14,13) h6 B1#12@start(14,13) h6 B1#14@start(12,11) h6
  - node 7: E1@end(16,3) h2 E1#13@start(16,3) h2 E1#15@start(18,1) h2
- control, stage B with all rows: status Optimal ms 482 binaries 1272 judge accept

### LAZY T2 offers 1 d443 off (flex pairs 99)

| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |
|---|---|---|---|---|---|---|---|---|
| A | 0 | Optimal | 2373 | 0 | 0 | 0 | 51 | - |
| A | 1 | Infeasible | 42 | 51 | 0 | 0 | - | 7 |
| A | 2 | Optimal | 1671 | 44 | 7 | 28 | 15 | - |
| A | 3 | Infeasible | 44 | 59 | 7 | 28 | - | 2 |
| A | 4 | Optimal | 1937 | 57 | 9 | 36 | 12 | - |
| A | 5 | Infeasible | 45 | 69 | 9 | 36 | - | 2 |
| A | 6 | Optimal | 3123 | 67 | 11 | 44 | 1 | - |
| A | 7 | Infeasible | 433 | 68 | 11 | 44 | - | 1 |
| A | 8 | Infeasible | 2153 | 67 | 12 | 48 | - | 0 |
- LAZY T2 offers 1 d443 off A round 8 conflict rows 0:
- LAZY T2 offers 1 d443 off A conflict: first solve Infeasible (conflict rows 0, families 0)

  - families needed:
- stage B contracted variant: per-track turns hold, node turns (F6/F8) FAIL
| B | 0 | Optimal | 150 | 0 | 49 | 196 | 18 | - |
| B | 1 | Infeasible | 19 | 18 | 49 | 196 | - | 8 |
| B | 2 | Infeasible | 20 | 10 | 57 | 228 | - | 1 |
| B | 3 | Infeasible | 37 | 9 | 58 | 232 | - | 2 |
| B | 4 | Infeasible | 40 | 7 | 60 | 240 | - | 1 |
| B | 5 | Infeasible | 64 | 6 | 61 | 244 | - | 0 |
- LAZY T2 offers 1 d443 off B round 5 conflict rows 0:
- LAZY T2 offers 1 d443 off B conflict: first solve Infeasible (conflict rows 0, families 0)

  - families needed:

- LAZY T2 offers 1 d443 off stage A: rounds 9 converged false stop 'Infeasible' pairs with rows 79 of 1945 total ms 11826
- stage B seed: 49 of stage A's 79 pairs map to stage B tags (listing solve status Optimal, 21 ms, not counted)
- LAZY T2 offers 1 d443 off stage B: rounds 6 converged false stop 'Infeasible' pairs with rows 67 of 1032 total ms 332
- control, stage B with all rows: status Infeasible ms 1237 binaries 4128

Verdicts. T1 (both configs, regression): converges in both stages; judge accept (also without interface rows), domain accept (0 breaks), control accepts; offers 3 + D443 stage B needs two freeing rounds (4 fixed, conflict named 2, then 1) before it converges. T2 offers 1: stage A does not converge: each round's new pairs make the next Infeasible (7, 2, 2, 1 pairs freed), 9 rounds / 11.8 s, and round 8 (67 fixed, 12 free, 48 binaries) is Infeasible with an EMPTY conflict (the MILP proves it by branching, there is no row set to name), so nothing is freed and the loop stops; stage B, started from stage A's last Optimal drawing (round 6), does the same (round 5, 61 free, empty conflict); the all-rows control is Infeasible (4128 binaries). No judge or domain verdict for T2 (never converged). Compared with lazy-1/3 (round 1 with 51 free pairs, 204 binaries, hit the 15 s cap): the fixed rounds are fast (1.7 to 3.1 s Optimal, Infeasible in 0.04 to 2.2 s), but fixing from the previous points is wrong for a part of the pairs on every round (about 15 of the 51, then 2 to 12 more each time), and the freed ones cannot be found once the conflict is empty.
