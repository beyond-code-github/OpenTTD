# RECON: why mapped LL2 is 2.5x the hand area, and what makes it slow

Read-only recon on wt-planar (c8410f84db). Mapped variant o49t0f0s0: area 2064.25, box 82x59, 13.8 s.
Hand LL2: area 800.25, box 64x47 (-1F1w1). Units below: half tiles unless marked "tiles".
Scratch files: /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2recon/ (poly.py, cmp2.py, run-*.txt).
The hand composer has no dump; its layout is wip/probe8-out/routes-ll2-p8.txt (same 800.25), read with the same scripts.

## 0. What "area" is
Area is NOT the box. It is the polygon of three outer routes (LL2Outline, planar_components.cpp:113):
EF1,E1,EO1 (top), AB1,A1,SO1 (east then south), CD1,C1,WO1 (south back to west), closed along the box edges
(Outline(), line 1814). Recomputed from the dump this gives exactly 2064.25 (mapped) and 800.25 (hand). VERIFIED.
Box ratio is only 1.6x (4838 vs 3008). The other 1.55x is polygon shape. A variant with a LARGER box (o49t0f1s0, 101x65) has a
SMALLER area (1654.25), so the box is the wrong number to watch.

## 1. Where the extra area goes
Both layouts have the same ring: W ports at y 0..8, E ports about 4-8 above them, S ports at the bottom.
Three numbers set the polygon (mapped vs hand):
- width X: 162 vs 126 (every W-E route is exactly X long);
- top edge (E1 route peak above the W port): 24 vs 12;
- S-arm neck, the C1 column to the A1 column: 48 vs 20 (C1 col x=75, A1 col x=123; hand 53 and 73).
Also the S arm sits at x=105..113 of 162 (65 percent along) versus x=57..65 of 126 (45 percent).
Band split of the polygon: above the C1 west leg 1404 vs 556 tiles; below it 660 vs 244 tiles.

Counterfactuals on the mapped polygon (area removed, overlapping, so not additive):
- top at -12 instead of -24 (fan 24 down to 12): -354 tiles
- C column kept at x=105 (no 30 diagonal): -360 tiles
- both together: -714 tiles
- every x scaled by 126/162 (width alone): -459 tiles

Biggest contributors:
1. Entry fans are long. EF1 = 2:10 1:24 2:12, EF2 1:22, CD1 = 0:8 7:30 0:12, CD2 7:28; hand EF1 has 1:4 + 1:8 only, CD none.
   The E lanes leave 24 above the port (hand 12), the C/D lanes leave 30 west. AB is short (5:10, 5:8). Costs about 350 tiles each.
2. S arm is 24 tiles too far east, so X is 36 half tiles too wide. Route sums: F1 route dx 113 (hand 65), C1 105 (hand 57),
   A1 49 (hand 61). X = F dx + A dx = 113 + 49 = 162 (hand 65 + 61 = 126). So W half of the box is the F chain.
3. F crosses D at right angles. F1 = 3:20 2:6 2:18 1:2 2:6 3:2 2:6 3:7 4:22: after a 20 diagonal it runs east
   (36 straight in S-bend jogs) at y about -4 and crosses the vertical D1/D2 columns (x about 81-83, still N-running up to y -7)
   near their tops, then comes down at x=113. Hand F1 crosses D's NE diagonal while heading south (route "S-bend form"),
   so its F column is at x=65 total.
4. D/C columns west of F/A: because F crosses D heading east, the D and C columns must sit WEST of the SO column. The CD ports
   are at x=105,107 and SO at 111,113 (port order), so CD must fan 30 NW to get the columns to x=75..83. That 30 is the neck.
5. Height 59 vs 47 (+24 half): EF1 fan rise 24 vs 12 (+12) and F2 route EF2 to SO2 dy 87 vs 75 (+12). Chain: EF1 y0, rises 24,
   F2 drops 87, so Y = 2 + 87 + 24 (+ margin).

