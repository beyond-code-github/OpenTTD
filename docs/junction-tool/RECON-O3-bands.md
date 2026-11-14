# Recon O3: band occupancy, segments (A4), event order and clearance before 2b2b

Read-only recon at 122e9be16b (worktree `.claude/o4c2`, branch `wt-recon-o3`, reverted clean). Facts are read from dumps of three seeds at `system.solved` (T, LL_RR x L_R "LLRR_LR", L_R x LL_RR "LR_LLRR"; all three have every crossing in the S-bend form, form 0) and of LL² at form 100100000. Anything not read or run is marked INFERRED. Probe patch: `docs/junction-tool/wip/recon-o3-probe.patch` (applies to 122e9be16b). Dumps and the Python used: `docs/junction-tool/wip/recon-o3/` (`dump-*.txt`, `q*.py`, `seg*.py`; run from that directory).

**Method.** The probe adds a text label to each occupant and a `probe` string to each recorded clash (jp:1938), dumps every occupant, item row, static row, relation and the arrangement, and a test writes one dump per seed. Occupancy pairs = `system.clashes` (the flat order's adjacent groups, jp:1932-1997). "On an item row" = both occupants are in `item_rows` of the row at that y (chosen crosser pieces only). "Band-edge piece" = a piece of a non-row edge's chain (Corner, SBend, Fan, Tile, Heads, vertical or horizontal run). Event rows: node = its point's row; band crossing rho = north head row + 1 (S-bend form, DESIGN §11.4; INFERRED for the square form); row crossing (Under pair of a doubled bridge) = the crossed item row (from `crosser_pieces`).

## Findings the orchestrator should act on first
1. All band-edge vs band-edge occupancy pairs at the seeds are neighbour pairs of A2: 0 non-neighbour pairs (T 21, LLRR_LR 53, LR_LLRR 128; LL² at its joint optimum 453). Nothing lies between them.
2. O3's domain is exactly the non-item rows (class D below). Band-edge pairs on item rows are already O1 rows (crossers' pieces).
3. Band-edge pieces also clash with row pieces, nodes and prio tiles outside item rows (class D "other") and with other rows' W/E port fans on item rows (class C). No G4 family states these (O1 skips them, O2 is fan against fan, O3 is band against band): LLRR_LR 18, LR_LLRR 40, T 1 (node/node). They are far apart in columns at the seeds (INFERRED to stay so); the checker is the only oracle today.
4. `BandNeighbour` (th:288) carries no begin/end events, though DESIGN §1 A2 says it does. I derived them from `order.band_lines` (Q5).
5. O3 (i) read literally (one run column per segment, segments cut only at events) fails at some seeds: a run-less segment whose far bend end lies outside the rows where the pair coexists, and segments with two vertical run columns. With c taken at the coexisting rows it holds everywhere (Q4).
6. Free S-bends straddle row-crossing events (Under pairs of a doubled bridge); at LL² two free S-bends straddle another edge's band-crossing row (Q3).
7. The item-row recorder (jp:1690, `assert(last >= 1 && travel[r][0].node && travel[r][last].node)`) aborts at LL² when recorded at a point: its rows 1-3 end with crossing edges after the last node (`ROW 1: n2 e24 e26 n24 e14 e15`). Not hit at the seeds. 2b2b's LL² work needs it (I bypassed it in the probe only).
8. Clearance Y: sign always equals the band side; some rows are not implied by other unconditional Y rows (Q7).

## Q1. Band occupancy today
Classes: A = both on an item row (O1 rows exist); B = O2 end row of a fan pair; C = on an item row's y but not both item-row occupants; D = not on an item row. "bb" = band-edge vs band-edge of two different edges.

| hub | clashes | A | B | C | D | bb total | bb in A | bb in C | bb in D |
|---|---|---|---|---|---|---|---|---|---|
| T | 30 | 14 | 0 | 0 | 16 | 21 | 6 | 0 | 15 |
| LLRR_LR | 109 | 50 | 4 | 17 | 38 | 53 | 22 | 0 | 31 |
| LR_LLRR | 180 | 21 | 0 | 3 | 156 | 128 | 10 | 0 | 118 |
| LL² square form (joint optimum; item rows 1-3 skipped by the probe, so A and C are not reliable) | 689 | 72 | 3 | 40 | 574 | 453 | 31 | 11 | 411 |

