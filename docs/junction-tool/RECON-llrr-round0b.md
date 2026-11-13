# RECON: LL_RR x LL_RR round 0 infeasible again (8a63a4ae2e), after R3b port fans

Method: throwaway dump of the infeasible rows and a throwaway test (all reverted; ng1 clean on wt-llrr3). Round 0 = RecordAt(MakeLoopStart, all-floor, no point); system.solved is empty there (X flat solve fails), so the relations come from the flat recording, not from rows the joint picks.
The printed conflict is the solver's IIS: not minimal over relations (parallel/duplicate rows). Single-row drop scan (below) names the rows that are each necessary.

## 1. The set (axis, v, u, c: x(v) - x(u) >= c; a/b: c = a L + b; over d: c += d (t - t_rec))
X (axis 0), all constants plain unless noted; points to elements from system.chains:
| rel | reason | v | u | c | elements |
|---|---|---|---|---|---|
| 31 | S-bend size e1 | 56 | 57 | -L2 (a-1) | bend 2 = e1 tok 1 SBend 56->57 (hin south, hand -1, free) |
| 33 | chain run e1 | 57 | 14 | 0 | e1 tile 14 (node 1) |
| 114 | S-bend size e14 | 14 | 79 | L16 (a1) | bend 16 = e14 tok 0 SBend 14->79 (hand +1, free) |
| 149,150 | prio element e19 n24 | 37<-150, 150<-149 | | 0, 14 | e19 Prio 149..150 (14 wide) |
| 167,166 | prio / series bridge heads e19 | 149<-148, 148<-154 | | 2, 2 | e19 SBend 147..148 (bend 21), HeadsPair c1 153..154 |
| 161,163 | lane split run / series heads e19 | 146<-154, 147<-146 | | 1, 1 | HeadsPair c0 145..146 |
| 175 | hug crossers e19 row 1 | 154 | 101 | 1 | crosser = e26 SBend 100..101 end (bend 31) |
| 223 | chain run e26 | 101 | 40 | 0 | node 40 tile (e24/e26/e47) |
| 452,453 | lane split run e44 n24 | 209<-37, 203<-209 | | 2, 2 | e44 c1 SBend 209..210 (bend 55), HeadsPair c0 203..204 |
| 455,457 | lane split run e44 | 210<-209, 211<-210 | | 0, 1 | c1 copy |
| 467 | hug crossers e44 row 1 | 79 | 203 | 1 | crosser = e14 SBend end pt 79 |
| 488 | chain run e47 | 40 | 130 | 0 | e47 Heads 130..131 (bridge column) |
| 498 | bridge over S-bend e47 | 130 | 56 | 1, over 8 d +1 | x130 >= x56 + t (t rec. 1); over 8 = e47 tok 0, bend 2, no "after" (bend_after -1) |
Y (axis 1): 797/799 stack rows (y2>=y1+1, y3>=y2+1); 817 band gap node 1 (y4 >= y14 + 1); 887 S-bend size e1 (y57 - y56 >= L2 - 1); 1245 chain run e47 (y131 >= y132 + 2); 1247 Corner e47 (y132 >= y133 + 2); 1248 (y133 >= y3); 1257 bridge over e47 (y56 >= y131 + 1 - (t-1) = 2 - t, d -1).
Non-relation rows also in the IIS (not printed): link rows x57 = x56 +- L2, y57 = y56 + L2 - 1; the port fan row of e2 (bend 3, item pt 4): y4 + L3 = 26, L3 >= 0.

