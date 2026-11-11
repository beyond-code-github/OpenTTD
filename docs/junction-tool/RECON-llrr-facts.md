# RECON-llrr-facts: what is ruled, built and measured for multi-lane (LL_RR) T junctions (LR0, 2026-11-11, head 9fdb82b721)
Read only. Companion to RECON-llrr.md (the earlier rules survey from RULES-hubs/RULES-coop, H/R/Q ids are from there; this file adds DECISIONS, code and measurements). Code refs are src/ file:line at this head.

## 1. Rulings (DECISIONS.md rows) and what each implies for an LL_RR T
- D38 (l.44): "LL_RR is two lanes directly adjacent either side of a one tile gap ... a guideline rather than a mandatory distance". Gap is a default; spacing may vary along a line (terrain, widening before a junction).
- D60 (l.66): ML and branch each take their own notation, "e.g. an LL_RR mainline with an L_R branch". T spec = two JunctionLineSpec-like strings.
- D71 (l.77): multi-line needs "waiting bays become a must"; (a) every connection leaving the ML holds a whole train TL between split after-signal and merge signal; (b) no deadlock. Implies a bay on every leg of every line-choice fan.
- D72 (l.78): multi-lane is a follow-on; 1b prints "not supported yet" (the MultiLane error, see 2.1).
- D80/D174 (l.86): every ML bridge doubled (split, two parallel equal-length bridges, plain merge). Silent on per-lane doubling for LL_RR (RECON-llrr Q5, CARDS card 3 open).
- D83/D88 (l.89, 94): whole-train bays, boundary signal at least B = 2*TL units back from the previous one on a legal non-switch piece; at least one bay.
- D100/D105 (l.107, 112): balance = equal pathfinder cost of parallel routes (symmetry); the shape is free. For LL_RR: the legs of every fan need equal cost.
- D113 (l.120): a signal-legal piece with a signal between a split and the next bridge head (and after a far head before a merge). Conflicts with D176 on multi-line (open, card 4).
- D126 (l.133): lane counts "per direction and per arm" (independent), up to LLLL_RRRR and beyond. In scope: (1) line choice (which lane a joiner enters, which lanes a splitter leaves from), (2) balancing with equal YAPF cost, (3) shifters, (4) bays wherever trains choose. Evaluator "per lane and per block"; solver variables "indexed by lane". Target T = 6 movements.
- D135 (l.142): line painter takes the same notation and ML gap as the junction tool; unequal counts (LLL_RR) allowed; inner track of a turn meets curve_spacing; S-bends only when every track meets the S-bend window.
- D142 (l.150): same-direction tracks adjacent (gap 0); underscores set the only gap, between the two groups. Implies lane k+1 of a group is exactly one track offset from lane k, so a bundle is a rigid shape.
- D145 (l.155): constants measured for B's two lanes only (RFL/LFR = one x column, split/head offsets, stagger 0 or +1, the stagger is a discrete choice).
- D161 (l.174): line router pruning key omits path_used, so a multi-track line can lose a cheaper legal route (LL_RR 259 found vs 251 legal). Approximate by ruling.
- D162 (l.175): multi-track heuristic is the weak floor (rail floor x track 0 Manhattan + turns). Multi-track routes get a weaker bound.
- D176 (l.190): on LL_RR "each train has a choice of line on the ML"; each leg from a line-choice split to its merge signal is whole bays (exact multiples of TL in tile units) or the leftover carries no signal. "Enforced in the map layer as whole-bay edge lengths in tile units when LL_RR comes; the checker gains the rule then." Also L_R tolerance stays: "for L_R the waiting bays don't matter as much".
- D184 (l.200): the flat structure is built on first use from today's flat generator via ExtractJunctionChains; "Building it from the rules directly waits for LL_RR". So the LL_RR template cannot come from the flat generator (it refuses multi-lane, 2.1); it must be built from the rules.
- D182/DESIGN-nodes 7: "Parallel lanes are one bundle edge" (settled in rev 3).

