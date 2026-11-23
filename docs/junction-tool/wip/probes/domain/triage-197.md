# Triage of V's PSG 197 domain breaks (domain-39, D393)

Head c2ee5525bd. Dump: `PLANAR_DOMAIN_DUMP_197=<path> build/openttd_test '[planardomain197]'` (env-gated, hidden 197 test). Compare: `python3 -I docs/junction-tool/wip/probes/domain/triage197.py <dump> [corpus root]` (env `TRIAGE_AT=x,y`, `TRIAGE_SPAN=1`, `TRIAGE_LINES=L5,L61`, `TRIAGE_TILES=1` give detail views).

Frame: the test shifts the map frame by the box low corner (156, 274) half tiles = (78, 137) tiles; game tile = break tile + (78, 137). Our tile map and V's agree on 1057 of 1152 tiles in the box (tunnel middles adjusted: our structure piece is not rail track, so it is dropped unless V also has that piece there).

## Counts by the brief's test (any difference on the break's nine tiles = ARTEFACT, else DOMAIN GAP)

ARTEFACT 7, DOMAIN GAP 57 (all of the 64).

| rule | ARTEFACT | DOMAIN GAP |
|---|---|---|
| span | 0 | 13 |
| connection | 2 | 17 |
| diamond | 1 | 15 |
| missing crossing | 0 | 10 |
| head | 0 | 2 |
| node | 4 | 0 |