Already implied by static rows (A + B): T 14 of 30, LLRR_LR 54 of 109, LR_LLRR 21 of 180. Every D clash lies in band rows (outside the stack's item-row range); none lies between item rows.

D band-edge vs band-edge, by kind of the two pieces:
- T 15: SBend/run 3, Corner/run 2, Heads/run 2, Corner/SBend 2, run/run 2, Corner/Heads 1, Heads/SBend 1, SBend/SBend 1, Tile/Tile 1.
- LLRR_LR 31: SBend/run 12, run/run 10, SBend/SBend 3, Corner/run 2, Heads/SBend 2, Heads/run 1, Tile/Tile 1.
- LR_LLRR 118: run/run 58, SBend/run 39, Heads/run 6, Heads/SBend 4, SBend/SBend 4, Corner/run 3, Tile/Tile 3, Corner/Corner 1.
- No Fan piece of a band edge occurs at the seeds.

D, not band against band (nothing in G4 covers it): T 1 (node/node); LLRR_LR 7 (run/node 2, SBend/node 1, Heads/node 1, SBend/row-bend 1, Corner/row-bend 1, node/node 1); LR_LLRR 38 (band run/SBend/Corner/Heads against W/E port-fan bends on the rows between port rows and item rows 31, band against node 6, node/row-bend 1; e.g. y 3-19, edge 0's and edge 20's fans against edges 1, 21, 11). C (item-row y, not O1): LLRR_LR 17 (band Corner/run/SBend or fan against another row's fan or heads 12, fan/fan 2, node/prio 1, other 2), LR_LLRR 3.

## Q2. Neighbour pairs (`arrangement.band_neighbours`; `x` = extension-only)
- T, band 1: 4|8. Band 2 (9): 1|4x 1|10 4|6 4|8 7|4 7|8 8|4 8|6 8|10. Total 10, 1 extension-only.
- LLRR_LR, band 1: 7|13. Band 2 (4): 7|3 7|13 8|7 13|14. Band 3 (4): 3|17 7|3 8|7 11|8. Band 4 (19): 2|21 2|23x 3|17 3|21 3|22 3|23 4|3 4|8x 4|23x 7|3 8|7 11|3 11|8 11|23 12|11 17|18 23|3 23|17x 23|22. Total 28, 4 extension-only.
- LR_LLRR, band 1 (7): 1|21 6|7 11|12 12|1x 12|6 21|1 21|6. Band 2 (24): 2|3 2|10x 3|6x 3|18 6|7 6|14 7|5 7|14 7|15 9|10 9|11 10|11 10|12 11|3 11|10 11|12 12|6 12|18 14|7 14|15 14|19 17|11 18|7x 18|19. Total 31, 4 extension-only.
- LL² (square form recording): 99 pairs, 14 extension-only.
- Band-edge clashes between non-neighbours: 0 at all hubs (checked in the band of the clash row against that band's pairs; in item rows against the pairs of any band). Pairs listed both ways (a|b and b|a) are the two phases around their crossing.

## Q3. Segments (A4)
- Band edges: T 6, LLRR_LR 14, LR_LLRR 16, LL² 36. Boundary events found: T 1 band crossing, 4 Under tokens of doubled bridges (2 pairs); LLRR_LR 1 band crossing, 20 Under tokens; LR_LLRR 3 band crossings, 8 Under tokens; LL² 9 band crossings, 40 Under tokens; plus a start and an end per edge (node or port). The full per-edge event and piece table of T is in the dump (`python3 seg2.py T`); e.g. edge 4: start y1, Corner rows 2-3, row crossings y3 and y4, run rows 4-6, band crossing y7 with the S-bend rows 7-9, run row 10, end y11.
- Segments cut at events inside the band's rows: T 8, LLRR_LR 18, LR_LLRR 25 (7 / 15 / 22 have one vertical run column, 1 / 3 / 1 are run-less, 0 / 0 / 2 have two run columns).
- Bends straddling an event row (strictly inside the bend's rows): T 0. LLRR_LR 4 instances: one free S-bend (edge 8, rows 7-16) over 4 row-crossing events. LR_LLRR 4 instances: two are the own crossing's S-bend (edges 1 and 14), one free S-bend (edge 6) over 2 row-crossing events. LL² 6 instances: 2 own-crossing S-bends, 2 free S-bends over a row crossing, 2 free S-bends over another edge's band-crossing row (edge 17 at y64, edge 39 at y79; rho INFERRED there). So bends do straddle events other than a crossing's own S-bend: Under pairs always, a band crossing at LL². Nothing straddles a node or port event.
- Coexisting segment pairs of neighbour pairs, at the seed (rows where the pair is adjacent in the seed's line, band rows only; variant A): T 9, LLRR_LR 18, LR_LLRR 30. Pairs with no adjacent coexisting band row: T 3 (1 only in the stack = item rows, 1 extension-only, 1 disjoint), LLRR_LR 12 (6 only in item rows, 2 extension-only, 2 disjoint, 2 non-adjacent in the seed line), LR_LLRR 5 (1 item rows only, 1 extension-only, 3 disjoint). Variant B (all rows, a physically west of b): T 26, LLRR_LR 130, LR_LLRR 69.

## Q4. O3 at the seeds (values, from tiles; g = 1; INFERRED reading of DESIGN §3)
c is the segment's vertical run column (west edge: the largest, east edge: the smallest), or for a run-less segment its tile column facing the partner. For (ii) A(rho) and B(rho) are the physical tile columns of a and b at the boundary row: the bend lines extended beyond a bend's far end move away from the partner (DESIGN §3), so they cannot bind.

| | T | LLRR_LR | LR_LLRR |
|---|---|---|---|
| A: pairs evaluated (of neighbour pairs) | 7 of 10 | 16 of 28 | 26 of 31 |
| A: (i) segment pairs, hold / fail | 8 / 1 | 18 / 0 | 28 / 2 |
| A: (ii) boundary rows, hold / fail | 4 / 0 | 5 / 0 | 14 / 0 |
| B: pairs evaluated | 8 | 24 | 27 |
| B: (i) hold / fail | 24 / 2 | 125 / 5 | 67 / 2 |
| B: (ii) hold / fail | 16 / 0 | 86 / 0 | 46 / 0 |
| (i) with c taken at the coexisting rows, hold | 9 of 9 | 18 of 18 | 30 of 30 |

Why (i) fails: T (7 west of 4: edge 7's run-less Corner segment rows 5-11 has its facing end at column 25, but edge 4's segment coexists only on rows 6-7 where the Corner is near column 21; 8 west of 6: the same with edge 8's Corner end 38). LLRR_LR (edge 8's run-less S-bend segment rows 11-16, facing end 28, against a run at 29 on rows 12-14). LR_LLRR (segments of edges 2 and 19 with two run columns). All are segments cut only at events; §11.4 says segments are cut at rows, which these cases need.

Bends approaching a partner other than the pair's other edge (INFERRED rule: a bend of a pair edge whose rows contain or touch an event row; partner = the crossing partner, the node's other edges, or a row bridge): T 17 of 22 instances, LLRR_LR 51 of 61, LR_LLRR 48 of 64. By event type of those: row crossing 12 / 40 / 14, band crossing 3 / 5 / 9, node start 1 / 2 / 14, node end 1 / 4 / 11.

## Q5. Event order
- `band_event_pairs` (EVPAIR): T 1, LLRR_LR 7, LR_LLRR 4, LL² 21.
- At the seed, y of the two events: T (N1, N4) same row. LLRR_LR: 2 first more south, 4 first more north than the second (against the sweep's index order), 1 same row. LR_LLRR: 1 first south, 3 first north. LL² at its optimum: 7 south, 13 north, 1 same. (Event rows are not monotone in sweep index within a layer, e.g. LLRR_LR band 4: 22, 15, 22, 18, 16, 17.)
- Neighbour pairs whose coexistence begins or ends at an event that is in an event pair, or that are extension-only (so their O3 rows would be conditional on event-order binaries; begin/end events INFERRED from `band_lines`): T 6 of 10 (5 + 1 extension-only), LLRR_LR 19 of 28 (15 + 4), LR_LLRR 22 of 31 (18 + 4), LL² 71 of 99 (57 + 14). The rest begin and end at events fixed by the order (4 / 9 / 9 / 28).

## Q6. Square form (LL² at 100100000)
Recorded at the joint optimum (the hub has no flat solve; `RecordAt` with the joint's lengths and point; objective not re-checked). Groups square: edge 1 (Corners elements 3 and 4, run 3) and edge 5 (Corners 2 and 3, run 2). 20 clashes involve the 6 square pieces, all on non-item rows; 14 are band against band over 4 distinct edge pairs, all neighbour pairs; 6 are against row bends, row heads or nodes. The all-S form 000000000 at offset 25 gave no optimal joint, so there is no baseline.

## Q7. Clearance (D271 moved it here)
- **X family** (jp:2052-2071, called from `detect` jp:2074-2078): a head whose row lies strictly inside a Y-axis single bridge's deck rows keeps its flat side of the bridge's column by 1. 0 relations at T, LLRR_LR, LR_LLRR and LL². DESIGN §2: a single bridge's deck as an O3 occupant (side from A2).
- **Y family** (`detect_y` jp:3061-3082 from `plan_clearance` jp:3085-3114, rows jp:2601-2607): a head whose column lies strictly inside an X-axis (doubled) bridge's column span keeps its flat side of that bridge's row by 1. Relations: T 4, LR_LLRR 10, LLRR_LR 58, LL² 94.
  - (a) a single-bridge head against a doubled bridge's row: 4 / 10 / 12 / 68. The sign equals the band side in all of them (head south iff its crossing's band index > the bridge's row index). Not implied by the other unconditional linear Y rows at floor sizes (longest-path test, INFERRED): 4 of 4, 7 of 10, 10 of 12, 48 of 68. DESIGN §2 maps this to a static band side; the "implied by band gap" guess does not hold as a derivation, so the 2b2b test must state it.
  - (b) a doubled bridge's head (item row) against another doubled bridge's row: 0 / 0 / 46 / 26, all implied by the stack rows' order.
- Point-dependence: yes. Both families take the side from the flat solve (`flat[head.xpt] - flat[line.line]`, jp:2063; `y[head.ypt] - y[line.line]`, jp:3072) and are listed only for heads inside the span at that point; Y at flat side 0 demands both sides (jp:3074-3077). The band side makes the sign static; which heads are inside the span stays point-dependent (an unconditional row for every pair would hold at the seeds, INFERRED).

## Q8. Test time (Release, per-case processes, 14 jobs, this tree's `openttd_test`; two samples)
- Gate (549 cases): 28.3 s (load about 8) and 27.0 s. Slowest: joint "at each form, the recording solved at its own form equals the fixed-form joint" 21.3 / 21.1 s; "failing topologies with counters" 18.6 / 18.0; "failing topologies with the hug relaxed" 18.5 / 17.9; "an infeasible system names its conflict" 18.0 / 17.5; "a zero-size bend adds no units" 14.4 / 14.1.
- Sweep (21 cases): 60.3 s (load about 10) and 55.9 s. Slowest: "a row node reached only by row edges is placed by the order" 60.1 / 55.9; "branch legs bound for one main lane merge before joining it" 60.2 / 55.6; "the loop ends settled or names a reason" 56.3 / 52.4; "rule source: detection at the flat solve is today's rulebook" 54.4 / 50.1; "a bridge may pass over the S-bend after its crossing" 52.1 / 48.2. Next: "LL² round 0 with q free" 51.3 / 47.8; "LL² at form 100100000 ..." 41.8 / 39.1 (the LL² seed attempt is most of it, and my probe pays it once).
- 2b2b's tests that touch LL² share its 40 s start; a new LL² case costs about that unless it reuses `Ll2Start`'s cache in the same process (each `jt` case is its own process, so it does not).