## 2. Code that already handles more than one lane per direction
### 2.1 Notation
- ParseJunctionNotation (junction_generator.cpp:44-65): returns gap only for exactly one L and one R; LL_RR or interleaved forms (L_L__R_R, l.59-63) give JunctionNotationError::MultiLane. Callers: junction_gui.cpp:737 (shows STR_JUNCTION_ERROR_MULTILANE), console_cmds.cpp:374. The generator enum has GenerateError::MultiLane (junction_generator.h:57).
- ParseLineNotation (junction_line.cpp:354-370): accepts any L-count, 0..3 underscores, any R-count -> JunctionLineSpec{tracks = L+R, gap, left_tracks} (junction_line.h:24-28). Rejects interleaved gaps (L_L__R_R is Invalid). Tests: src/tests/junction_line.cpp:194 (LL_RR 4,1,2; LLL__RR 5,2,3).
- Two parsers coexist; the T tool uses the single-lane one. JunctionLineSpec carries one gap, only between the groups (D142).

### 2.2 Line painter / router (multi-track)
- Geometry: RasterizeJunctionLine / ExtendJunctionLine / ExtendJunctionLineBridge (junction_line.cpp:303, 208, 248); per-track offsets via TrackOffset(spec, i, heading) (l.106); turn strings per track (l.146). Bridges of a multi-track line start from axis pieces only (D149; test junction_line_router.cpp:648). Tests: LL_RR keeps groups (:220), bridges a river (:571), benchmark loop over L_R and LL_RR (:448, :710).
- Router: LineRouter (junction_line_router.cpp:93). With tracks != 1: no sharing a tile with existing track for a split or merge (ExistingHolds l.241), weaker heuristic (l.384), no "two-step" moves (l.910), and movement rules refused: "movement rules need one track and no prefix" (l.653). So the router cannot yet route a fan leg's split/merge for a bundle, and the O4c1 JunctionMovementRules (phase, private_units, plain, after_head) are single-track only.
- Self-avoidance of tracks of one line is the path_used key (l.98, 144); D161 leaves it out of the pruning key.

### 2.3 B's two lanes in today's T (the only multi-lane in a T today)
- BuildParallelogramLanes (junction_generator.cpp:258): lane A straight through its bridge then S-bends to the b row; lane B S-bends at the lane split, bridges on the b row, shifts back. Called l.630 and l.1485. Lane merge/split are explicit nodes: lane split "where lane B first leaves the W row's axis piece" (l.771); lane merge = a merge whose joiner has the same movement (IsLaneMerge junction_signal_pass.cpp:270, used l.467-545; junction_chain.cpp:101 "A lane merge has the same ends and no prio").
- Lanes are two paths of ONE movement (WIn->WOut etc.), not two ports.

### 2.4 Checker and generator assumptions of one lane per direction
- FindJunctionPorts (junction_rules.cpp:154-176): an edge must have exactly two port tiles (tiles.size() != 2 returns nothing), each with only the axis track. An LL_RR end has four tiles per edge: gives no ports at all. Roles are single: enum JunctionPortRole {EIn, EOut, WIn, WOut, NIn, SOut} (junction_rules.h:27).
- JunctionTMovements (junction_rules.cpp:1685-1692): fixed six movements by role. CheckConnectivity (l.352-395) already allows count > 1 paths per movement if they are equal-length "doubled lanes" (message "doubled lanes differ in length", l.395), i.e. lanes of one movement, not distinct lanes.
- CheckWaitingRoom (l.843-): for each path From EIn/WIn To SOut: takes the first split and the first merge after it, needs 2*TL units to the last signal (l.880). Whole-train bays loop (l.882-): every non-ML path, every merge. With a line-choice fan the first split/merge pairing and "is_ml" test (EIn->EOut, WIn->WOut only) would pick the wrong events and treat lane-choice splits as exits; no whole-bay-multiple rule exists (D176 says the checker gains it).
- CheckDeadlock (l.1014): G8, no signal block holds both a split and a merge; lane-count independent in form (sets of split/merge tiles), but a fan's blocks are untested.
- CheckPrio (l.1054-): ML merges only (is_ml = the two ML movements, l.1090-96); the joiner is "the other path entering the switch tile"; a merge whose joiner has the same movement is a lane merge, no prio (l.1121). With LL_RR, merges where N->A joins lane 1 or lane 2 are the joiners of two different ML paths; "exactly one prio component" (l.1154) per merge assumes one joiner. CheckPrioReach (l.1535) likewise.
- Bridges: l.507-522 "Every bridge on a mainline path needs another path of the same movement that avoids it" (ML bridge doubled). For LL_RR the other path of the movement might be the other lane's route, which would pass the rule without doubling: card 3 unanswered.
- Consumers that take the FIRST path per movement (so ignore a second lane): junction_generator.cpp:1815 (PrioTimingsOf), junction_throughput.cpp:53 (EvaluateJunctionThroughput).
- ExtractJunctionChains (junction_chain.cpp, N0b) and the N3 template (DESIGN-N3 1) assume on one path "all free links travel the same way in x (assert; true for the T)" and unique points per (tile, trackdir); lanes share pieces only at splits/merges.

