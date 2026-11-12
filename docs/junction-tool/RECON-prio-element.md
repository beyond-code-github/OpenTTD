# RECON: the prio element (DESIGN-G1 §12.4)

Line numbers are the wt-g1d numbering the brief gave (commit 8c6a9b7b1b). The main checkout has since merged G1d (207035131b): CheckPrio is now at src/junction_rules.cpp:1162 and CheckPrioReach at :1643 (+113); the logic is unchanged except `is_ml` is `IsThroughPath` and the lane test is `SameLanePath`. INFERRED marks anything not read in code.
Units: an axis piece is 2 units, a half piece 1 (junction_route.cpp:70). So `2 * prio_tiles` units = p axis tiles. Signal weight: axis 192, half 128 (junction_rules_internal.h:21). SignalType: Block 0, Entry 1, Exit 2, Combo 3 (signal_type.h:22-30); the diag file's type numbers are right.

## 1. Conditions (merge piece j of an ML movement, M = merge tile, offsets counted upstream, M = 0)

CheckPrio (junction_rules.cpp:1049-1360), per ML merge with a joiner of another movement (a lane merge, same movement, has no prio: :1069):
1. The merge piece is an axis piece (:1158); the joiner is the other path entering M (:1100-1112).
2. The joiner's nearest signalled piece before M carries Entry (:1121-1134). The old pass retypes it in PlaceRequiredSignals via JoinerSignals (junction_signal_pass.cpp:97-104; rules.cpp:690-718 in the old numbering).
3. Exactly one prio component (unused track bits joined across edges) shares a block with M (:1139-1157); any other unused component is "stray track" (:1355).
4. Shape (:1188-1246): the component's tiles are ML tiles ("joins") or row tiles. A join tile holds exactly {axis, h1} (plus the joiner's piece on M) with only h1 unused. The tile across the side edge holds {axis, h2}, or on M only {h2} (an axis there is a stub violation, :1225). All row tiles lie in one row at distance 1 from the ML; a row tile holds the axis alone, or the axis+h2 of a join. h1 touches the side edge and p's exit (downstream) edge; h2 touches the ML-side edge and the row tile's upstream edge (GeometryOfPrio :277-304).
5. The component touches nothing used (:1248-1265): the row's far end and every edge of an unused bit must meet only unused bits of the component. The one exemption is h1 meeting p's own track at the exit edge. INFERRED consequence: the tile beyond the row's upstream end must have no track on the facing edge.
6. A join is on M itself (:1267-1271; D116(a) would allow otherwise, but the code still requires it).
7. Coverage (:1272-1303): walking upstream from M until the units reach 2p (pieces 1..p), every block met, other than M's block, holds exactly one join. A block with no joins is excused only if none of its ML tiles is a bare axis (nothing joinable). Every block of the row holds exactly one join.
8. G9c (:1305-1321, D108): no ML split (split, lane split, lane merge) on any lane of the movement among the pieces of that walk (1..p). Beyond that it is G9b's business.
9. Row signals (:1323-1350): each faces upstream (reverse of the merge trackdir); each lies strictly between two join positions along the axis and not on one; exactly one between each consecutive join pair; the one farthest from M is Exit, all others Combo.
CheckPrioReach, G9b (junction_rules.cpp:1531-1630, D107):
10. The joiner's Entry signal must settle red for a one-tile train on every covered ML (tile, track). Covered = the walk pieces 1..p on every lane through the merge piece, plus piece p+1 when piece p carries a signal (:1599). Bridge span tiles map to their heads. The report is "ML tile in prio coverage does not hold the joiner" (:1617). Only merges whose M holds an unused track are tested (:1585).
11. Signal tiles belong to no block (D107). OpenTTD puts a signal tile in both segments it bounds, so a train there counts for both; a segment with no join (e.g. a lone lane-merge tile between two signals) lets Entry go green.
Related: G6 spacing (:933): more than sd*192 weight (sd=2: three axis tiles) without a signal on a legal piece is a violation. A tile with a join is not legal for a signal (overlapping tracks, :790-795; the phase-0 comment says the same). G6 also wants a signal before and after each merge on the nearest legal piece. G10 and D107 3c: no two consecutive signalled ML pieces when one is a fill signal.