## Cycle (west smaller x, south larger y)
X: x79 >= x203+1 >= x209+3 >= x37+5 >= x150+5 >= x149+19 >= x148+21 >= x154+23 >= x101+24 >= x40+24 >= x130+24 >= x56 + t + 24.
Then x14 >= x79 + L16, x57 >= x14, x56 >= x57 - L2  =>  L2 >= t + 24 + L16 >= 26 (t >= 1, L16 >= 1).
Y: y1 = 0, y2 >= 1, y3 >= 2, y131 >= y3 + 4 >= 6, y56 >= y131 + 2 - t >= 8 - t (also y56 >= y13 + 6 = 7 by curve gap e1), y57 = y56 + L2 - 1, y14 >= y57 + 2, y4 >= y14 + 1  =>  y4 >= L2 + 10 - t.
Fan: y4 = 26 - L3 <= 26.
Together: 26 >= y4 >= L2 + 10 - t >= t + 24 + L16 + 10 - t = 34 + L16 >= 35. t cancels, L2 cancels: 0 >= 9 for every L, t. Alt: none (e47 has one candidate S-bend). The cycle mixes axes only through L2 (the e1 S-bend's width = its height) and t; X forces L2 >= 26, Y caps L2 <= 26 - 10 + t - ... i.e. the rows run out at the port row 26.
Check: 9deaa6901c's solution has y56 7, t 1, L2 26, y57 32, y14 36, y4 37 (> 26): exactly this chain, with the fan row absent.

## 2. Geometry
- Edge 47's bridge crosses e1's S-bend (bend 2, tok 1 of e1), not a Corner. Its "Corner size (edge 47)" rows are its own joiner Corner 132->133 (fixed_k 2, hin north), which sets how far up the head 131 sits from the stack item 3 (y131 >= y3 + 4); the Corner is a Y link, no bridge goes over it.
- e44's bridge (heads 203/204 c0, 211/212 c1, doubled, west-bound from node 24) hugs e14 at pt 79 (end of e14's S-bend 14->79). e19's bridge (HeadsPair 145/146 c0, 153/154 c1, series SBend 147..148, Prio 149..150 of width 14) hugs e26 at pt 101 (end of e26's S-bend 100->101). e19/e44 sit between node 40 (e47's bridge column) and e14's S-bend: the 24 columns.
- So e47's bridge column (x56 + t, over e1's S-bend) must lie west of everything e19/e44 span, and e14's S-bend lies east of all that; e1's S-bend 56->57 has to cover those 24 + t + L16 columns; being diagonal, it is as tall as wide: L2 >= 26 rows, from row y56 >= 7, ending y57 >= 32, and y4 >= 35, but the fan of e2 pins y4 <= 26.

## 3. Staleness tests (single-reason drops, all at the round-0 recording)
- Drop "bridge heads hug crossers": FEASIBLE (Optimal). Drop "prio element": still infeasible, new conflict is the same family with edges 28/29 instead of 19/44 (hug (edge 28, row 2), hug (edge 29, row 3), lane split runs, bridge over S-bend e47, Corner size e47, band gap, e8/e14 bridge heads). Drop "stack rows" or "band gap": stack rows still infeasible (curve gap e1 gives y56 >= 7), band gap FEASIBLE.
- Single-relation drop scan of the 26 printed relations: each of 33, 223, 488, 498, 817 alone makes it feasible; the other 21 are redundant singly (parallel routes). Not necessary: the hugs 175/467 individually (the e28/e29 family also closes).
- Staleness verdict (not proven): the hugs are the likeliest stale rows. They read "which crosser tile is on the head row" at the flat rows (y4 near 12 at the recording: y4 = 26 - 14), but the 9deaa solution has y4 = 37, y101 = 48 (e26's S-bend end BELOW the head row, so e19's crosser on row 4 is not pt 101 there) while y79 = 36 < y4 (e14's pt 79 is above, column stays x79: hug 467 fine). system.solved is empty at round 0, so I could not re-record at a row point to test it. A test would be RecordAt at the 9deaa joint point.
- "prio element", "stack rows", "band gap" are Y-independent constants; prio is a width (14 + 2 + 2), stack rows and band gap read rows only as least gaps (no stale dependence).

## 4. Before R3b
- 9deaa6901c (G3.4b tip, forked before R3b 65500765be): same test, status 0 Optimal. L2 = 26, L16 = 1, t 1, L3 = 14 (fan held at its recorded size 14, no row link), y4 = 37, y56 = 7, y57 = 32, y14 = 36.
- The R3b change (junction_joint.cpp: port_rows bends get L variable and the row y(item) + L = port_row for an in-edge fan): bend 3 (e2, Fan 137->138, hand -1, hin 1, item pt 4, port row 26, recorded L 14 so y4 was 12) now forces y4 = 26 - L3 <= 26. Before, L3 was constant and y4 was free upward (37). Same for bend 0 (item 2, port 25), bends 12/14/42/45/54/58 on other items (not in the cycle).
- So R3b did not add a bend to the cycle; it added the cap y4 <= port row. The cycle itself (e1's S-bend wide by 26) existed before at L2 = 26 and needed y4 >= 35, which only the unbounded old rows allowed.
- Reading for the fix: the cap is physical (the fan from the stack item must reach its port below: port rows 25..29 are fixed by the notation offset), while 24 columns of W-bound heads force an e1 diagonal that tall; either the stack/head rows must be above the port rows differently (port row offset free?) or e47's bridge must not sit over the e1 S-bend (e1's S-bend is a 26-row diagonal only because of the bridge hug).