## 3. The movement set of an LL_RR T
Ruled or recorded:
- Six directed movements: EIn->EOut, EIn->SOut, WIn->WOut, WIn->SOut, NIn->WOut, NIn->EOut (JunctionTMovements; D126(3), R10, H35). With lanes, each ML direction has 2 lane ports in and out.
- Draft (DESIGN-O4.md:65-72, not a decision): L_R branch first; each lane of A splits toward S, the two legs merge before the S port (card 2a); N->A fans to both lanes with one prio join per lane (H13 1-to-2; R12, R13); legs are whole bays (D176); equal cost per fan (D100); B bridges over the crossings, doubling per card 3.
- ML-to-ML movements: lane k in -> lane k out, straight (no balancing, SLH has no ML turns H35). Which lane a train uses is then a property of where it entered; D126(1) "line choice" applies only at splits/joins of the T.
- D71 "trains choose between tracks": at a fan split a train picks a leg by YAPF cost, so equal cost (D100) is what makes a choice; whole-bay legs (D176) keep a full leg from blocking the other leg's choice.
Open (questions for the engineer, evidence in brackets):
1. Does every ML lane split toward S (two exits, card 2a), or only the outer lane (2b), or outer plus shifter (2c)? [R11 "exits need no balancing" vs D126(1) and D176; CARDS 2 unanswered; STATUS l.15 only says prepare design with cards]
2. N->A onto two ML lanes: is the N train's choice a split on the branch (1-to-2) with a prio join per lane, or two separate branch lanes? [H13, R12/R13; DESIGN-O4:68 is a draft only]
3. LL_RR branch (multi-to-multi): which style (H24), and is it in step one? [DESIGN-O4:71 excludes it; Q3]
4. Doubled ML bridge in LL_RR: two bridges (the lanes are the doubling) or four? [D80, D174, card 3, checker l.507 passes either]
5. D113 vs D176 on a fan leg: does the signal between split and bridge head hold when the leftover must be whole bays? [D176 "to settle with LL_RR", card 4 recommends (b)]
6. Shifters in step one? [D126(3) vs card 5 recommends no]
7. Balance along the ML after exits: SML-style imbalance (inner busier, RULES coop gap 8) is "to be taken into consideration"; do we balance lanes at all in an SLH T? [H9, H19, D129 modular balance]
8. Minimum ML gap for an LL_RR T and how groups adjacent (gap 0 within group) interact with crossings: a bundle's S-bend moves two adjacent tracks, so is the bundle's shift one row for both with equal cost per lane? [D142, D105; Q9; FABLE 1.3 "lane can only shift if its sibling shifts"]
9. Per-lane signals: are the two lanes of a direction signalled independently in their own blocks (D126 "per lane and per block") and does the SD grid align between the lanes at the fan? [D83: SD grid anchored at the merge signal; no ruling for parallel lanes]
10. Equal cost for an asymmetric branch (branch arrives from one side, so its two legs differ) [Q11, D100 says equal cost is the balance rule, not equal length]
11. Prio length for an LL_RR join: SLH-style longer vs BBH-style short; D29 range [R20, Q6]
12. Notation: do we keep two parsers (ParseJunctionNotation single-lane; ParseLineNotation general)? And is interleaved L_L__R_R (accepted by neither route to a layout) supported? [junction_generator.cpp:59-63, junction_line.cpp:354, D142 says gap only between groups]