## 2. PlacePrio (junction_signal_pass.cpp:305-432), as steps

1. GeometryOfPrio (:277-304): axis, exit edge, side = opposite the joiner's approach (the sign of the joiner's previous tile offset), h1 = Corner(side, exit), h2 = Corner(reverse side, reverse exit). Skip if the merge is not axis, there is no joiner, it is a lane merge, or the joiner's index is 0.
2. If M already holds h1, another lane did it: return true.
3. `required` blocks (:326-350): for each lane through the merge piece, walk 1..p and add each block met other than M's; add the block of piece (last walked - 1) (the D107 beyond-piece) unconditionally.
4. Per required block (:358-388) take the first ML piece, nearest M, that is in the block, not a signal tile, and is a bare axis (or an already reserved join). If none: if the block has a joinable tile return false (abandon, undo phase 0); else skip it.
5. Joins must lie on M's row at positive step; steps = {0} + sorted join steps (:391-398).
6. Row (:400-410): step 0 gets h2 alone, join steps axis+h2, the rest axis; fail on any clash with existing track.
7. Row signals (:412-418): one on the row tile one step upstream of each downstream join of a consecutive pair; fail if two joins are adjacent.
8. Write h1 on M and every join, the row tracks, and the signals facing the reverse of the merge trackdir, the last Exit, the others Combo (:420-430).
Inputs read from the layout: the fill/anchor signals already placed (has_signal skips signal tiles); ComputeBlocks (so the joins depend on where fill put signals); the phase-0 reservations (ReservePrioJoins :453-498, only for lane merges inside a coverage); the paths. Phase 3b (:500-530) adds signals to lone-lane pieces, and phase 3c (:531-574) drops fill signals so no two adjacent ML signals stand in the coverage.

## 3. Fixed pattern

