# For Gemini: the T3/LL2 crowding is sibling lanes of one channel

## Recap
- Our drawing pipeline (D422 + Scheme A) on the five test hubs, last LP round: T1 0, T2 0, PSG 197 0 slack. T3 is left at 13 and LL2 at 10, all of it keep-apart (`sep`) rows.
- Keep-apart is pairwise separation. Per face, each pair of walls that aren't adjacent takes one of 4 candidate sides: two axes times two directions. Each LP round writes one row per pair (the extremal endpoints, exact for lengths >= 0). The side comes from the previous round's drawing, at most N rounds (D418).
- Flex inserts are optional sideways steps (bend pairs h, h±1, h). The pipeline discovers with them free, commits those used (length >= 1 half tile, the rest held at 0), then prunes.

## What we ran (your decisive test, plus three follow-ups)
1. **Per stretched pair, 4 LPs**, each with one candidate side hard and every other keep-apart row elastic (as the last round).
   - With the commit's inserts: 11 of 13 pairs have a side that holds. Two LL2 pairs hold on no side.
   - **With every insert free:** all 13 pairs have a side that holds.
   - No single forced side lowers the total slack: it moves onto other pairs.
2. **Your (a) checks:** no corridor (copy or row) track is involved, and every stretched track carries flex inserts.
   - Cause 2: the seed drawing already overlaps these pairs. Margins are negative on almost every candidate; one candidate is at 0 on 4 pairs.
3. **Binaries on just the stretched pairs**, together, with one extremal row per candidate (the over-counting bug of the earlier free-sides probe fixed):

   | Hub | Last round | Joint, commit's inserts | Joint, inserts free |
   |---|---|---|---|
   | T3 | 13 | 13 | 12 |
   | LL2 | 10 | 7 | 7 |

   In the free run, slack moves onto neighbouring pairs whose side stayed fixed.
4. **Every pair of the affected faces binary** (85 pairs, 340 binaries): no incumbent in 15 s on either hub. HiGHS, big-M, every sep row elastic.
5. **IIS:** sides re-picked from the joint-free drawing, keep-apart hard, inserts free, curve rows elastic. Infeasible on both hubs.
   - **T3, 13 rows:** 7 sep rows, all between D2 and D3 (face 75) or B1 and B2 (face 29); 5 piece rows (least lengths) on those tracks; 1 crossing. No port, node, bay or corridor row.
   - **LL2, 44 rows:** of the 12 sep rows, 9 are D1/D2, 1 is B1/B2 and 1 is B2/E2. The rest are pieces, 2 nodes, 9 crossings, a deck/span, an end row and a corridor row's opening and closing steps. The facing D1/B1 corners we suspected before are NOT in it.

## Our reading
- D2/D3, B1/B2 and D1/D2 are **sibling lanes of one channel**: parallel lanes of one bundle that turn together.
- The drawing program draws each lane as an independent track. Shape's spine rule (H14) makes sibling lanes' headings equal only at channel crossings. Between crossings each lane bends on its own pieces with its own flex inserts. Their keep-apart sides are read from earlier drawings, and where the lanes bend at slightly different places those sides contradict each other.
- Our plan of record already says otherwise:
  - D381: parallel neighbours stay at least one lane apart, bends included, by construction from the track map.
  - D388: a channel's lanes share one spine heading list.
  - The drawing program never built that.
- **Proposed rule:** over the stretch where sibling lanes share their spine, draw them as offset curves (as our corridor family already does for copies and prio rows). Their side comes from the channel's lane order in the track map, never from a drawing. Drawing-read sides stay only for pairs between different bundles.

## Questions
1. Do you agree with the reading: the IIS says sibling lanes, so the missing piece is a bundle (offset-curve) model of a channel's lanes, not side picking or the commit? Is anything in the numbers against it?
2. **Flex inserts per lane:** sibling lanes carry their own inserts, so their pieces don't pair one to one. Options we see:
   - (a) inserts at bundle level only: one insert per channel stretch, every lane stepping together, which keeps the offset exact;
   - (b) per-lane inserts allowed, but a lane's step is an exact one-lane opening or closing (as our corridor copies do), with the siblings' offsets rows broken across it;
   - (c) offset rows only on the stretches between inserts, with pairwise sep across the steps.

   Which is sound, and which matches how expert hubs are built (lanes fan and close up near ports, but run as a bundle elsewhere)?
3. **Concentric bends:** for two parallel lanes turning together (45 degrees per bend), what is the exact LP form that keeps them one lane apart through the bend? We use the inner lane's bend vertex earlier and the outer's later along the travel. In half-tile units with axis spacing 2 and diagonal spacing 1, is it just "corresponding pieces separated along their common normal by the spacing, and corresponding vertices offset by the miter vector"? Any trap at a lane order change (a lane crossing over or a lane split inside the bundle)?
4. **The facing corners and B1 on B2:** before this IIS we thought LL2's westbound/eastbound facing corners (D1/B1) and B1#0 lying on B2#4 were the core. Only B1/B2 survives into the IIS. Is it plausible that once siblings are offset curves, the cross-bundle pairs fall out, or should we expect them back?
5. With sibling sides by construction, does the D422 commit ordering problem (inserts committed before the sides are right) shrink, or is it independent?