## 2. Construction causes (MappedComposer)
- A. Prio fray at every entry (VERIFIED). ResolvePlanarSites takes site prio from the straight channel's prio
  (planar_gaps.cpp:62); LL2 has prio on E1, F2, A1, B2, C1, D2, so all three entry frays are prio frays. MappedComposer::Fray
  (planar_components.cpp:891) then adds a REQUIRED fan piece (`if (prio) AddSeg(x, Lt, 2, ..., 1.0)`), lane spacing at least 4
  (hand plain fray 2) and a doubled stretch with decks for the near lane (E2 and E2'). The hand LL2 calls Fray with prio=false
  (line 538-545, `t2` is the prio flag) and makes the fan an OPTIONAL step (line 308-310), so the solver drops it. Prio in
  the hand LL2 lives in the Block227 exits (line 233) instead.
- B. Why the fan is 24 and not 4 (INFERRED from the dump, not from a rule): E1 starts 24 above, F1's first piece is a 20 diagonal,
  F2's 18; the E lanes end 4 apart (24 vs 22); so fan = F diagonal + 4. F1 must go under E2' and E2 (dump: F1 under E2' under E2)
  and then be level with the F2 lane. The diagonal 20 is the tunnel under the doubled E2 pair plus curve spacing. Not proved.
- C. Right-angle crossings (VERIFIED in dump and doc): CLAUDE.md "Crossings under a prio (doubled) channel are at right angles where a back-and-forth allows".
  Over choice o49 gives F1 over D1, D2 with F heading east. The hand uses D's diagonal. The neck cost follows (item 4 above).
  Whether the solver could pick a diagonal crossing is not offered: the crossing piece heading is fixed by BuildPlanarPaths.
- D. Bay and doubling. A whole-train bay (2 TL + 4) per non-prio route and doubled decks over every full-share crossing (CLAUDE.md line 59).
  B2 is 6:14 6:2 6:44 7:2 6:12 5:2 6:30 (106), B1 84; hand B2 76, B1 54: +30 each. Those are tied to the same X, so they are
  slack or tight at X=162, I could not tell which (INFERRED: B2 and B1 hold the W-E stack with 4 doubled decks over F1, F2, D2, D1).
- E. Per-node lane split/merge (Fork, merge{n}j): E2a/E2b, EDj1/2 etc. add 20-long jogs (E2a 20, E2b 18, D1a 20, EDj1 18): about 4 pieces
  per exit versus Block227 in the hand. They show up in all six dump routes but sit on the same 126-162 chains as X. INFERRED small.

## 3. Why lazy rounds
PLANAR_TIMES, o49t0f0s0: clashes per round 23, 20, 18, 14, 9, 6, 3, 0 (8 rounds). Each round: one pair separated, four sorted
directions tried (SepOptions), the first feasible one kept. The log shows two solves per round: a trial of the first option
(1.0-1.5 s) and the committed re-solve (0.4-0.55 s); total 13.8 s = 1.5 s first solve + 8 x 1.7 s + 0.8 s area step.
Other variants start higher: o0t0f0s0 34, o20t0f0s0 31, o1t0f0s0 27.
The log prints counts only, not pairs (SolveLazy1, line 1896), so which strands clash is INFERRED: the up-front rules cover
only (a) in-port fan order, (b) a joiner arriving from the neighbour's side. Not covered for LL2:
exit forks and merges per node (E2a/E2b/EDj, D1a/D1b, C2a/b, B1a/b, A2a/b, F1a/b) against the neighbouring channels' bundles,
the doubled B/F/D deck stacks against each other, and the fan legs of EF/CD against the other port's lanes. 12 sites of
that kind exist and all are free diagonal pieces, so about 20+ clashes at round 0 is plausible.
A cheap measurable check: print the pair names in line 1896 (not done, read-only).

## 4. Other variants (single, PLANAR_ONLY, timeout 60 s)
| variant | area | box | time | note |
|---|---|---|---|---|
| o49t0f0s0 | 2064.25 | 82x59 | 13.8 s | 8 rounds, 23 to 0 |
| o49t1f0s0 | 1970.25 | 82x59 | 12.9 s | tie bit 1, -4.6 percent |
| o49t0f1s0 | 1654.25 | 101x65 | 16.3 s | flip bit: bigger box, -20 percent area |
| o1t0f0s0 | 2022.25 | 81x56 | 15.8 s | other F/B over order |
| o20t0f0s0 | 2556.25 | 89x65 | 16.0 s | worse |
| o0t0f0s0 | unsolved | - | 16.6 s | budget at round 6, 3 clashes (15 s lazy budget) |
Dry run: 4032 variants, 128 with o49; those built are the o/t/f/s combinations with s0..s3 (s4+ refused: candidate step on
a non-core). Best seen is the flip variant, 2.07x hand. The flip bit moves area by 20 percent, so the best variant does
differ from o49t0f0s0; whether any of the 4032 gets near 800 is not known (not run).

## 5. Pointers
- Cheapest suspect to test first: make the entry fan an optional step (as in the hand Fray) or drop the prio flag at entry frays.
  That attacks items 1 and 5 together; the CD 30 neck needs the crossing piece to allow a diagonal.
- The 2 solves per round: reuse the trial solution instead of re-solving (would save about 0.4 s of 1.9 s per round) and add the
  per-node join/fork clash sides up front (fewer rounds).