Let p = spec.prio_tiles, n = p if p is even, else p + 1. Coverage pieces 1..p must all be bare straight axis pieces of every lane (chain.h:75 states this for G9c). In the T render they are: W prio y=4 x=7..20, E prio y=1 x=23..36, all `-` or `#`. In offsets upstream of M:
- ML joins (h1 on the ML tile): offsets 0, 2, 4, ..., n.
- ML signals (spec type, facing the ML travel): odd offsets 1, 3, ..., n-1. The next ML signal upstream of n is at offset n+1 or later, within the G6 spacing; the next signal (n+1 or later) comes from the existing tokens/fill. One plain tile per block, so G6 spacing holds (limit is 2).
- Row tiles (side = opposite the joiner's approach), steps 0..n: step 0 h2 alone; even steps 2..n axis+h2; odd steps axis. The row tile at step n has its axis upstream end open, so the tile at step n+1 must be empty or hold no track on the facing edge.
- Row signals: odd steps 1, 3, ..., n-1, facing upstream, Exit at n-1, Combo at the rest (n/2 signals between the n/2+1 joins).
- Joiner's last signal before M becomes Entry.
- No ML split, curve or lane merge on offsets 1..p (G9c). For p even a split may stand at offset p+1 (D148's p+1; the E prio below). For p odd, piece p is a signal tile, so G9b adds piece p+1, whose block {p+1} holds the last join; p+1 must not be a switch, so the nearest split/lane merge is at offset p+2 or later (INFERRED; consistent with D148a "+1 when prio_tiles is odd and at least 5", generator.h:205).
Rule check: 1 (join per block: blocks are the plain tiles at even offsets, signal tiles at odd offsets are in none), 6 (join at 0), 7, 9 (one signal between each pair, farthest = Exit), 10 (every covered tile is in a block with a join or is a signal tile between two such blocks; p even leaves p unsignalled so no p+1 test; p odd has the p+1 join). G9b for a signal tile at offset k: the train sits in both neighbouring segments, each with a join (INFERRED; the E prio passes it).

Hand check on the diag file (p = 14, test spec, junction_generator.cpp:107):
- E prio matches the pattern exactly. M = (37,1) (travel east, td8); ML joins at x=37,35,...,23 (offsets 0..14, `#` in row y=1 and y=0); ML signals td8 at even x 36..24 (offsets 1..13); row y=0 signals td0 (reverse) at x=36..24 (offsets 1..13), Exit at (24,0), Combo elsewhere (types in the diag list). The E->S split tile is (22,1), offset 15 = p+1, outside the coverage. No violation there.
- W prio does not match. M = (6,4) (travel west, td0), row y=3. ML signals td0 at x=7..17 odd (offsets 1..11) as the pattern says. Joins only at x=6..18 (offsets 0..12); row steps 0..12, signals x=7..17 (offsets 1..11), Exit at (17,3). The pattern wants joins to offset 14 and signals to 13. Instead there is an ML signal at (20,4), offset 14 = p (S list `S (20,4) td0 type0`), and the lane-merge tile (21,4) is offset 15 = p+1.

Cause of the violation at (21,4): CheckPrioReach adds piece p+1 because piece p (x=20) is signalled (:1599). Piece p+1 is the lane-merge tile (21,4): a switch tile in the segment between the signals at (20,4) and (22,4), with no join and no room for one (the old pass places no join on a switch tile, :369-388 skips it as unjoinable). So a train there lets the joiner's Entry (6,5) go green: "ML tile in prio coverage does not hold the joiner" (:1617). The signal at (20,4) is the AfterMerge/fill signal of the lane merge (INFERRED from position), which the old pass puts on the first legal piece after the merge. Phase 0 reserved a join only up to offset 12 (INFERRED from the render), so offset 14 stayed legal for a signal.
Does the pattern fix it? Offset 14 would hold a join (illegal for signals), so the AfterMerge token falls back to offset 13, which is signalled, and piece p is unsignalled, so no beyond-piece. But the row would then reach (20,3), whose upstream edge meets (21,3), a tile of the lane-B S-bend (`/e`, td3 signal, :Upper+Lower glyph `/`) that touches that edge: condition 5 fails (INFERRED from the glyph; I did not read the track bits). So the T's W side lacks one tile: the lane merge must be at offset >= n+2 (= p+2 for even p), where the S-bend on the row side no longer touches the row's end. The T render is the trace with "one free run a tile shorter" (DESIGN-G1.md:186), and D145 says k_w is pinned by the checker: k_w for even p is 2 (INFERRED).

## 4. What the element needs from placement

- Row room: tiles on the row side, steps 0..n, free of any other track; the tile at step n+1 free of any track on the facing edge; the ML tiles 1..n free of splits and bends; for odd p, offset p+1 too.
- The row side is opposite the joiner's approach (D75/D87); it must not be the side where a lane S-bend or bridge approach stands within n+1 tiles of M.
- The joiner needs a signalled piece before M (BeforeMerge token) for the Entry retype.
- Floors already in DESIGN-G1.md:165: lane merge -> joiner merge >= p + k_w (D145; k_w pinned in S2e; this recon says 2 for even p, 3 for odd p, INFERRED, today the T has 1); exit split -> joiner merge >= p + 1 (D148: gives the E prio above; odd p gets +1 from JunctionEWindowPhase, generator.h:205); joiner merge -> fan start >= JUNCTION_PRIO_AFTER_MERGE + JUNCTION_SHIFT_BACK_TILES. D108 is behind the split floors.
- Gap: no floor yet covers the row's far end (step n+1) next to a lane S-bend on the row side; the k_w floor must absorb it.
- Order: the element must reserve its joins before the anchor/fill tokens resolve, or those tokens would sit on offset p (the phase-0 trick, :453-498).