The nine-tile test is blind for structure rules (a structure's extent and end pieces are not tiles), so the 57 were also traced by reading `planar_domain.cpp` and `planar_domain_map.cpp`. They come from four domain causes (below), not 57 separate situations.

## The 7 artefacts (shared causes)

1. Box edge (3 breaks: node L94, node L96, connection L94/L96 at game (128..129,173)). V's lines continue past the box edge (V has X on (128..130,173..174)); ours stop at the port. Matches "end pins not met: 2" in the answer-key report.
2. A missing V piece next to L69/L70's node (3 breaks: node L69, node L70, connection L69/L70 at game (104..105,179..180)): V has X plus Lower on (104,179), ours only X. V's step/split sits one piece earlier.
3. Diamond L62 x L60 at game (98,180): V has Lower on (99,181), ours nothing. Same region as 2 (the L68/L69/L70 diagonals around (98..104,179..184) where ours has fewer pieces than V), a different V diagonal.

## The 57 domain gaps: four causes

### A. Tunnel end run of 2 pieces on sloped ground (13 span breaks, plus the 3 "no room" notes)
`ends` comes from the test's `v.structures[k].end_run` (tunnel = 2: portal and ramp on flat ground). V's tunnel heads here are on sloped ground (head height differs from the tile outside by 1 in every one of the 13) and sit in the model's inner end-piece position: the domain's outer end piece is the tile outside V's head, and V has a diagonal there (a turn right after the head), so the "straight run" check fails.
Every one of the 13 has a non-straight piece at the outer end-run position:
`L48 3..17 (last 17 = Left turn at (93,175))`, `L80 1..6 (first = Lower)`, `L45 1..6 (last Upper)`, `L46 3..7 (last Upper)`, `L11 20..25 (first Lower)`, `L12 19..25 (first Lower)`, `L11 11..17 (last Upper)`, `L12 11..16 (last Upper)`, `L57 19..28`, `L59 18..27`, `L60 19..28`, `L48 18..27` (first = Right), `L49 6..10 (first Lower)`.
Hand case: a tunnel head tile on sloped ground (a rise of one level across the head), entered straight, with a diagonal (curve) piece directly before the head outside it, and the crossing surface track on the tile directly beside the head (no flat middle between head and crossing). Examples (game tiles): L80 heads (105,164) and (108,164) heights 1/2/2; L48 far head (93,174) h3 with outside tile (93,175) a Left diagonal, crossing X on (93,173) directly inside the head. The game allows it (V's save).
The 3 notes `no room for its end runs` (L5 structure 11, L61 structure 43, L70 structure 48) are the same end-run model meeting a second flaw in `AddStructure`: a crosser "meets" the separator on any shared tile, including the tiles the two share at their common split node (L4/L5 pieces 0..1 at (81..82,173); L69/L70 piece 0 at (104,180)), so `least < ends` and the structure is dropped. V's real crossing is mid-run (L4 at (82,161..162) under L5's tunnel (82,163)->(82,160); L69 X at (100,185) under L70's tunnel (100,184)->(100,186)).

### B. Cascade of the dropped structures (15 diamond + 17 connection = 32 breaks)
A dropped structure (cause A) leaves its pieces Ground, so the crossing tracks' pieces share a tile as X over Y (diamond) and their end points coincide (connection, "4 track ends meet with no node"). Check in the script (`cascade-check` lines): every one of the 32 involves a dropped separator (L48, L80, L45, L46, L11, L12, L57, L59, L60, L49, L5, L61, L70). Not run in the domain itself with the structures forced in (the brief forbids changing it); by reading the code the cascade is the only way these rules fire on tiles equal to V's.

### C. Tunnel under tunnel (8 missing-crossing breaks)
L30 (X tunnel (90,163)->(94,163)) and L31 (X tunnel (90,164)->(94,164)) pass under the three parallel Y tunnels L32 (91,161)->(91,174), L47 (92,161)->(92,174), L48 (93,161)->(93,174): at game tiles (91..93,163) both pieces are structure middles, V has only surface track there (Left/Right, L40) and the game allows it (both tunnels exist in the save). The domain only counts a crossing when the crosser's piece is Ground (`kinds[c][i] != Ground` is skipped), so "L30 listed under L32 but never passes under a middle" for each pair, in both directions (L30xL32 x2, L30xL47 x2, L31xL32 x2, L31xL47 x2). Plain words: one tunnel passes beneath the middle of another tunnel; the domain has no such crossing.

### D. End run reaching past V's head onto a crossing (2 missing crossing + 2 head)
L30's last end-run piece (greatest + 2) is (95,163) and L31's (95,164), one tile beyond V's head (94,163)/(94,164): V has ground track there and L57's tunnel (T2 (95,158)->(95,172)) passes under it. The domain marks that ground piece Head: "head: an end piece shares its tile with another piece" (L30/L57 at game (95,163), L31/L57 at (95,164)), and L57's crosser L30/L31 "never passes under a middle" (the crosser's piece there is not Ground). Same end-run model as A, at a tile where V has one end piece and a crossing under the next tile.

Totals: A 13, B 32, C 8, D 4 = 57.

## Every break

- DOMAIN GAP span shifted(15,24) game(93,161) L48: all nine tiles identical
- DOMAIN GAP span shifted(26,27) game(104,164) L80: all nine tiles identical
- DOMAIN GAP span shifted(14,29) game(92,166) L45: all nine tiles identical
- DOMAIN GAP span shifted(15,30) game(93,167) L46: all nine tiles identical
- DOMAIN GAP span shifted(16,32) game(94,169) L11: all nine tiles identical
- DOMAIN GAP span shifted(16,33) game(94,170) L12: all nine tiles identical
- DOMAIN GAP span shifted(9,34) game(87,171) L11: all nine tiles identical
- DOMAIN GAP span shifted(10,35) game(88,172) L12: all nine tiles identical
- DOMAIN GAP span shifted(18,36) game(96,173) L57: all nine tiles identical
- DOMAIN GAP span shifted(19,36) game(97,173) L59: all nine tiles identical
- DOMAIN GAP span shifted(20,36) game(98,173) L60: all nine tiles identical
- DOMAIN GAP span shifted(16,38) game(94,175) L48: all nine tiles identical
- DOMAIN GAP span shifted(15,43) game(93,180) L49: all nine tiles identical
- ARTEFACT node shifted(27,42) game(105,179) L69: (104,179) ours {X} V {X,Lower}
- ARTEFACT node shifted(27,42) game(105,179) L70: (104,179) ours {X} V {X,Lower}
- ARTEFACT node shifted(51,36) game(129,173) L96: (128,173) ours {} V {X}; (129,173) ours {} V {X}; (130,173) ours {} V {X}; (129,174) ours {} V {X}; (130,174) ours {} V {X}
- ARTEFACT node shifted(51,36) game(129,173) L94: (128,173) ours {} V {X}; (129,173) ours {} V {X}; (130,173) ours {} V {X}; (129,174) ours {} V {X}; (130,174) ours {} V {X}
- DOMAIN GAP connection shifted(4,25) game(82,162) L4,L5: all nine tiles identical
- DOMAIN GAP connection shifted(12,34) game(90,171) L11,L37: all nine tiles identical
- DOMAIN GAP connection shifted(13,35) game(91,172) L12,L37: all nine tiles identical
- DOMAIN GAP connection shifted(15,28) game(93,165) L40,L48: all nine tiles identical
- DOMAIN GAP connection shifted(15,34) game(93,171) L11,L48: all nine tiles identical
- DOMAIN GAP connection shifted(15,35) game(93,172) L12,L48: all nine tiles identical
- DOMAIN GAP connection shifted(16,43) game(94,180) L48,L49: all nine tiles identical
- DOMAIN GAP connection shifted(16,45) game(94,182) L35,L48: all nine tiles identical
- DOMAIN GAP connection shifted(17,29) game(95,166) L40,L45: all nine tiles identical
- DOMAIN GAP connection shifted(18,41) game(96,178) L49,L57: all nine tiles identical
- DOMAIN GAP connection shifted(18,43) game(96,180) L57,L61: all nine tiles identical
- DOMAIN GAP connection shifted(19,32) game(97,169) L11,L40: all nine tiles identical
- DOMAIN GAP connection shifted(19,40) game(97,177) L59,L64: all nine tiles identical
- DOMAIN GAP connection shifted(19,42) game(97,179) L61,L59: all nine tiles identical
- DOMAIN GAP connection shifted(20,33) game(98,170) L12,L40: all nine tiles identical
- DOMAIN GAP connection shifted(24,41) game(102,178) L62,L61: all nine tiles identical
- ARTEFACT connection shifted(26,43) game(104,180) L69,L70: (104,179) ours {X} V {X,Lower}
- DOMAIN GAP connection shifted(29,27) game(107,164) L54,L80: all nine tiles identical
- ARTEFACT connection shifted(50,36) game(128,173) L94,L96: (128,173) ours {} V {X}; (129,173) ours {} V {X}; (129,174) ours {} V {X}
- DOMAIN GAP diamond shifted(15,29) game(93,166) L45,L48: all nine tiles identical
- DOMAIN GAP diamond shifted(15,30) game(93,167) L46,L48: all nine tiles identical
- DOMAIN GAP diamond shifted(15,36) game(93,173) L37,L48: all nine tiles identical
- DOMAIN GAP diamond shifted(15,45) game(93,182) L35,L49: all nine tiles identical
- DOMAIN GAP diamond shifted(16,40) game(94,177) L33,L48: all nine tiles identical
- DOMAIN GAP head shifted(17,26) game(95,163) L30,L57: all nine tiles identical
- DOMAIN GAP head shifted(17,27) game(95,164) L31,L57: all nine tiles identical
- DOMAIN GAP diamond shifted(17,30) game(95,167) L40,L46: all nine tiles identical
- DOMAIN GAP diamond shifted(18,38) game(96,175) L37,L57: all nine tiles identical
- DOMAIN GAP diamond shifted(18,40) game(96,177) L33,L57: all nine tiles identical
- DOMAIN GAP diamond shifted(18,43) game(96,180) L57,L62: all nine tiles identical
- DOMAIN GAP diamond shifted(19,38) game(97,175) L37,L59: all nine tiles identical
- DOMAIN GAP diamond shifted(19,43) game(97,180) L62,L59: all nine tiles identical
- DOMAIN GAP diamond shifted(20,38) game(98,175) L37,L60: all nine tiles identical
- DOMAIN GAP diamond shifted(20,39) game(98,176) L60,L64: all nine tiles identical
- DOMAIN GAP diamond shifted(20,41) game(98,178) L61,L60: all nine tiles identical
- ARTEFACT diamond shifted(20,43) game(98,180) L62,L60: (99,181) ours {} V {Lower}
- DOMAIN GAP diamond shifted(22,48) game(100,185) L69,L70: all nine tiles identical
- DOMAIN GAP missing crossing shifted(17,22) game(95,159) L30,L57: all nine tiles identical
- DOMAIN GAP missing crossing shifted(17,22) game(95,159) L31,L57: all nine tiles identical
- DOMAIN GAP missing crossing shifted(13,25) game(91,162) L30,L32: all nine tiles identical
- DOMAIN GAP missing crossing shifted(13,25) game(91,162) L31,L32: all nine tiles identical
- DOMAIN GAP missing crossing shifted(14,26) game(92,163) L30,L47: all nine tiles identical
- DOMAIN GAP missing crossing shifted(14,26) game(92,163) L31,L47: all nine tiles identical
- DOMAIN GAP missing crossing shifted(13,26) game(91,163) L30,L32: all nine tiles identical
- DOMAIN GAP missing crossing shifted(13,26) game(91,163) L30,L47: all nine tiles identical
- DOMAIN GAP missing crossing shifted(13,27) game(91,164) L31,L32: all nine tiles identical
- DOMAIN GAP missing crossing shifted(13,27) game(91,164) L31,L47: all nine tiles identical