## 4. Measurements on file for multi-lane
- LL_RR line router, D161 case: informed route cost 251, uninformed 259 (start (40,20) X_NE, goal (34,18) td 3); more cases failing: 255/263, 270/278, 259/267, 263/271, always informed < uninformed (FINDINGS-O4x.md:6-10). Making the pruning key exact multiplies expansions 20-30x and hits the search limit on most benchmark cases (D161).
- D162: LL_RR multi-start returned 80 informed vs 74 uninformed with per-track Manhattan, cured by the weaker floor.
- Router time: single-track turn 1.2 ms, hills 4 ms, crossing 7.8 ms; multi-track 20-40 ms (weak D162 floor) (STATUS.md:99, :108 for the single-track figures). Budget for a hover is under 100 ms for the whole junction (hover-target; DESIGN-nodes 8).
- Single-lane doubled-bridge balance (the only lane-balance measurement): ML-only run with 6 ML trains, 12.6% of ML trains stopped in the junction because every train took lane A (STATUS.md:336); D100 symmetric Y fixed it (equal YAPF cost).
- Lane geometry constants (RECON-lane-geometry.md, D145) for B's two lanes: RFL/LFR one x column; S1 winners height 15 and 13, parallelogram style 1; style 0 has no candidate on either test spec. None for LL_RR.
- No FINDINGS/PERF/BENCH file has a number for a generated multi-lane T: PERF2.md and BENCH-B0.md contain only L_R (grep for LL_RR, multi, lane: no hits). Tests measured: `[nodes]` 0.45 s Release (TESTS.md:21), wide gate 17 s (TESTS.md:24), both L_R only.

## 5. Constraints the node model must add for bundles
Classification: DIFF = difference constraint on integer positions (x_i - x_j >= k); DISC = discrete structural choice; NEITHER = a post-solve check, a structure property, or not expressible as above.
| Rule | Source | Class |
|---|---|---|
| A bundle of k adjacent tracks moves as one: one S-bend shifts all k rows; one lane-split/lane-merge node per bundle | FABLE 1.3 "bundle edge", D142, DESIGN-nodes 7 | DISC (shift side and where) with rigid shape; positions DIFF |
| Bundle curve spacing: every track meets curve_spacing / S-bend window; inner track of a turn is the binding one | D135c, router l.281-307 | DIFF (min straight length uses the inner track; pairwise curve gaps per track path as 4a) |
| Curve gaps per path (4a) for each lane's path through split/merge nodes | DESIGN-nodes 4a, DESIGN-N3 2.2 | DIFF, one set per lane path (paths multiply: a fan gives 2 x 2 paths per exit) |
| Whole-bay legs: leg from a line-choice split to its merge signal is an exact multiple of TL units, or the leftover carries no signal | D176, D83, D88 | NEITHER as stated (an equality mod B over a length): DIFF only for fixed k (x_m - x_s = k*B units, or <= with signal-free leftover), k is DISC; D88 piece legality (not a switch, not on a bridge) is a per-tile fact (domain) |
| One signal-legal piece between split and bridge head carrying a signal | D113 | DIFF (>= 1 piece) but conflicts with whole-bay rule until card 4 |
| Waiting room: 2*TL units from first split to last signal before the merge for every ML->S leg | D71, CheckWaitingRoom l.880 | DIFF |
| Prio coverage 2p / 2p+1 before an ML merge (per join, so per lane) | DESIGN-N3 2.3, D108, D148 | DIFF, one per lane join (two for the 1-to-2 fan) |
| No ML split inside a prio coverage; no merge followed by split on a path (CheckOrder, adjacent splits) | D108, junction_rules.cpp:429 | DIFF (order constraints) |
| Equal cost / length of the two legs of a fan (line sync, equal YAPF cost) | D100, D105, R33, checker l.395 (doubled lanes differ in length) | DIFF (equality as two difference constraints) if cost = length + curve count; cost with different curve counts is NEITHER (a derived check, YAPF curve penalty 100 per 45 degree) |
| Lanes of a bundle on adjacent rows, different bundles never overlap; crossings of two bundles at their windows | D142, D172 span window, DESIGN-N3 2.4 | DIFF for crossings (strictly between heads, per lane pair); non-overlap is NEITHER (collision check, N3 4) |
| ML bridge doubling count (2 or 4 bridges) | D80, D174, card 3 | DISC once ruled (not yet) |
| Shifters (optional) | D126(3), card 5 | DISC, out of step one |
| Per-lane signal slots and SD grid anchored at merge signal | D83, D117 | NEITHER (post-solve signal pass, N3 2 "not in the template") |
| Ports: 2 lane ports per ML direction per end, frame-fixed | D178, D142, FindJunctionPorts l.154 (refuses 4 tiles) | DISC (frame), anchors fix a class (N3 2.5) |
