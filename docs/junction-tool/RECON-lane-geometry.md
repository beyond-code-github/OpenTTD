# Doubled-lane geometry recon (worktree .claude/geo @ 6ef853bf27, instrumented, reverted)

Conventions: working x of the generator frame (travel is -x on the W row; split side = high x). Columns in tables: `x,y`; `L` = IsLegalSignalPiece on the layout without signals (any non-bridge piece whose tile has no overlapping track, so half pieces count when alone on their tile); `S>` signal facing along travel, `S<` against; "pre" = as built by the candidate (PlaceJunctionSignals), "post" = after RemoveRedundantJunctionSignals (what GenerateJunction returns).
Specs: S1 = MakeTestJunctionSpec() (tl 4, prio 14, D138 train, already in the spec); S2 = same with tl 5, prio 15.
Winners (first sweep-order candidate whose layout JSON equals GenerateJunction's best): S1 height 15, cross 0, e_slack 1, w_slack 1, margin 1, style 1 (gap 1, sb_back 4). S2 height 13, cross 0, e_slack 2, w_slack 1, margin 3, style 1 (gap 1, sb_back 5).
Both winners are lane_style 1 (parallelogram). Flipped to style 0 with the same params: no candidate at all (has=0) for both. `forced-gapN` sections build the same params with the gap pinned (instrumentation), to show the tight variants.
Lane A = in-line lane (W row y=2, goes straight through its bridge, then RFL down onto the b row y=3 = merge); lane B = shifted lane (R F L down to the b row at the lane split, bridge on b row, LFR back up to the W row at x_sb). Note: "RFL" here yields two half pieces on ONE x column (the first on the upper row, the second on the lower row), axis pieces at column+1 and column-1.

## Findings summary (S1 numbers; S2 differs only where stated)

**c. Derived**
- RFL/LFR occupy ONE x column (two half pieces, one per row); axis-to-axis step is 2 (S1: lane B axis 49 -> bend col 48 -> axis 47; lane A axis 36 -> col 35 -> axis 34; shift-back axis 16 -> col 15 -> axis 14). Counting the two flanking axis tiles it spans 3 tiles.
- Near end, gap 1 (winner): x_ls=48, x_heA=x_heB=45 (the generator has ONE h_e for both lanes), x_ls-x_heA = x_ls-x_heB = 3. Signal-legal PLAIN axis pieces between split and near head: A {47,46} = 2, B {47,46} = 2; counting half pieces B also has its second half piece (48,3) = 3 legal. Post-removal signals: A at 46 (47 removed), B at (48,3) and 46.
- Far end, gap 1: x_hwA=x_hwB=37 (S2: 35), merge x_lm=34 (S2: 32), x_hwA-x_lm = x_hwB-x_lm = 3. Plain legal pieces between far head and merge: A {36}=1, B {36}=1 (A also has its first half piece (35,2), legal, pre-signal removed). B's tile (35,3) is shared with A's second half piece, so not legal.
- x_sl (W->S start piece)=51, x_ls=48, x_sl-x_ls = 3 (SL split tile, first non-axis piece of W->S, is x=50). Always 3 (S2 too).
- N->W join x_nw=18 (S2: 15, on the b row), x_lm-x_nw = 16 (S2: 17), prio_tiles 14 (S2: 15): nw_max_x = x_lm - prio - w_slack = 19 (S2 16), the route lands one tile beyond that.
- Gap pinned to 0 (S1 only; S2 has no gap-0 candidate): x_ls=47, x_heA=x_heB=45 (x_ls-x_he=2): near side is rule-clean (A 1 plain legal {46}, B {(47,3),46}), far side is not: x_hw-x_lm=2, B has no legal piece between its far head (37,3) and merge ("split in a bridge's block: no signal between the bridge and the merge" at 37,3, because A's first bend tile (36,3) overlaps B). Gap 2: x_ls=49 (extra near plain, far side unchanged). I.e. `gap` shifts the near end and far end together although only the far end needs the extra tile.

**d. Branch crossings (x per row), S1** (S2 in its section)

| route | W row y=2 | b row y=3 | stagger |
|---|---|---|---|
| E->S (far side, low x) | 38,39 | 39 | b row min is +1 vs W row (E->S slants toward +x going down) |
| N->E (near side, high x) | 43,44 | 42,43 | b row is -1 vs W row (N->E slants toward -x going down) |

S2: E->S 38,39 | 39; N->E 41,42 | 40,41 (b row -1).
Heads today: h_w = min(W row crossing) - margin, h_e = max(W row crossing) + margin, same x for both lanes. Near end: b-row crossing max (43) is 1 BELOW the W-row max (44), so the b-row lane could legally sit 1 tile nearer the bridge middle (x 44), i.e. the diagonal permits the shifted lane's head 1 tile FARTHER from the split, the opposite of D118's wording. Far end: b-row crossing min (39) is 1 ABOVE W-row min (38), so B's far head may sit at 38 = h_w+1.

## e. D118 arithmetic (parallelogram; derived from the measured RFL extent and legality)

Facts used: (1) at the lane split tile x_ls, A's axis piece and B's first half piece share a tile, so that tile is not signal-legal for either lane; B's second half piece (x_ls, b row) is alone on its tile, legal, and can carry a signal (it does post-removal in the winner: (48,3) S>). (2) RFL = one bend column, axis at x_ls+1 before and x_ls-1 after.

Near end, exactly one signal-legal piece per leg:
- Lane A (in-line, W row): tile x_ls (shared, illegal), then ONE plain legal piece x_ls-1, head at x_ls-2: x_ls - x_heA = 2.
- Lane B (shifted): bend col x_ls, its second half piece (x_ls, b row) is the ONE legal piece, head right after at x_ls-1: x_ls - x_heB = 1.
- So the shifted lane B's near head sits one tile closer to the split than A's (x_heB = x_heA + 1): matches D118. Today both are at h_e with x_ls - x_he = 3 (gap 1) or 2 (gap 0; gap 0 gives A 1 plain piece but B 2 legal, still not minimal for B). To build it from h_e as the in-line anchor: A head stays h_e (45), B head moves +x (toward the split) by 1 to h_e+1 (46), and x_ls = h_e+2 = 47 (S1), i.e. one tile less than today's 48. Relative to today's B head (45): +1 x; B's near leg loses (3 - 1) = 2 legal pieces, A's loses 1 (3 -> 2 in x_ls-x_he terms).
- Crossing safety: B's near head at 46 is farther from the b-row N->E crossing (max 43) than before, so it is safe; only its bridge gets one longer unless the far head also moves (below).

Exit end, exactly one signal-legal piece per leg (mirror): at the merge, A's first half piece (x_b, W row) is alone on its tile (legal), A's second half piece (x_b, b row) shares a tile with B's axis (illegal), where x_b is the bend column and the merge piece is at x_lm = x_b - 1.
- Lane A: far head, then the half piece (x_b, W row) as its ONE legal piece: x_hwA = x_b + 1, x_hwA - x_lm = 2.
- Lane B: far head, ONE plain legal piece, then the shared bend tile: x_hwB = x_b + 2, x_hwB - x_lm = 3.
- So B's far head is one tile FARTHER from the merge than A's: x_hwB = x_hwA + 1, which, with x_heB = x_heA + 1, is exactly equal bridge lengths (B shifted +1 at both ends). This is D118's "bridge ends stay an even length" case. For S1: x_hwA=37 -> x_lm=35 (this equals the gap-0 merge), x_hwB=38, B's bridge 38..46 vs A's 37..45; b-row crossing min 39 is clear of B's far head 38 (margin 1, same as margin 1 of the winner).
- Today's gap-0 failure is exactly this: both far heads at h_w, B then has zero legal pieces (A's bend overlaps its first tile). Moving only B's far head +1 (x) fixes it with no extra tile on A.

Generator consequence: the lane programs need independent near/far head targets per lane (A at h_e / h_w, B at h_e+1 / h_w+1), not one shared head x plus a shared `gap`. Caveat measured on the gap-0 build: with q=0 the move string `RFL` + `B<n>` already leaves ONE plain piece between the S-bend and the near head (x_ls-x_heB = 2, bend col 47, plain 46, head 45), so `RFL B<n>` cannot express x_ls - x_heB = 1 (head directly after the bend); the solver/lane builder must place B's head at the bend's second half piece + 1 directly. A's `F.. B<n>` from the split tile with head at x_ls-2 is expressible.

# SPEC S1 tl=4 prio=14 best_hub=27,1
WINNER height=15 cross=0 e_slack=1 w_slack=1 margin=1 style=1


## S1 / flipped-style
`CONFIG flipped-style style=0 clean=0 has=0 height=15 cross=0 e_slack=1 w_slack=1 margin=1`

No candidate built with these params.


## S1 / winning-style
`CONFIG winning-style style=1 clean=1 has=1 height=15 cross=0 e_slack=1 w_slack=1 margin=1`

`{'GAP': 1, 'SBBACK': 4, 'h_w': 37, 'h_e': 45, 'w_row': 2, 'b_row': 3, 'merge_x': 34, 'x_ls': 48, 'x_sb': 15, 'nw_max_x': 19}` shift=(13, -1)

Lane A moves: `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFB7FRFL`

Lane B moves: `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFRFLFB7FFFFFFFFFFFFFFFFFFFFFLFRFFFFFFFFFFFFFF`

**b. Positions (working x; travel is -x)**

| item | x | note |
|---|---|---|
| W->S start piece | 51 | y=2 axis; first piece leaving the axis (SL split tile) x=50 |
| lane split (B first non-axis) | 48 | tile cols of bend [(35, 48, 2, 3, 0, 0), (36, 48, 3, 2, 0, 1)] |
| lane A S-bend #1 | cols [35] | rows [2, 3], idx 48..49 (LoE UpE ) |
| lane B S-bend #1 | cols [48] | rows [2, 3], idx 35..36 (LoE UpE ) |
| lane B S-bend #2 | cols [15] | rows [2, 3], idx 69..70 (LfN RtN ) |
| near head A / B | 45 / 45 | rows y=2 / 3; idx 38 / 39 |
| far head A / B | 37 / 37 | idx 46 / 47 |
| lane merge x_lm (merge_x) | 34 | A last piece (34, 3); B axis piece there |
| N->W join (last NW piece) | 18 | y=3 td=NE; nw_max_x=19 |
| shift-back S-bend x_sb (meta) | 15 | B bend #2 cols [15] |

S-bend extents (axis before -> axis after): A: axis 36->34, step 2, bend cols [35], rows [2, 3]; B: axis 49->47, step 2, bend cols [48], rows [2, 3]; B: axis 16->14, step 2, bend cols [15], rows [2, 3]

**a. Pieces, x in [11,54]** (cols: idx | x,y | trackdir | B=bridge | L=signal-legal | S pre-removal | S post-removal; > with, < against travel)

Lane A:

| idx | x,y | td | br | leg | sig pre | sig post |
|---|---|---|---|---|---|---|
| 29 | 54,2 | NE |  | L |  |  |
| 30 | 53,2 | NE |  | L | S> | S> |
| 31 | 52,2 | NE |  | L |  |  |
| 32 | 51,2 | NE |  | L | S> | S> |
| 33 | 50,2 | NE |  |  |  |  |
| 34 | 49,2 | NE |  | L | S> | S> |
| 35 | 48,2 | NE |  |  |  |  |
| 36 | 47,2 | NE |  | L | S> |  |
| 37 | 46,2 | NE |  | L | S> | S> |
| 38 | 45,2 | NE | B |  |  |  |
| 39 | 44,2 | NE | B |  |  |  |
| 40 | 43,2 | NE | B |  |  |  |
| 41 | 42,2 | NE | B |  |  |  |
| 42 | 41,2 | NE | B |  |  |  |
| 43 | 40,2 | NE | B |  |  |  |
| 44 | 39,2 | NE | B |  |  |  |
| 45 | 38,2 | NE | B |  |  |  |
| 46 | 37,2 | NE | B |  |  |  |
| 47 | 36,2 | NE |  | L | S> | S> |
| 48 | 35,2 | LoE (half) |  | L | S> |  |
| 49 | 35,3 | UpE (half) |  |  |  |  |
| 50 | 34,3 | NE |  | L | S> | S> |

Lane B:

| idx | x,y | td | br | leg | sig pre | sig post |
|---|---|---|---|---|---|---|
| 29 | 54,2 | NE |  | L |  |  |
| 30 | 53,2 | NE |  | L | S> | S> |
| 31 | 52,2 | NE |  | L |  |  |
| 32 | 51,2 | NE |  | L | S> | S> |
| 33 | 50,2 | NE |  |  |  |  |
| 34 | 49,2 | NE |  | L | S> | S> |
| 35 | 48,2 | LoE (half) |  |  |  |  |
| 36 | 48,3 | UpE (half) |  | L | S> | S> |
| 37 | 47,3 | NE |  | L |  |  |
| 38 | 46,3 | NE |  | L | S> | S> |
| 39 | 45,3 | NE | B |  |  |  |
| 40 | 44,3 | NE | B |  |  |  |
| 41 | 43,3 | NE | B |  |  |  |
| 42 | 42,3 | NE | B |  |  |  |
| 43 | 41,3 | NE | B |  |  |  |
| 44 | 40,3 | NE | B |  |  |  |
| 45 | 39,3 | NE | B |  |  |  |
| 46 | 38,3 | NE | B |  |  |  |
| 47 | 37,3 | NE | B |  |  |  |
| 48 | 36,3 | NE |  | L | S> | S> |
| 49 | 35,3 | NE |  |  |  |  |
| 50 | 34,3 | NE |  | L | S> | S> |
| 51 | 33,3 | NE |  |  |  |  |
| 52 | 32,3 | NE |  | L | S> | S> |
| 53 | 31,3 | NE |  |  |  |  |
| 54 | 30,3 | NE |  | L | S> | S> |
| 55 | 29,3 | NE |  |  |  |  |
| 56 | 28,3 | NE |  | L | S> | S> |
| 57 | 27,3 | NE |  |  |  |  |
| 58 | 26,3 | NE |  | L | S> | S> |
| 59 | 25,3 | NE |  |  |  |  |
| 60 | 24,3 | NE |  | L | S> | S> |
| 61 | 23,3 | NE |  |  |  |  |
| 62 | 22,3 | NE |  | L | S> | S> |
| 63 | 21,3 | NE |  |  |  |  |
| 64 | 20,3 | NE |  | L | S> | S> |
| 65 | 19,3 | NE |  |  |  |  |
| 66 | 18,3 | NE |  | L | S> | S> |
| 67 | 17,3 | NE |  | L |  |  |
| 68 | 16,3 | NE |  | L | S> | S> |
| 69 | 15,3 | LfN (half) |  | L |  |  |
| 70 | 15,2 | RtN (half) |  | L | S> | S> |
| 71 | 14,2 | NE |  | L |  |  |
| 72 | 13,2 | NE |  | L |  |  |
| 73 | 12,2 | NE |  | L |  |  |
| 74 | 11,2 | NE |  | L |  |  |

Branch routes (pass 2) pieces in the same x range:

- WS: (51,2)NE (50,2)LoE (50,3)UpE (49,3)LoE (49,4)UpE (48,4)LoE (48,5)UpE (47,5)LoE (47,6)UpE (46,6)LoE (46,7)SE (46,8)SE (46,9)UpE (45,9)LoE (45,10)UpE (44,10)LoE (44,11)UpE (43,11)LoE (43,12)UpE (42,12)LoE (42,13)SE
- ES: (36,0)SW (37,0)RtS (37,1)LfS (38,1)RtS (38,2)LfS (39,2)RtS (39,3)SE (39,4)SE (39,5)SE (39,6)SE (39,7)SE (39,8)LfS (40,8)RtS (40,9)LfS (41,9)RtS (41,10)LfS (42,10)RtS (42,11)SE (42,12)SE
- NE: (40,12)NW (40,11)NW (40,10)NW (40,9)NW (40,8)NW (40,7)NW (40,6)NW (40,5)LoW (41,5)UpW (41,4)LoW (42,4)UpW (42,3)LoW (43,3)UpW (43,2)LoW (44,2)UpW (44,1)LoW (45,1)SW (46,1)SW (47,1)SW (48,1)SW (49,1)SW (50,1)SW (51,1)SW (52,1)UpW (52,0)LoW (53,0)SW
- NW: (40,13)NW (40,12)RtN (39,12)LfN (39,11)RtN (38,11)LfN (38,10)RtN (37,10)LfN (37,9)RtN (36,9)LfN (36,8)RtN (35,8)LfN (35,7)RtN (34,7)LfN (34,6)RtN (33,6)LfN (33,5)RtN (32,5)LfN (32,4)RtN (31,4)NE (30,4)NE (29,4)NE (28,4)NE (27,4)NE (26,4)NE (25,4)NE (24,4)NE (23,4)NE (22,4)NE (21,4)NE (20,4)NE (19,4)LfN (19,3)RtN (18,3)NE

**c. Derived**

- x_ls=48; x_heA=45, x_heB=45; x_ls-x_heA=3, x_ls-x_heB=3
- signal-legal plain axis pieces between lane split (x_ls, exclusive) and near head: A [47, 46] (2), B [47, 46] (2)
- all signal-legal pieces (incl. half pieces), lane split to near head: A [(47, 2), (46, 2)], B [(48, 3), (47, 3), (46, 3)] (B counts from after its first half piece; A from after the split tile)
- all signal-legal pieces, far head to merge piece: A [(36, 2), (35, 2)], B [(36, 3)]
- between far head and merge x_lm (exclusive both): A [36] (1), B [36] (1)
- x_hwA=37, x_hwB=37, x_lm=34: x_hwA-x_lm=3, x_hwB-x_lm=3
- x_sl=51, x_ls=48: x_sl-x_ls=3
- x_nw=18, x_lm-x_nw=16, prio_tiles=14

**d. Branch crossings (x per row; working coords)**

| route | y=1 | y=2 | y=3 | y=4 |
|---|---|---|---|---|
| ES | 37, 38 | 38, 39 | 39 | 39 |
| NE | 44, 45, 46, 47, 48, 49, 50, 51, 52 | 43, 44 | 42, 43 | 41, 42 |

**ASCII** (layout coords; working x = col + 13, y = row + -1); doubled section rows:

```
. . . . . . . . . . . . . . . . . . . . . . . . \s\ . . . . . / ->- ->- - ->- /w. . 
- - \n. . . / -># -># -># -># -># -># -># . / -<H =s= = = = =w= H -<- # -<# -<- -<- 
. . \ -<- -<# -<# -<# -<# -<# -<# -<# -<# -<# -<H = =v= = = = = H -<- /e/e/ . . . . 
. . . . . . \n- -<- - -<- -<- -<- -<- \n. . . . . . | . / /w. . . . . /e/ . . . . . 
```

## S1 / forced-gap0
`CONFIG forced-gap0 style=1 clean=0 has=1 height=15 cross=0 e_slack=1 w_slack=1 margin=1`

Violations: [rule 6 at 37,3: split in a bridge's block: no signal between the bridge and the merge] 

(NOT clean; geometry shown for reference)

`{'GAP': 0, 'SBBACK': 6, 'h_w': 37, 'h_e': 45, 'w_row': 2, 'b_row': 3, 'merge_x': 35, 'x_ls': 47, 'x_sb': 14, 'nw_max_x': 20}` shift=(12, -1)

Lane A moves: `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFB7RFL`

Lane B moves: `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFRFLB7FFFFFFFFFFFFFFFFFFFFFFLFRFFFFFFFFFFFFF`

**b. Positions (working x; travel is -x)**

| item | x | note |
|---|---|---|
| W->S start piece | 50 | y=2 axis; first piece leaving the axis (SL split tile) x=49 |
| lane split (B first non-axis) | 47 | tile cols of bend [(36, 47, 2, 3, 0, 0), (37, 47, 3, 2, 0, 1)] |
| lane A S-bend #1 | cols [36] | rows [2, 3], idx 47..48 (LoE UpE ) |
| lane B S-bend #1 | cols [47] | rows [2, 3], idx 36..37 (LoE UpE ) |
| lane B S-bend #2 | cols [14] | rows [2, 3], idx 70..71 (LfN RtN ) |
| near head A / B | 45 / 45 | rows y=2 / 3; idx 38 / 39 |
| far head A / B | 37 / 37 | idx 46 / 47 |
| lane merge x_lm (merge_x) | 35 | A last piece (35, 3); B axis piece there |
| N->W join (last NW piece) | 19 | y=3 td=NE; nw_max_x=20 |
| shift-back S-bend x_sb (meta) | 14 | B bend #2 cols [14] |

S-bend extents (axis before -> axis after): A: axis 37->35, step 2, bend cols [36], rows [2, 3]; B: axis 48->46, step 2, bend cols [47], rows [2, 3]; B: axis 15->13, step 2, bend cols [14], rows [2, 3]

**a. Pieces, x in [10,53]** (cols: idx | x,y | trackdir | B=bridge | L=signal-legal | S pre-removal | S post-removal; > with, < against travel)

Lane A:

| idx | x,y | td | br | leg | sig pre | sig post |
|---|---|---|---|---|---|---|
| 30 | 53,2 | NE |  | L |  |  |
| 31 | 52,2 | NE |  | L | S> | S> |
| 32 | 51,2 | NE |  | L |  |  |
| 33 | 50,2 | NE |  | L | S> | S> |
| 34 | 49,2 | NE |  |  |  |  |
| 35 | 48,2 | NE |  | L | S> | S> |
| 36 | 47,2 | NE |  |  |  |  |
| 37 | 46,2 | NE |  | L | S> | S> |
| 38 | 45,2 | NE | B |  |  |  |
| 39 | 44,2 | NE | B |  |  |  |
| 40 | 43,2 | NE | B |  |  |  |
| 41 | 42,2 | NE | B |  |  |  |
| 42 | 41,2 | NE | B |  |  |  |
| 43 | 40,2 | NE | B |  |  |  |
| 44 | 39,2 | NE | B |  |  |  |
| 45 | 38,2 | NE | B |  |  |  |
| 46 | 37,2 | NE | B |  |  |  |
| 47 | 36,2 | LoE (half) |  | L | S> | S> |
| 48 | 36,3 | UpE (half) |  |  |  |  |
| 49 | 35,3 | NE |  | L | S> | S> |

Lane B:

| idx | x,y | td | br | leg | sig pre | sig post |
|---|---|---|---|---|---|---|
| 30 | 53,2 | NE |  | L |  |  |
| 31 | 52,2 | NE |  | L | S> | S> |
| 32 | 51,2 | NE |  | L |  |  |
| 33 | 50,2 | NE |  | L | S> | S> |
| 34 | 49,2 | NE |  |  |  |  |
| 35 | 48,2 | NE |  | L | S> | S> |
| 36 | 47,2 | LoE (half) |  |  |  |  |
| 37 | 47,3 | UpE (half) |  | L | S> |  |
| 38 | 46,3 | NE |  | L | S> | S> |
| 39 | 45,3 | NE | B |  |  |  |
| 40 | 44,3 | NE | B |  |  |  |
| 41 | 43,3 | NE | B |  |  |  |
| 42 | 42,3 | NE | B |  |  |  |
| 43 | 41,3 | NE | B |  |  |  |
| 44 | 40,3 | NE | B |  |  |  |
| 45 | 39,3 | NE | B |  |  |  |
| 46 | 38,3 | NE | B |  |  |  |
| 47 | 37,3 | NE | B |  |  |  |
| 48 | 36,3 | NE |  |  |  |  |
| 49 | 35,3 | NE |  | L | S> | S> |
| 50 | 34,3 | NE |  |  |  |  |
| 51 | 33,3 | NE |  | L | S> | S> |
| 52 | 32,3 | NE |  |  |  |  |
| 53 | 31,3 | NE |  | L | S> | S> |
| 54 | 30,3 | NE |  |  |  |  |
| 55 | 29,3 | NE |  | L | S> | S> |
| 56 | 28,3 | NE |  |  |  |  |
| 57 | 27,3 | NE |  | L | S> | S> |
| 58 | 26,3 | NE |  |  |  |  |
| 59 | 25,3 | NE |  | L | S> | S> |
| 60 | 24,3 | NE |  |  |  |  |
| 61 | 23,3 | NE |  | L | S> | S> |
| 62 | 22,3 | NE |  |  |  |  |
| 63 | 21,3 | NE |  | L | S> | S> |
| 64 | 20,3 | NE |  |  |  |  |
| 65 | 19,3 | NE |  | L | S> | S> |
| 66 | 18,3 | NE |  | L |  |  |
| 67 | 17,3 | NE |  | L | S> | S> |
| 68 | 16,3 | NE |  | L |  |  |
| 69 | 15,3 | NE |  | L | S> | S> |
| 70 | 14,3 | LfN (half) |  | L |  |  |
| 71 | 14,2 | RtN (half) |  | L | S> | S> |
| 72 | 13,2 | NE |  | L |  |  |
| 73 | 12,2 | NE |  | L |  |  |
| 74 | 11,2 | NE |  | L |  |  |
| 75 | 10,2 | NE |  | L |  |  |

Branch routes (pass 2) pieces in the same x range:

- WS: (50,2)NE (49,2)LoE (49,3)UpE (48,3)LoE (48,4)UpE (47,4)LoE (47,5)UpE (46,5)LoE (46,6)UpE (45,6)LoE (45,7)SE (45,8)SE (45,9)SE (45,10)UpE (44,10)LoE (44,11)UpE (43,11)LoE (43,12)UpE (42,12)LoE (42,13)SE
- ES: (36,0)SW (37,0)RtS (37,1)LfS (38,1)RtS (38,2)LfS (39,2)RtS (39,3)SE (39,4)SE (39,5)SE (39,6)SE (39,7)SE (39,8)LfS (40,8)RtS (40,9)LfS (41,9)RtS (41,10)LfS (42,10)RtS (42,11)SE (42,12)SE
- NE: (40,12)NW (40,11)NW (40,10)NW (40,9)NW (40,8)NW (40,7)NW (40,6)NW (40,5)LoW (41,5)UpW (41,4)LoW (42,4)UpW (42,3)LoW (43,3)UpW (43,2)LoW (44,2)UpW (44,1)LoW (45,1)SW (46,1)SW (47,1)SW (48,1)SW (49,1)SW (50,1)SW (51,1)SW (52,1)UpW (52,0)LoW (53,0)SW
- NW: (40,13)NW (40,12)RtN (39,12)LfN (39,11)RtN (38,11)LfN (38,10)RtN (37,10)LfN (37,9)RtN (36,9)LfN (36,8)RtN (35,8)LfN (35,7)RtN (34,7)LfN (34,6)RtN (33,6)LfN (33,5)RtN (32,5)LfN (32,4)RtN (31,4)NE (30,4)NE (29,4)NE (28,4)NE (27,4)NE (26,4)NE (25,4)NE (24,4)NE (23,4)NE (22,4)NE (21,4)NE (20,4)LfN (20,3)RtN (19,3)NE

**c. Derived**

- x_ls=47; x_heA=45, x_heB=45; x_ls-x_heA=2, x_ls-x_heB=2
- signal-legal plain axis pieces between lane split (x_ls, exclusive) and near head: A [46] (1), B [46] (1)
- all signal-legal pieces (incl. half pieces), lane split to near head: A [(46, 2)], B [(47, 3), (46, 3)] (B counts from after its first half piece; A from after the split tile)
- all signal-legal pieces, far head to merge piece: A [(36, 2)], B []
- between far head and merge x_lm (exclusive both): A [] (0), B [] (0)
- x_hwA=37, x_hwB=37, x_lm=35: x_hwA-x_lm=2, x_hwB-x_lm=2
- x_sl=50, x_ls=47: x_sl-x_ls=3
- x_nw=19, x_lm-x_nw=16, prio_tiles=14

**d. Branch crossings (x per row; working coords)**

| route | y=1 | y=2 | y=3 | y=4 |
|---|---|---|---|---|
| ES | 37, 38 | 38, 39 | 39 | 39 |
| NE | 44, 45, 46, 47, 48, 49, 50, 51, 52 | 43, 44 | 42, 43 | 41, 42 |

**ASCII** (layout coords; working x = col + 12, y = row + -1); doubled section rows:

```
. . . . . . . . . . . . . . . . . . . . . . . . . \s\ . . . . . / ->- ->- - ->- /w. . 
- - \n. . . . . / -># -># -># -># -># -># -># . /eH =s= = = = =w= H -<# -<# -<- -<- - 
. . \ -<- -<- -<# -<# -<# -<# -<# -<# -<# -<# -<# H = =v= = = = = H -</ /e/ . . . . . 
. . . . . . . . \n- -<- - -<- -<- -<- -<\ . . . . . . | . / /w. . . . /e/ . . . . . . 
```

## S1 / forced-gap1
`CONFIG forced-gap1 style=1 clean=1 has=1 height=15 cross=0 e_slack=1 w_slack=1 margin=1`

Violations: none

`{'GAP': 1, 'SBBACK': 4, 'h_w': 37, 'h_e': 45, 'w_row': 2, 'b_row': 3, 'merge_x': 34, 'x_ls': 48, 'x_sb': 15, 'nw_max_x': 19}` shift=(13, -1)

Lane A moves: `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFB7FRFL`

Lane B moves: `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFRFLFB7FFFFFFFFFFFFFFFFFFFFFLFRFFFFFFFFFFFFFF`

**b. Positions (working x; travel is -x)**

| item | x | note |
|---|---|---|
| W->S start piece | 51 | y=2 axis; first piece leaving the axis (SL split tile) x=50 |
| lane split (B first non-axis) | 48 | tile cols of bend [(35, 48, 2, 3, 0, 0), (36, 48, 3, 2, 0, 1)] |
| lane A S-bend #1 | cols [35] | rows [2, 3], idx 48..49 (LoE UpE ) |
| lane B S-bend #1 | cols [48] | rows [2, 3], idx 35..36 (LoE UpE ) |
| lane B S-bend #2 | cols [15] | rows [2, 3], idx 69..70 (LfN RtN ) |
| near head A / B | 45 / 45 | rows y=2 / 3; idx 38 / 39 |
| far head A / B | 37 / 37 | idx 46 / 47 |
| lane merge x_lm (merge_x) | 34 | A last piece (34, 3); B axis piece there |
| N->W join (last NW piece) | 18 | y=3 td=NE; nw_max_x=19 |
| shift-back S-bend x_sb (meta) | 15 | B bend #2 cols [15] |

S-bend extents (axis before -> axis after): A: axis 36->34, step 2, bend cols [35], rows [2, 3]; B: axis 49->47, step 2, bend cols [48], rows [2, 3]; B: axis 16->14, step 2, bend cols [15], rows [2, 3]

**a. Pieces, x in [11,54]** (cols: idx | x,y | trackdir | B=bridge | L=signal-legal | S pre-removal | S post-removal; > with, < against travel)

Lane A:

| idx | x,y | td | br | leg | sig pre | sig post |
|---|---|---|---|---|---|---|
| 29 | 54,2 | NE |  | L |  |  |
| 30 | 53,2 | NE |  | L | S> | S> |
| 31 | 52,2 | NE |  | L |  |  |
| 32 | 51,2 | NE |  | L | S> | S> |
| 33 | 50,2 | NE |  |  |  |  |
| 34 | 49,2 | NE |  | L | S> | S> |
| 35 | 48,2 | NE |  |  |  |  |
| 36 | 47,2 | NE |  | L | S> |  |
| 37 | 46,2 | NE |  | L | S> | S> |
| 38 | 45,2 | NE | B |  |  |  |
| 39 | 44,2 | NE | B |  |  |  |
| 40 | 43,2 | NE | B |  |  |  |
| 41 | 42,2 | NE | B |  |  |  |
| 42 | 41,2 | NE | B |  |  |  |
| 43 | 40,2 | NE | B |  |  |  |
| 44 | 39,2 | NE | B |  |  |  |
| 45 | 38,2 | NE | B |  |  |  |
| 46 | 37,2 | NE | B |  |  |  |
| 47 | 36,2 | NE |  | L | S> | S> |
| 48 | 35,2 | LoE (half) |  | L | S> |  |
| 49 | 35,3 | UpE (half) |  |  |  |  |
| 50 | 34,3 | NE |  | L | S> | S> |

Lane B:

| idx | x,y | td | br | leg | sig pre | sig post |
|---|---|---|---|---|---|---|
| 29 | 54,2 | NE |  | L |  |  |
| 30 | 53,2 | NE |  | L | S> | S> |
| 31 | 52,2 | NE |  | L |  |  |
| 32 | 51,2 | NE |  | L | S> | S> |
| 33 | 50,2 | NE |  |  |  |  |
| 34 | 49,2 | NE |  | L | S> | S> |
| 35 | 48,2 | LoE (half) |  |  |  |  |
| 36 | 48,3 | UpE (half) |  | L | S> | S> |
| 37 | 47,3 | NE |  | L |  |  |
| 38 | 46,3 | NE |  | L | S> | S> |
| 39 | 45,3 | NE | B |  |  |  |
| 40 | 44,3 | NE | B |  |  |  |
| 41 | 43,3 | NE | B |  |  |  |
| 42 | 42,3 | NE | B |  |  |  |
| 43 | 41,3 | NE | B |  |  |  |
| 44 | 40,3 | NE | B |  |  |  |
| 45 | 39,3 | NE | B |  |  |  |
| 46 | 38,3 | NE | B |  |  |  |
| 47 | 37,3 | NE | B |  |  |  |
| 48 | 36,3 | NE |  | L | S> | S> |
| 49 | 35,3 | NE |  |  |  |  |
| 50 | 34,3 | NE |  | L | S> | S> |
| 51 | 33,3 | NE |  |  |  |  |
| 52 | 32,3 | NE |  | L | S> | S> |
| 53 | 31,3 | NE |  |  |  |  |
| 54 | 30,3 | NE |  | L | S> | S> |
| 55 | 29,3 | NE |  |  |  |  |
| 56 | 28,3 | NE |  | L | S> | S> |
| 57 | 27,3 | NE |  |  |  |  |
| 58 | 26,3 | NE |  | L | S> | S> |
| 59 | 25,3 | NE |  |  |  |  |
| 60 | 24,3 | NE |  | L | S> | S> |
| 61 | 23,3 | NE |  |  |  |  |
| 62 | 22,3 | NE |  | L | S> | S> |
| 63 | 21,3 | NE |  |  |  |  |
| 64 | 20,3 | NE |  | L | S> | S> |
| 65 | 19,3 | NE |  |  |  |  |
| 66 | 18,3 | NE |  | L | S> | S> |
| 67 | 17,3 | NE |  | L |  |  |
| 68 | 16,3 | NE |  | L | S> | S> |
| 69 | 15,3 | LfN (half) |  | L |  |  |
| 70 | 15,2 | RtN (half) |  | L | S> | S> |
| 71 | 14,2 | NE |  | L |  |  |
| 72 | 13,2 | NE |  | L |  |  |
| 73 | 12,2 | NE |  | L |  |  |
| 74 | 11,2 | NE |  | L |  |  |

Branch routes (pass 2) pieces in the same x range:

- WS: (51,2)NE (50,2)LoE (50,3)UpE (49,3)LoE (49,4)UpE (48,4)LoE (48,5)UpE (47,5)LoE (47,6)UpE (46,6)LoE (46,7)SE (46,8)SE (46,9)UpE (45,9)LoE (45,10)UpE (44,10)LoE (44,11)UpE (43,11)LoE (43,12)UpE (42,12)LoE (42,13)SE
- ES: (36,0)SW (37,0)RtS (37,1)LfS (38,1)RtS (38,2)LfS (39,2)RtS (39,3)SE (39,4)SE (39,5)SE (39,6)SE (39,7)SE (39,8)LfS (40,8)RtS (40,9)LfS (41,9)RtS (41,10)LfS (42,10)RtS (42,11)SE (42,12)SE
- NE: (40,12)NW (40,11)NW (40,10)NW (40,9)NW (40,8)NW (40,7)NW (40,6)NW (40,5)LoW (41,5)UpW (41,4)LoW (42,4)UpW (42,3)LoW (43,3)UpW (43,2)LoW (44,2)UpW (44,1)LoW (45,1)SW (46,1)SW (47,1)SW (48,1)SW (49,1)SW (50,1)SW (51,1)SW (52,1)UpW (52,0)LoW (53,0)SW
- NW: (40,13)NW (40,12)RtN (39,12)LfN (39,11)RtN (38,11)LfN (38,10)RtN (37,10)LfN (37,9)RtN (36,9)LfN (36,8)RtN (35,8)LfN (35,7)RtN (34,7)LfN (34,6)RtN (33,6)LfN (33,5)RtN (32,5)LfN (32,4)RtN (31,4)NE (30,4)NE (29,4)NE (28,4)NE (27,4)NE (26,4)NE (25,4)NE (24,4)NE (23,4)NE (22,4)NE (21,4)NE (20,4)NE (19,4)LfN (19,3)RtN (18,3)NE

**c. Derived**

- x_ls=48; x_heA=45, x_heB=45; x_ls-x_heA=3, x_ls-x_heB=3
- signal-legal plain axis pieces between lane split (x_ls, exclusive) and near head: A [47, 46] (2), B [47, 46] (2)
- all signal-legal pieces (incl. half pieces), lane split to near head: A [(47, 2), (46, 2)], B [(48, 3), (47, 3), (46, 3)] (B counts from after its first half piece; A from after the split tile)
- all signal-legal pieces, far head to merge piece: A [(36, 2), (35, 2)], B [(36, 3)]
- between far head and merge x_lm (exclusive both): A [36] (1), B [36] (1)
- x_hwA=37, x_hwB=37, x_lm=34: x_hwA-x_lm=3, x_hwB-x_lm=3
- x_sl=51, x_ls=48: x_sl-x_ls=3
- x_nw=18, x_lm-x_nw=16, prio_tiles=14

**d. Branch crossings (x per row; working coords)**

| route | y=1 | y=2 | y=3 | y=4 |
|---|---|---|---|---|
| ES | 37, 38 | 38, 39 | 39 | 39 |
| NE | 44, 45, 46, 47, 48, 49, 50, 51, 52 | 43, 44 | 42, 43 | 41, 42 |

**ASCII** (layout coords; working x = col + 13, y = row + -1); doubled section rows:

```
. . . . . . . . . . . . . . . . . . . . . . . . \s\ . . . . . / ->- ->- - ->- /w. . 
- - \n. . . / -># -># -># -># -># -># -># . / -<H =s= = = = =w= H -<- # -<# -<- -<- 
. . \ -<- -<# -<# -<# -<# -<# -<# -<# -<# -<# -<H = =v= = = = = H -<- /e/e/ . . . . 
. . . . . . \n- -<- - -<- -<- -<- -<- \n. . . . . . | . / /w. . . . . /e/ . . . . . 
```

## S1 / forced-gap2
`CONFIG forced-gap2 style=1 clean=1 has=1 height=15 cross=0 e_slack=1 w_slack=1 margin=1`

Violations: none

`{'GAP': 2, 'SBBACK': 4, 'h_w': 37, 'h_e': 45, 'w_row': 2, 'b_row': 3, 'merge_x': 34, 'x_ls': 49, 'x_sb': 15, 'nw_max_x': 19}` shift=(13, -1)

Lane A moves: `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFB7FRFL`

Lane B moves: `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFRFLFFB7FFFFFFFFFFFFFFFFFFFFFLFRFFFFFFFFFFFFFF`

**b. Positions (working x; travel is -x)**

| item | x | note |
|---|---|---|
| W->S start piece | 52 | y=2 axis; first piece leaving the axis (SL split tile) x=51 |
| lane split (B first non-axis) | 49 | tile cols of bend [(34, 49, 2, 3, 0, 0), (35, 49, 3, 2, 0, 1)] |
| lane A S-bend #1 | cols [35] | rows [2, 3], idx 48..49 (LoE UpE ) |
| lane B S-bend #1 | cols [49] | rows [2, 3], idx 34..35 (LoE UpE ) |
| lane B S-bend #2 | cols [15] | rows [2, 3], idx 69..70 (LfN RtN ) |
| near head A / B | 45 / 45 | rows y=2 / 3; idx 38 / 39 |
| far head A / B | 37 / 37 | idx 46 / 47 |
| lane merge x_lm (merge_x) | 34 | A last piece (34, 3); B axis piece there |
| N->W join (last NW piece) | 18 | y=3 td=NE; nw_max_x=19 |
| shift-back S-bend x_sb (meta) | 15 | B bend #2 cols [15] |

S-bend extents (axis before -> axis after): A: axis 36->34, step 2, bend cols [35], rows [2, 3]; B: axis 50->48, step 2, bend cols [49], rows [2, 3]; B: axis 16->14, step 2, bend cols [15], rows [2, 3]

**a. Pieces, x in [11,55]** (cols: idx | x,y | trackdir | B=bridge | L=signal-legal | S pre-removal | S post-removal; > with, < against travel)

Lane A:

| idx | x,y | td | br | leg | sig pre | sig post |
|---|---|---|---|---|---|---|
| 28 | 55,2 | NE |  | L |  |  |
| 29 | 54,2 | NE |  | L |  |  |
| 30 | 53,2 | NE |  | L |  |  |
| 31 | 52,2 | NE |  | L | S> | S> |
| 32 | 51,2 | NE |  |  |  |  |
| 33 | 50,2 | NE |  | L | S> | S> |
| 34 | 49,2 | NE |  |  |  |  |
| 35 | 48,2 | NE |  | L | S> | S> |
| 36 | 47,2 | NE |  | L |  |  |
| 37 | 46,2 | NE |  | L | S> | S> |
| 38 | 45,2 | NE | B |  |  |  |
| 39 | 44,2 | NE | B |  |  |  |
| 40 | 43,2 | NE | B |  |  |  |
| 41 | 42,2 | NE | B |  |  |  |
| 42 | 41,2 | NE | B |  |  |  |
| 43 | 40,2 | NE | B |  |  |  |
| 44 | 39,2 | NE | B |  |  |  |
| 45 | 38,2 | NE | B |  |  |  |
| 46 | 37,2 | NE | B |  |  |  |
| 47 | 36,2 | NE |  | L | S> | S> |
| 48 | 35,2 | LoE (half) |  | L | S> |  |
| 49 | 35,3 | UpE (half) |  |  |  |  |
| 50 | 34,3 | NE |  | L | S> | S> |

Lane B:

| idx | x,y | td | br | leg | sig pre | sig post |
|---|---|---|---|---|---|---|
| 28 | 55,2 | NE |  | L |  |  |
| 29 | 54,2 | NE |  | L |  |  |
| 30 | 53,2 | NE |  | L |  |  |
| 31 | 52,2 | NE |  | L | S> | S> |
| 32 | 51,2 | NE |  |  |  |  |
| 33 | 50,2 | NE |  | L | S> | S> |
| 34 | 49,2 | LoE (half) |  |  |  |  |
| 35 | 49,3 | UpE (half) |  | L | S> |  |
| 36 | 48,3 | NE |  | L | S> | S> |
| 37 | 47,3 | NE |  | L |  |  |
| 38 | 46,3 | NE |  | L | S> | S> |
| 39 | 45,3 | NE | B |  |  |  |
| 40 | 44,3 | NE | B |  |  |  |
| 41 | 43,3 | NE | B |  |  |  |
| 42 | 42,3 | NE | B |  |  |  |
| 43 | 41,3 | NE | B |  |  |  |
| 44 | 40,3 | NE | B |  |  |  |
| 45 | 39,3 | NE | B |  |  |  |
| 46 | 38,3 | NE | B |  |  |  |
| 47 | 37,3 | NE | B |  |  |  |
| 48 | 36,3 | NE |  | L | S> | S> |
| 49 | 35,3 | NE |  |  |  |  |
| 50 | 34,3 | NE |  | L | S> | S> |
| 51 | 33,3 | NE |  |  |  |  |
| 52 | 32,3 | NE |  | L | S> | S> |
| 53 | 31,3 | NE |  |  |  |  |
| 54 | 30,3 | NE |  | L | S> | S> |
| 55 | 29,3 | NE |  |  |  |  |
| 56 | 28,3 | NE |  | L | S> | S> |
| 57 | 27,3 | NE |  |  |  |  |
| 58 | 26,3 | NE |  | L | S> | S> |
| 59 | 25,3 | NE |  |  |  |  |
| 60 | 24,3 | NE |  | L | S> | S> |
| 61 | 23,3 | NE |  |  |  |  |
| 62 | 22,3 | NE |  | L | S> | S> |
| 63 | 21,3 | NE |  |  |  |  |
| 64 | 20,3 | NE |  | L | S> | S> |
| 65 | 19,3 | NE |  |  |  |  |
| 66 | 18,3 | NE |  | L | S> | S> |
| 67 | 17,3 | NE |  | L |  |  |
| 68 | 16,3 | NE |  | L | S> | S> |
| 69 | 15,3 | LfN (half) |  | L |  |  |
| 70 | 15,2 | RtN (half) |  | L | S> | S> |
| 71 | 14,2 | NE |  | L |  |  |
| 72 | 13,2 | NE |  | L |  |  |
| 73 | 12,2 | NE |  | L |  |  |
| 74 | 11,2 | NE |  | L |  |  |

Branch routes (pass 2) pieces in the same x range:

- WS: (52,2)NE (51,2)LoE (51,3)UpE (50,3)LoE (50,4)UpE (49,4)LoE (49,5)UpE (48,5)LoE (48,6)UpE (47,6)LoE (47,7)UpE (46,7)LoE (46,8)SE (46,9)UpE (45,9)LoE (45,10)UpE (44,10)LoE (44,11)UpE (43,11)LoE (43,12)UpE (42,12)LoE (42,13)SE
- ES: (36,0)SW (37,0)RtS (37,1)LfS (38,1)RtS (38,2)LfS (39,2)RtS (39,3)SE (39,4)SE (39,5)SE (39,6)SE (39,7)SE (39,8)LfS (40,8)RtS (40,9)LfS (41,9)RtS (41,10)LfS (42,10)RtS (42,11)SE (42,12)SE
- NE: (40,12)NW (40,11)NW (40,10)NW (40,9)NW (40,8)NW (40,7)NW (40,6)NW (40,5)LoW (41,5)UpW (41,4)LoW (42,4)UpW (42,3)LoW (43,3)UpW (43,2)LoW (44,2)UpW (44,1)LoW (45,1)SW (46,1)SW (47,1)SW (48,1)SW (49,1)SW (50,1)SW (51,1)SW (52,1)UpW (52,0)LoW (53,0)SW
- NW: (40,13)NW (40,12)RtN (39,12)LfN (39,11)RtN (38,11)LfN (38,10)RtN (37,10)LfN (37,9)RtN (36,9)LfN (36,8)RtN (35,8)LfN (35,7)RtN (34,7)LfN (34,6)RtN (33,6)LfN (33,5)RtN (32,5)LfN (32,4)RtN (31,4)NE (30,4)NE (29,4)NE (28,4)NE (27,4)NE (26,4)NE (25,4)NE (24,4)NE (23,4)NE (22,4)NE (21,4)NE (20,4)NE (19,4)LfN (19,3)RtN (18,3)NE

**c. Derived**

- x_ls=49; x_heA=45, x_heB=45; x_ls-x_heA=4, x_ls-x_heB=4
- signal-legal plain axis pieces between lane split (x_ls, exclusive) and near head: A [48, 47, 46] (3), B [48, 47, 46] (3)
- all signal-legal pieces (incl. half pieces), lane split to near head: A [(48, 2), (47, 2), (46, 2)], B [(49, 3), (48, 3), (47, 3), (46, 3)] (B counts from after its first half piece; A from after the split tile)
- all signal-legal pieces, far head to merge piece: A [(36, 2), (35, 2)], B [(36, 3)]
- between far head and merge x_lm (exclusive both): A [36] (1), B [36] (1)
- x_hwA=37, x_hwB=37, x_lm=34: x_hwA-x_lm=3, x_hwB-x_lm=3
- x_sl=52, x_ls=49: x_sl-x_ls=3
- x_nw=18, x_lm-x_nw=16, prio_tiles=14

**d. Branch crossings (x per row; working coords)**

| route | y=1 | y=2 | y=3 | y=4 |
|---|---|---|---|---|
| ES | 37, 38 | 38, 39 | 39 | 39 |
| NE | 44, 45, 46, 47, 48, 49, 50, 51, 52 | 43, 44 | 42, 43 | 41, 42 |

**ASCII** (layout coords; working x = col + 13, y = row + -1); doubled section rows:

```
. . . . . . . . . . . . . . . . . . . . . . . . \s\ . . . . . / ->- ->- - ->- /w. . 
- - \n. . . / -># -># -># -># -># -># -># . / -<H =s= = = = =w= H -<- -<# -<# -<- - 
. . \ -<- -<# -<# -<# -<# -<# -<# -<# -<# -<# -<H = =v= = = = = H -<- -</ /e/ . . . 
. . . . . . \n- -<- - -<- -<- -<- -<- \n. . . . . . | . / /w. . . . . . /e/ . . . . 
```

# SPEC S2 tl=5 prio=15 best_hub=31,1
WINNER height=13 cross=0 e_slack=2 w_slack=1 margin=3 style=1


## S2 / flipped-style
`CONFIG flipped-style style=0 clean=0 has=0 height=13 cross=0 e_slack=2 w_slack=1 margin=3`

No candidate built with these params.


## S2 / winning-style
`CONFIG winning-style style=1 clean=1 has=1 height=13 cross=0 e_slack=2 w_slack=1 margin=3`

`{'GAP': 1, 'SBBACK': 5, 'h_w': 35, 'h_e': 45, 'w_row': 2, 'b_row': 3, 'merge_x': 32, 'x_ls': 48, 'x_sb': 11, 'nw_max_x': 16}` shift=(9, -1)

Lane A moves: `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFB9FRFL`

Lane B moves: `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFRFLFB9FFFFFFFFFFFFFFFFFFFFFFFLFRFFFFFFFFFF`

**b. Positions (working x; travel is -x)**

| item | x | note |
|---|---|---|
| W->S start piece | 51 | y=2 axis; first piece leaving the axis (SL split tile) x=50 |
| lane split (B first non-axis) | 48 | tile cols of bend [(35, 48, 2, 3, 0, 0), (36, 48, 3, 2, 0, 1)] |
| lane A S-bend #1 | cols [33] | rows [2, 3], idx 50..51 (LoE UpE ) |
| lane B S-bend #1 | cols [48] | rows [2, 3], idx 35..36 (LoE UpE ) |
| lane B S-bend #2 | cols [11] | rows [2, 3], idx 73..74 (LfN RtN ) |
| near head A / B | 45 / 45 | rows y=2 / 3; idx 38 / 39 |
| far head A / B | 35 / 35 | idx 48 / 49 |
| lane merge x_lm (merge_x) | 32 | A last piece (32, 3); B axis piece there |
| N->W join (last NW piece) | 15 | y=3 td=NE; nw_max_x=16 |
| shift-back S-bend x_sb (meta) | 11 | B bend #2 cols [11] |

S-bend extents (axis before -> axis after): A: axis 34->32, step 2, bend cols [33], rows [2, 3]; B: axis 49->47, step 2, bend cols [48], rows [2, 3]; B: axis 12->10, step 2, bend cols [11], rows [2, 3]

**a. Pieces, x in [7,54]** (cols: idx | x,y | trackdir | B=bridge | L=signal-legal | S pre-removal | S post-removal; > with, < against travel)

Lane A:

| idx | x,y | td | br | leg | sig pre | sig post |
|---|---|---|---|---|---|---|
| 29 | 54,2 | NE |  | L |  |  |
| 30 | 53,2 | NE |  | L | S> | S> |
| 31 | 52,2 | NE |  | L |  |  |
| 32 | 51,2 | NE |  | L | S> | S> |
| 33 | 50,2 | NE |  |  |  |  |
| 34 | 49,2 | NE |  | L | S> | S> |
| 35 | 48,2 | NE |  |  |  |  |
| 36 | 47,2 | NE |  | L | S> |  |
| 37 | 46,2 | NE |  | L | S> | S> |
| 38 | 45,2 | NE | B |  |  |  |
| 39 | 44,2 | NE | B |  |  |  |
| 40 | 43,2 | NE | B |  |  |  |
| 41 | 42,2 | NE | B |  |  |  |
| 42 | 41,2 | NE | B |  |  |  |
| 43 | 40,2 | NE | B |  |  |  |
| 44 | 39,2 | NE | B |  |  |  |
| 45 | 38,2 | NE | B |  |  |  |
| 46 | 37,2 | NE | B |  |  |  |
| 47 | 36,2 | NE | B |  |  |  |
| 48 | 35,2 | NE | B |  |  |  |
| 49 | 34,2 | NE |  | L | S> | S> |
| 50 | 33,2 | LoE (half) |  | L | S> |  |
| 51 | 33,3 | UpE (half) |  |  |  |  |
| 52 | 32,3 | NE |  | L | S> | S> |

Lane B:

| idx | x,y | td | br | leg | sig pre | sig post |
|---|---|---|---|---|---|---|
| 29 | 54,2 | NE |  | L |  |  |
| 30 | 53,2 | NE |  | L | S> | S> |
| 31 | 52,2 | NE |  | L |  |  |
| 32 | 51,2 | NE |  | L | S> | S> |
| 33 | 50,2 | NE |  |  |  |  |
| 34 | 49,2 | NE |  | L | S> | S> |
| 35 | 48,2 | LoE (half) |  |  |  |  |
| 36 | 48,3 | UpE (half) |  | L | S> | S> |
| 37 | 47,3 | NE |  | L |  |  |
| 38 | 46,3 | NE |  | L | S> | S> |
| 39 | 45,3 | NE | B |  |  |  |
| 40 | 44,3 | NE | B |  |  |  |
| 41 | 43,3 | NE | B |  |  |  |
| 42 | 42,3 | NE | B |  |  |  |
| 43 | 41,3 | NE | B |  |  |  |
| 44 | 40,3 | NE | B |  |  |  |
| 45 | 39,3 | NE | B |  |  |  |
| 46 | 38,3 | NE | B |  |  |  |
| 47 | 37,3 | NE | B |  |  |  |
| 48 | 36,3 | NE | B |  |  |  |
| 49 | 35,3 | NE | B |  |  |  |
| 50 | 34,3 | NE |  | L | S> | S> |
| 51 | 33,3 | NE |  |  |  |  |
| 52 | 32,3 | NE |  | L | S> | S> |
| 53 | 31,3 | NE |  | L |  |  |
| 54 | 30,3 | NE |  |  |  |  |
| 55 | 29,3 | NE |  | L | S> | S> |
| 56 | 28,3 | NE |  |  |  |  |
| 57 | 27,3 | NE |  | L | S> | S> |
| 58 | 26,3 | NE |  |  |  |  |
| 59 | 25,3 | NE |  | L | S> | S> |
| 60 | 24,3 | NE |  |  |  |  |
| 61 | 23,3 | NE |  | L | S> | S> |
| 62 | 22,3 | NE |  |  |  |  |
| 63 | 21,3 | NE |  | L | S> | S> |
| 64 | 20,3 | NE |  |  |  |  |
| 65 | 19,3 | NE |  | L | S> | S> |
| 66 | 18,3 | NE |  |  |  |  |
| 67 | 17,3 | NE |  | L | S> | S> |
| 68 | 16,3 | NE |  |  |  |  |
| 69 | 15,3 | NE |  | L | S> |  |
| 70 | 14,3 | NE |  | L | S> | S> |
| 71 | 13,3 | NE |  | L |  |  |
| 72 | 12,3 | NE |  | L | S> | S> |
| 73 | 11,3 | LfN (half) |  | L |  |  |
| 74 | 11,2 | RtN (half) |  | L | S> | S> |
| 75 | 10,2 | NE |  | L |  |  |
| 76 | 9,2 | NE |  | L |  |  |
| 77 | 8,2 | NE |  | L |  |  |
| 78 | 7,2 | NE |  | L |  |  |

Branch routes (pass 2) pieces in the same x range:

- WS: (51,2)NE (50,2)LoE (50,3)UpE (49,3)LoE (49,4)UpE (48,4)LoE (48,5)UpE (47,5)LoE (47,6)UpE (46,6)LoE (46,7)UpE (45,7)LoE (45,8)UpE (44,8)LoE (44,9)UpE (43,9)LoE (43,10)UpE (42,10)LoE (42,11)SE
- ES: (36,0)SW (37,0)RtS (37,1)LfS (38,1)RtS (38,2)LfS (39,2)RtS (39,3)SE (39,4)SE (39,5)SE (39,6)LfS (40,6)RtS (40,7)LfS (41,7)RtS (41,8)LfS (42,8)RtS (42,9)SE (42,10)SE
- NE: (40,10)NW (40,9)NW (40,8)NW (40,7)NW (40,6)NW (40,5)NW (40,4)NW (40,3)LoW (41,3)UpW (41,2)LoW (42,2)UpW (42,1)LoW (43,1)SW (44,1)SW (45,1)SW (46,1)SW (47,1)SW (48,1)SW (49,1)SW (50,1)SW (51,1)SW (52,1)SW (53,1)SW (54,1)UpW (54,0)LoW (55,0)SW
- NW: (40,11)NW (40,10)RtN (39,10)LfN (39,9)RtN (38,9)LfN (38,8)RtN (37,8)LfN (37,7)RtN (36,7)LfN (36,6)RtN (35,6)LfN (35,5)RtN (34,5)LfN (34,4)RtN (33,4)NE (32,4)NE (31,4)NE (30,4)NE (29,4)NE (28,4)NE (27,4)NE (26,4)NE (25,4)NE (24,4)NE (23,4)NE (22,4)NE (21,4)NE (20,4)NE (19,4)NE (18,4)NE (17,4)NE (16,4)LfN (16,3)RtN (15,3)NE

**c. Derived**

- x_ls=48; x_heA=45, x_heB=45; x_ls-x_heA=3, x_ls-x_heB=3
- signal-legal plain axis pieces between lane split (x_ls, exclusive) and near head: A [47, 46] (2), B [47, 46] (2)
- all signal-legal pieces (incl. half pieces), lane split to near head: A [(47, 2), (46, 2)], B [(48, 3), (47, 3), (46, 3)] (B counts from after its first half piece; A from after the split tile)
- all signal-legal pieces, far head to merge piece: A [(34, 2), (33, 2)], B [(34, 3)]
- between far head and merge x_lm (exclusive both): A [34] (1), B [34] (1)
- x_hwA=35, x_hwB=35, x_lm=32: x_hwA-x_lm=3, x_hwB-x_lm=3
- x_sl=51, x_ls=48: x_sl-x_ls=3
- x_nw=15, x_lm-x_nw=17, prio_tiles=15

**d. Branch crossings (x per row; working coords)**

| route | y=1 | y=2 | y=3 | y=4 |
|---|---|---|---|---|
| ES | 37, 38 | 38, 39 | 39 | 39 |
| NE | 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54 | 41, 42 | 40, 41 | 40 |

**ASCII** (layout coords; working x = col + 9, y = row + -1); doubled section rows:

```
. . . . . . . . . . . . . . . . . . . . . . . . . . . . \s\ . . . / ->- - ->- ->- ->- ->- /w. . 
- - \n. . . . / -># -># -># -># -># -># -># . . / -<H = = = =s= = =w= = H -<- # -<# -<- -<- -<- 
. . \ -<- -<- # -<# -<# -<# -<# -<# -<# -<# - -<# -<H = = = = = = = = = H -<- /e/e/ . . . . . . 
. . . . . . . \n- -<- -<- -<- -<- - -<- -<- - -<- \n. . . . |v|^. . . . . . . / / . . . . . . . 
```

## S2 / forced-gap0
`CONFIG forced-gap0 style=1 clean=0 has=0 height=13 cross=0 e_slack=2 w_slack=1 margin=3`

Violations: none

No candidate built with these params.


## S2 / forced-gap1
`CONFIG forced-gap1 style=1 clean=1 has=1 height=13 cross=0 e_slack=2 w_slack=1 margin=3`

Violations: none

`{'GAP': 1, 'SBBACK': 5, 'h_w': 35, 'h_e': 45, 'w_row': 2, 'b_row': 3, 'merge_x': 32, 'x_ls': 48, 'x_sb': 11, 'nw_max_x': 16}` shift=(9, -1)

Lane A moves: `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFB9FRFL`

Lane B moves: `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFRFLFB9FFFFFFFFFFFFFFFFFFFFFFFLFRFFFFFFFFFF`

**b. Positions (working x; travel is -x)**

| item | x | note |
|---|---|---|
| W->S start piece | 51 | y=2 axis; first piece leaving the axis (SL split tile) x=50 |
| lane split (B first non-axis) | 48 | tile cols of bend [(35, 48, 2, 3, 0, 0), (36, 48, 3, 2, 0, 1)] |
| lane A S-bend #1 | cols [33] | rows [2, 3], idx 50..51 (LoE UpE ) |
| lane B S-bend #1 | cols [48] | rows [2, 3], idx 35..36 (LoE UpE ) |
| lane B S-bend #2 | cols [11] | rows [2, 3], idx 73..74 (LfN RtN ) |
| near head A / B | 45 / 45 | rows y=2 / 3; idx 38 / 39 |
| far head A / B | 35 / 35 | idx 48 / 49 |
| lane merge x_lm (merge_x) | 32 | A last piece (32, 3); B axis piece there |
| N->W join (last NW piece) | 15 | y=3 td=NE; nw_max_x=16 |
| shift-back S-bend x_sb (meta) | 11 | B bend #2 cols [11] |

S-bend extents (axis before -> axis after): A: axis 34->32, step 2, bend cols [33], rows [2, 3]; B: axis 49->47, step 2, bend cols [48], rows [2, 3]; B: axis 12->10, step 2, bend cols [11], rows [2, 3]

**a. Pieces, x in [7,54]** (cols: idx | x,y | trackdir | B=bridge | L=signal-legal | S pre-removal | S post-removal; > with, < against travel)

Lane A:

| idx | x,y | td | br | leg | sig pre | sig post |
|---|---|---|---|---|---|---|
| 29 | 54,2 | NE |  | L |  |  |
| 30 | 53,2 | NE |  | L | S> | S> |
| 31 | 52,2 | NE |  | L |  |  |
| 32 | 51,2 | NE |  | L | S> | S> |
| 33 | 50,2 | NE |  |  |  |  |
| 34 | 49,2 | NE |  | L | S> | S> |
| 35 | 48,2 | NE |  |  |  |  |
| 36 | 47,2 | NE |  | L | S> |  |
| 37 | 46,2 | NE |  | L | S> | S> |
| 38 | 45,2 | NE | B |  |  |  |
| 39 | 44,2 | NE | B |  |  |  |
| 40 | 43,2 | NE | B |  |  |  |
| 41 | 42,2 | NE | B |  |  |  |
| 42 | 41,2 | NE | B |  |  |  |
| 43 | 40,2 | NE | B |  |  |  |
| 44 | 39,2 | NE | B |  |  |  |
| 45 | 38,2 | NE | B |  |  |  |
| 46 | 37,2 | NE | B |  |  |  |
| 47 | 36,2 | NE | B |  |  |  |
| 48 | 35,2 | NE | B |  |  |  |
| 49 | 34,2 | NE |  | L | S> | S> |
| 50 | 33,2 | LoE (half) |  | L | S> |  |
| 51 | 33,3 | UpE (half) |  |  |  |  |
| 52 | 32,3 | NE |  | L | S> | S> |

Lane B:

| idx | x,y | td | br | leg | sig pre | sig post |
|---|---|---|---|---|---|---|
| 29 | 54,2 | NE |  | L |  |  |
| 30 | 53,2 | NE |  | L | S> | S> |
| 31 | 52,2 | NE |  | L |  |  |
| 32 | 51,2 | NE |  | L | S> | S> |
| 33 | 50,2 | NE |  |  |  |  |
| 34 | 49,2 | NE |  | L | S> | S> |
| 35 | 48,2 | LoE (half) |  |  |  |  |
| 36 | 48,3 | UpE (half) |  | L | S> | S> |
| 37 | 47,3 | NE |  | L |  |  |
| 38 | 46,3 | NE |  | L | S> | S> |
| 39 | 45,3 | NE | B |  |  |  |
| 40 | 44,3 | NE | B |  |  |  |
| 41 | 43,3 | NE | B |  |  |  |
| 42 | 42,3 | NE | B |  |  |  |
| 43 | 41,3 | NE | B |  |  |  |
| 44 | 40,3 | NE | B |  |  |  |
| 45 | 39,3 | NE | B |  |  |  |
| 46 | 38,3 | NE | B |  |  |  |
| 47 | 37,3 | NE | B |  |  |  |
| 48 | 36,3 | NE | B |  |  |  |
| 49 | 35,3 | NE | B |  |  |  |
| 50 | 34,3 | NE |  | L | S> | S> |
| 51 | 33,3 | NE |  |  |  |  |
| 52 | 32,3 | NE |  | L | S> | S> |
| 53 | 31,3 | NE |  | L |  |  |
| 54 | 30,3 | NE |  |  |  |  |
| 55 | 29,3 | NE |  | L | S> | S> |
| 56 | 28,3 | NE |  |  |  |  |
| 57 | 27,3 | NE |  | L | S> | S> |
| 58 | 26,3 | NE |  |  |  |  |
| 59 | 25,3 | NE |  | L | S> | S> |
| 60 | 24,3 | NE |  |  |  |  |
| 61 | 23,3 | NE |  | L | S> | S> |
| 62 | 22,3 | NE |  |  |  |  |
| 63 | 21,3 | NE |  | L | S> | S> |
| 64 | 20,3 | NE |  |  |  |  |
| 65 | 19,3 | NE |  | L | S> | S> |
| 66 | 18,3 | NE |  |  |  |  |
| 67 | 17,3 | NE |  | L | S> | S> |
| 68 | 16,3 | NE |  |  |  |  |
| 69 | 15,3 | NE |  | L | S> |  |
| 70 | 14,3 | NE |  | L | S> | S> |
| 71 | 13,3 | NE |  | L |  |  |
| 72 | 12,3 | NE |  | L | S> | S> |
| 73 | 11,3 | LfN (half) |  | L |  |  |
| 74 | 11,2 | RtN (half) |  | L | S> | S> |
| 75 | 10,2 | NE |  | L |  |  |
| 76 | 9,2 | NE |  | L |  |  |
| 77 | 8,2 | NE |  | L |  |  |
| 78 | 7,2 | NE |  | L |  |  |

Branch routes (pass 2) pieces in the same x range:

- WS: (51,2)NE (50,2)LoE (50,3)UpE (49,3)LoE (49,4)UpE (48,4)LoE (48,5)UpE (47,5)LoE (47,6)UpE (46,6)LoE (46,7)UpE (45,7)LoE (45,8)UpE (44,8)LoE (44,9)UpE (43,9)LoE (43,10)UpE (42,10)LoE (42,11)SE
- ES: (36,0)SW (37,0)RtS (37,1)LfS (38,1)RtS (38,2)LfS (39,2)RtS (39,3)SE (39,4)SE (39,5)SE (39,6)LfS (40,6)RtS (40,7)LfS (41,7)RtS (41,8)LfS (42,8)RtS (42,9)SE (42,10)SE
- NE: (40,10)NW (40,9)NW (40,8)NW (40,7)NW (40,6)NW (40,5)NW (40,4)NW (40,3)LoW (41,3)UpW (41,2)LoW (42,2)UpW (42,1)LoW (43,1)SW (44,1)SW (45,1)SW (46,1)SW (47,1)SW (48,1)SW (49,1)SW (50,1)SW (51,1)SW (52,1)SW (53,1)SW (54,1)UpW (54,0)LoW (55,0)SW
- NW: (40,11)NW (40,10)RtN (39,10)LfN (39,9)RtN (38,9)LfN (38,8)RtN (37,8)LfN (37,7)RtN (36,7)LfN (36,6)RtN (35,6)LfN (35,5)RtN (34,5)LfN (34,4)RtN (33,4)NE (32,4)NE (31,4)NE (30,4)NE (29,4)NE (28,4)NE (27,4)NE (26,4)NE (25,4)NE (24,4)NE (23,4)NE (22,4)NE (21,4)NE (20,4)NE (19,4)NE (18,4)NE (17,4)NE (16,4)LfN (16,3)RtN (15,3)NE

**c. Derived**

- x_ls=48; x_heA=45, x_heB=45; x_ls-x_heA=3, x_ls-x_heB=3
- signal-legal plain axis pieces between lane split (x_ls, exclusive) and near head: A [47, 46] (2), B [47, 46] (2)
- all signal-legal pieces (incl. half pieces), lane split to near head: A [(47, 2), (46, 2)], B [(48, 3), (47, 3), (46, 3)] (B counts from after its first half piece; A from after the split tile)
- all signal-legal pieces, far head to merge piece: A [(34, 2), (33, 2)], B [(34, 3)]
- between far head and merge x_lm (exclusive both): A [34] (1), B [34] (1)
- x_hwA=35, x_hwB=35, x_lm=32: x_hwA-x_lm=3, x_hwB-x_lm=3
- x_sl=51, x_ls=48: x_sl-x_ls=3
- x_nw=15, x_lm-x_nw=17, prio_tiles=15

**d. Branch crossings (x per row; working coords)**

| route | y=1 | y=2 | y=3 | y=4 |
|---|---|---|---|---|
| ES | 37, 38 | 38, 39 | 39 | 39 |
| NE | 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54 | 41, 42 | 40, 41 | 40 |

**ASCII** (layout coords; working x = col + 9, y = row + -1); doubled section rows:

```
. . . . . . . . . . . . . . . . . . . . . . . . . . . . \s\ . . . / ->- - ->- ->- ->- ->- /w. . 
- - \n. . . . / -># -># -># -># -># -># -># . . / -<H = = = =s= = =w= = H -<- # -<# -<- -<- -<- 
. . \ -<- -<- # -<# -<# -<# -<# -<# -<# -<# - -<# -<H = = = = = = = = = H -<- /e/e/ . . . . . . 
. . . . . . . \n- -<- -<- -<- -<- - -<- -<- - -<- \n. . . . |v|^. . . . . . . / / . . . . . . . 
```

## S2 / forced-gap2
`CONFIG forced-gap2 style=1 clean=0 has=0 height=13 cross=0 e_slack=2 w_slack=1 margin=3`

Violations: none

No candidate built with these params.
