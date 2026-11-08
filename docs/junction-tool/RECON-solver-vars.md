# RECON: solver variables and constraints (S2)
Sources: gen = src/junction_generator.cpp (checked-out tree), rules = src/junction_rules.cpp, audit = docs/junction-tool/AUDIT-assumptions.md, DEC = DECISIONS.md.
Conventions: working frame x runs 0..width-1; E ML (row 0) travels +x; W ML (row w_row=g+1) travels -x. "Upstream" on W = larger x. g = ml_gap, TL = spec.tl, cover = TL*prio_tl tiles (gen:309; rules count 2*TL*prio_tl units, axis tile = 2 units, rules:86-89, so equal).
NOTE: gen:237 still has STUB = 3 although D115 says 1 (tree not yet updated, or other worktree). Treat STUB as a constant to verify.

## 1. Skeleton variables
### 1a. Global / frame (fixed, not swept)
- xI = hub_x = 30 + TL*(prio_tl-1) (gen:189-197, 212); xO = xI + branch_gap + 1 (gen:214). Derived constants.
- w_row = g+1, b_row = g+2 (gen:218-219) = lane B / outer row. Constants. yb = H-1 (gen:216).
- STUB = 3 (gen:237), ys = yb - STUB. Constant. goal_s = (xO, ys); start_n = (xI, ys).
- width, reach_east: frame, grow with prio_tl.
### 1b. Swept inputs (GenerateJunction gen:689-760, BuildJunctionLibrary gen:762; all integer or discrete)
- H = height in [g+5, g+20] (int, row count; yb=H-1; yb >= g+4). Early stop at first clean H + 2 (gen:~725 stop_after).
- crossing_cost in {0, bridge(3)} (discrete; router cost only, not a rule).
- e_slack in 0..3 (int, x offset): N->E join start = max(xO, x_s + cover + 1 + e_slack) (gen:309-310). x_s = E->S split x (router output!).
- w_slack in 0..3 (int): N->W join max x = min(xI-2, x_lm - cover - w_slack) (gen:335).
- bridge_margin in 1..3 (int): h_w = min(zone) - margin, h_e = max(zone) + margin (gen:323-324). D103.
- lane_style in {0,1} (discrete). 0 = symmetric Y, gap row; 1 = parallelogram.
- plane (terrain only, discrete, <=3 candidates).
### 1c. Inner implicit searches in BuildJunctionCandidateWithGap (gen:209-585)
- gap in {0,1} (BuildJunctionCandidate gen:587-618): tight tried first, loose only if tight not rule-clean. Enters as q >= gap (lane B plain tiles after S-bend) and, style 1, an extra F before lane A's RFL (gen:~352). D110. Discrete/int, "first clean wins".
- sb_back in 1..6, style 1 only (gen:612), tightest first, first clean wins. Style 0 uses fixed 6, unused (gen:607). x_sb = nw_max_x - sb_back (gen:364); needs x_sb >= 2.
- zone = set of W-row x used by E->S and N->E in pass-1 routing with W crossable (gen:253-262, 320). Router output, not a free variable: it determines h_w, h_e, n = h_e-h_w-1 (must satisfy bridge_span_ok(n), h_w>=0, h_e+1<=width-1; gen:325-327).
- Lane program parameters found by enumeration (rasterise-and-match), i.e. solved implicitly:
  - style 0, lane B moves = F^p "RFL" F^q B<n> F^r "LFR" F^s, from (width-1, w_row) heading -x (gen:370-418). r fixed 1 (gen:398, audit S38), q from gap upward (first match), p chosen so near head x = h_e, row b_row; s so the end is x=0. Lane A = mirror (L<->R), heads on row g at h_e/h_w (gen:420-441), same units.
  - style 1, lane A = F^k B<n> [F] "RFL" (gen:349-362): near head (h_e, w_row), far head (h_w, w_row), ends on b_row; merge_x = x of its last piece. Lane B = F^p RFL F^q B<n> F^t LFR F^s: near head (h_e,b_row), t chosen so the piece before LFR is at x_sb+1, s so it ends at (0, w_row) (gen:380-415).
  - Both lanes' heads are at the SAME x (h_e, h_w): code requires it (gen:361-362, 383, 433-434). D118's "shifted lane one tile closer" is NOT a separate variable; the stagger arises only from where each lane's S-bend sits. (Possible doc/code mismatch, needs engineer check.)
### 1d. Derived positions (formulas read from the lane programs; verify with a pinning test, they come from piece indices not literals)
Using pieces numbered along travel (x decreases by 1 per piece):
- x_ls (lane split, first non-axis piece of lane B, gen:486-492) = h_e + 4 + q (head index p+4+q, RFL at index p..p+2, one plain tile at p+3 when q=0, gen:372-376). [derived]
- SL split start piece sl_x = x_ls + 3 (gen:495, 296-298): the W row plain piece the W->S route leaves from; split tile x_ls+2, one plain signal tile x_ls+1, lane split x_ls. [derived from D110(3b), verify]
- far head x = h_w; style 0 lane merge x_lm = first non-axis piece after far head = h_w - (r+3) with r=1 -> h_w-4 [derived] (gen:444-466, A and B must agree).
- style 1 merge: A's RFL end tile (gen:363).
- x_sb shift-back start (style 1) = nw_max_x - sb_back; N->W goal window on b_row = [x_sb+1, nw_max_x] (gen:495).
- N->W window style 0: [2, nw_max_x] on w_row, nw_max_x = min(xI-2, x_lm - cover - w_slack).
- SL window: W->S start x = sl_x exactly (single start, gen:296), must be >= xO+2 (gen:298). First pass (no lanes): any W row plain piece in [xO+2, width-3] or b_row [xO+2, lane_b_max_x] (gen:293-294).
- E->S: leaves E row at x in [2, xI-2], must reach goal_s; column fence xI rows 0..g+3, ys-1, ys-2 (gen:271-276). N->E goal window on row 0: [max(xO, x_s+cover+1+e_slack), width-3] (gen:310).
- Crop: plain E/W row tiles kept to content +-2 (gen:498-517) = post-hoc bounding-box variable, not a decision.
Integer along-row x variables for the W section: x_sl (SL split), x_ls, h_e(near head), h_w(far head), x_lm (merge), x_nw (N->W join), x_sb (style 1); plus q, r, p, s, t, k program lengths (all functions of those x). Discrete: lane_style, gap (q>=gap), r (fixed 1), clusters (not implemented, see 5).

## 2. Routed parts (after skeleton)
Order always E->S, W->S, N->E, N->W (D90(1); route_movements gen:268-319), all through RouteFencedAndAdd with JunctionRouteCache.
- Pass 1 (no lanes, W row crossable at crossing_cost, gen:320): determines zone -> h_w,h_e,n. Everything after depends on this pass's router choices (so skeleton is not independent of the router; a solver must either take zone as input per (H,crossing_cost,e_slack) or constrain it).
- Pass 2 (lanes fixed, crossable none, gen:495): requests:
  - E->S: starts = E row plain pieces x in [2,xI-2]; goal goal_s; fence = column_fence. Output gives x_s.
  - W->S: starts = {W row piece at sl_x}; goals = S stub pieces at xO (Y_SE) rows ys+1..yb-1; fence = under_spans + rows 1..g+1? (branch_fence: rows y=1..w_row-1 all x = ML gap rows, gen:278-279) + under_spans (tiles under bridges on rows w_row-1 (style0)/w_row (style1) and b_row, x in (h_w,h_e)).
  - N->E: start start_n; goals = row-0 plain pieces x >= max(xO, x_s+cover+1+e_slack) (D108); crossable none; no fence except base.
  - N->W: starts = N stub pieces at xI rows ys+1..yb-1; goals = plain pieces of nw_row in [nw_min_x, nw_max_x]; branch_fence.
  - Common: bounds min (1,0), max (xO+reach_east, yb); curve_spacing = DeriveCurveSpacing; s_bend_window = 2TL; bridge limits; turn_cost.
- Skeleton -> router feeds: h_e/h_w/n (via under_spans fence), x_ls (sl_x), x_lm/x_sb/e_slack/w_slack/H (windows, bounds), g/H (fences, yb).
- Router decides: branch geometry between starts and goals, the E->S split x_s, the N->E join x, the branch bridges, crossing of N column.
- Signals: PlaceJunctionSignals(layout, spec) (gen:568) after normalise, then footprint check; signals are not skeleton variables but rules 3 and 7 depend on them.

## 3. Constraints (rule -> relation). Kind: D = difference (a-b>=k), L = other linear, P = procedural (router/checker). Class from AUDIT.
Symbols: x_sl SL split tile, x_ls lane split, x_h_e near head, x_h_w far head, x_lm lane merge, x_nw N->W join, x_sb shift-back, q, r plain tile counts.
### 3a. Lane-section geometry (W side)
1. D110(3)/CheckOrder adjacent splits (rules:429-445): x_sl - x_ls >= 2 (one plain signal tile between SL split and lane split; three-way split x_sl=x_ls is a separate legal form not offered by generator). D. COOP (engineer, audit "missing rules" list).
2. D113 / CheckSignals bridge clause (rules:758-773, "split in a bridge's block"): x_ls - x_h_e >= 2+? i.e. at least one signal-legal plain piece between lane-split S-bend and near head: q+1 >= 1 -> x_ls - x_h_e >= 4 (with the RFL occupying 3 pieces + >=1 plain) [encodes as x_ls - x_h_e = 4 + q, q >= 0]. D (equality-with-slack). COOP (D113, engineer). Same on exit: x_h_w - x_lm >= 4 (r >= 1) [style 0]. D. COOP (D113, D82; r fixed 1 by audit S38, "OURS" since G5 approach rule relaxed in D110; r=0 untested).
3. Gap choice: q >= gap in {0,1}: loose gap only if tight fails G-rules (BuildJunctionCandidate). Not a rule; a search policy.
4. G5 CheckBridges (rules:470-541): head distinct, aligned, no track/signal on head, approach track outside heads (curve may join directly, D110), span n=x_h_e-x_h_w-1 accepted by bridge_span_ok (L; n bounded above by 10 in router max_bridge_middle), no tile shared by two bridges (A and B on different rows: guaranteed by rows), every ML bridge doubled (lane A/B both exist). GAME for heads/approach/span/overlap (audit C10-C14), COOP for doubled (C13, D80). Span: x_h_e - x_h_w - 1 <= nmax (D, nmax from bridge_span_ok; generator also needs 1<=n).
5. Crossing coverage: x_h_w <= x_zone_min - bridge_margin, x_h_e >= x_zone_max + bridge_margin (D, zone from router; D103). margin is itself a sweep, so solver form: x_h_w <= zmin - m, x_h_e >= zmax + m with m >= 1 (generator tries 1..3; the maths minimum is the checker/G5 head-clears-crossing-set). COOP-proxy for "+margin" (D90 zone +-2, +1 hedge); GAME for "head clears the crossing set".
6. Symmetry D100 / G1 doubled equal length (rules:366-393): lane A, lane B same piece units (RasterUnits equality gen:441), same heads x. Equal-length is automatic in style 0 by mirror; in style 1 not automatic: lane A and B lengths from x_h_e to merge must be equal (L, equality). COOP (D100, R33, audit C5).
7. D105/D118 parallelogram: lane A: heads on w_row, S-bend after far head; lane B S-bend before near head; B and A merge on b_row at A's last piece; x_lm_A (A's RFL end) is a B-row axis piece: B has an axis piece at that x on b_row (gen:468-474). D105 structure; D for positions: lane B row b_row between x_h_w and x_sb+1.
8. Shift-back (style 1, D110(4)): x_sb = x_nw_max - sb_back, sb_back >= 1, x_sb >= 2, N->W goals in [x_sb+1, x_nw_max]; so x_nw - x_sb >= 1, x_nw_max - x_sb = sb_back (D). x_sb must be downstream of A's merge by enough for A's merge tile plus G3: x_lm - (x_sb+1) >= ? derived from curve spacing (below). COOP? (D105 "shift back when safe", "outside the prio coverage").
9. G3 CheckCurveSpeed (rules:544-624): every bend pair of S-bends obey curve_spacing; "after a tight pair, no turn until u>=2TL" (router R3, audit: OURS stricter than game). Relations: gaps between successive curves along each path >= CL where CL = DeriveCurveSpacing in units. L/D on x positions of consecutive bends: x_sl-related bends, x_ls bend, merge LFR, x_sb bend: difference constraints with k = CL/2 tiles. GAME (train_cmd.cpp:321 port) + COOP (reference_speed). Procedural in general (train of 2TL parts), but linear for straight-spaced bends. Skipped when curve_limit false.
### 3b. Prio and window rules (couple W and N->W)
10. D85/D107 N->W join after lane merge: x_lm - x_nw >= cover + w_slack (D; gen:335). The coverage part is COOP (D85, D108); w_slack >=0 is pure hedge -> "margins from maths": drop, G9b/G9c decide.
11. D108 / G9c (rules:1253-1270): the coverage window of each ML merge's prio (2*TL*prio_tl units upstream of the merge) contains no split/lane split/lane merge of the ML lane: for the N->W merge at x_nw: x_lm - x_nw >= cover (coverage is on single track downstream of lane merge, in -x direction i.e. lane merge must be >= cover tiles upstream). x_ls and x_sl are further upstream, so they do not bind unless style 1 where the shift-back x_sb lies downstream of the join (no coverage conflict as x_sb < x_nw). Also: E side x_nmerge - x_s >= cover (+sd+1, D108 text; code uses +1, gen:310). D. COOP (D108, audit C33).
12. G9b CheckPrioReach (rules:1483-1557, SettleSignals): joiner Entry signal settles red for a one-tile train anywhere in coverage; depends on signals around the joiner and block structure, e.g. G9b failure drove "D117 one signal may do several jobs". P (checker/simulator). GAME (signal.cpp ExploreSegment) + COOP (D107).
13. G9 CheckPrio (rules:1002-1280): prio shape (one straight row beside ML, D111 queued to generalise), prio touches no used track, one join per covered block, coverage = 2*TL*prio_tl. Mostly P. COOP (D70, R25, D75) with OURS (straight row C28, first join on merge tile C30, coverage C31).
14. G7 CheckWaitingRoom (rules:791-860, audit C21): from ML split to its last signal before the merge >= 2TL units (E->S, W->S). For W->S, the span from x_sl to the last signal before the S stub merge. L over router output, no skeleton variable except x_sl. COOP (D71a).
15. G4 order: no split after a merge on a path (rules:429-455): for a W ML path: x_ls > x_lm (always), x_sl > x_lm; for E->S etc. COOP (R8, D24). D with k=1; automatic from ordering.
16. G8 deadlock block (rules:962): no block holds both a split and a merge. P (needs signals; ComputeBlocks). COOP intent, exact test OURS.
17. G10 footprint (rules:1580-1610): W/E/bottom edges free of signals, splits, heads, unused track; min offsets 0. D58 COOP. Gives: shortest-W-extent = min over (x_nw, x_sb, 2); crop is post-hoc; on_boundary = (x == 0 | x == size.x-1 | y == last) so N->W join >= 1 tile from edge, x_sb >= 2 (gen) vs checker 1.
### 3c. Router windows (generator-only, OURS, audit S11-S42) that would become variable bounds
- STUB=3 (S11), x_s in [2,xI-2] (S19), x_sl >= xO+2 (S21), lane_b_max_x (S23, first pass only), N->E start >= xO (S25), N->W <= xI-2 (S28), N->W window floor x_sb+1 (S42 corrected), fences S15,S17,S18. These are encoded as bounds; audit proposes removing most and letting the checker judge.
### 3d. Counts (W-side, explicit list above): ~15 difference constraints (1,2x2,4a,5x2,8,10,11,15 and the G3 chain), ~4 other linear (span<=nmax, equal-length 6, style-1 lane A/B row structure 7, nw/sb bounds), ~6 procedural (G9, G9b, G8, G7, G3 in general, G5 doubled check).

## 4. Objective (D114, gen:627-646, gen:~742-760)
- Key 1: count of distinct occupied tiles = track tiles with any track + all bridge tiles from head to head (incl. spans). Key 2: cost = rail_piece*track bits + signal*count + sum bridge(middle tiles) (+ terrain cost). Key 3: layout size.x*size.y (tie-break). Lower is better.
- From skeleton alone: bridge tiles (2 bridges * (n+2)), lane section tiles (2 lanes, row-extent x_ls-x_lm), plain W/E rows x extent after crop (min over x_nw/x_sb/2 to max over...), bridge cost (2*bridge(n)), rail bits per S-bend. Calculable except branch routes.
- Needs routing: branch tiles, branch bridges, E->S/N->E pieces, N stub; signals count (after signal pass: bridge entry, spacing sd, prio signals).
- Not scored: bridge occupancy/headway (D103(2)), bridge speed (D94) -- L1 evaluator (junction_throughput) is the coming scorer (PLAN S1c).

## 5. Gaps (implicit decisions a solver needs as explicit variables)
1. gap in {0,1}: policy "tight first, loose if not clean". Make q an integer >= 0 and let constraints 2 decide. Also loose adds one F to BOTH lanes (A's pre-RFL F in style 1; mirror in style 0), so it is a coupled shared q rather than independent.
2. sb_back: tightest first; make x_sb an integer var with lower bound from G3/G9b.
3. r fixed at 1 (audit S38): should be {0,1} with D110 relaxed rules; and symmetric exit analog of q (exit "q" = r).
4. Lane-split to SL-split distance: fixed at exactly 2 (sl_x = x_ls+3 single start). The three-way split (distance 0) is an unused alternative; distance > 2 is not allowed by the code (single start) even though rules permit it (it would waste width, so objective excludes it).
5. Stagger of lane heads (D118): code forces equal x for both lanes' heads; a staggered variable (h_e^A - h_e^B in {0,+-1}) is not modelled. Style choice 0/1 absorbs it.
6. Clusters (D109): NOT implemented in the generator (grep cluster: none). Single zone, single doubled section; prio_tl >= 2 spans may fail. Min-gap is a missing variable.
7. zone (crossing set) from pass-1 router: depends on H, crossing_cost, e_slack... and fixes h_e,h_w. A solver needs either zone as a derived function of E->S/N->E router results or leave h_w, h_e free and constrain via fences/crossable. Today the dependence is through "first pass, then lane geometry, then re-route".
8. crossing_cost in {0, bridge(3)}: not a rule, a router cost; should be a variable only if the router is kept in the loop.
9. Margins: bridge_margin 1..3, e_slack 0..3, w_slack 0..3 are hedges (violates margins-from-maths feedback). Replace by constraints 5, 10/11 and let G9b/G9c reject. e_slack counterpart: N->E join - x_s >= cover (+1 from audit S24).
10. Planes (terrain): discrete choice; per-plane re-solve.
11. Height H: derived (rows needed), solver minimises; fences (column ys-2, g+3) are part of it.
12. First clean wins by sweep order (H ascending with stop at +2, then params order, then score): final selection uses the full score; but within BuildJunctionCandidate first-clean (tight, loose, sb_back) is order-dependent, not best-scoring.
13. Signal pass is a function of skeleton; solver cannot compute signal count without it.

## 6. Proposed W-side S2 target: SL split through shift-back (style 1 primary, style 0 as variant)
Decision variables (integers along row, travel -x):
- x_sl  SL (W->S) split tile
- x_ls  lane split
- x_he  near bridge head x (same x both lanes, D100/gen:361-433)
- x_hw  far bridge head x
- x_lm  lane merge (style 0: LFR half-piece; style 1: A's RFL end)
- x_nw  N->W join (max-window end, take the join x as a variable in [x_sb+1, x_nwmax])
- x_sb  shift-back start (style 1 only; omitted in style 0)
- q, r  plain tiles after the lane-split S-bend and before the merge S-bend (q>=0, r>=0, q+1>=1 and r+1>=1 as in constraint 2). q,r are definitions: q = x_ls - x_he - 4, r = x_hw - x_lm - 3.
- style in {0,1}.
Inputs (not variables): zmin, zmax (crossing set from pass 1), nmax (bridge_span_ok), cover = TL*prio_tl, CL (curve spacing in tiles), x_cap = xI-2 (N->W cap), sd.
Constraints:
 C1 x_sl - x_ls >= 2                         (D110 adjacent splits)
 C2 x_ls - x_he >= 4                         (D113 entry, RFL=3 + 1 signal tile; q = slack)
 C3 x_he - x_hw >= 1 and x_he - x_hw - 1 <= nmax  (span)
 C4 x_hw <= zmin - 1, x_he >= zmax + 1       (head clears crossing set; replaces bridge_margin sweep)
 C5 x_hw - x_lm >= 4                         (D113 exit; r>=1 until r=0 proven)
 C6 x_lm - x_nw >= cover                     (D108/G9c; w_slack dropped)
 C7 x_nw <= x_cap  (xI-2)                    (D90 window)
 C8 style 1: x_nw - x_sb >= 1 (N->W goal window [x_sb+1, x_nw]); x_sb >= 2; x_sb >= 1 per G10; x_nw >= x_sb+1
 C9 G3 curve spacing along the W path: successive bends separated by >= ceil(CL/2) tiles: x_sl-start bend, lane split bend (x_ls..x_ls-2), merge bend (x_lm..x_lm-2), shift-back bend (x_sb..x_sb-2): e.g. x_lm - 2 - x_sb >= CL/2 etc. (D; exact k from DeriveCurveSpacing, pinned by a G3 test)
 C10 equal lengths (L): style 0 automatic by mirror; style 1 enforce |lane A len - lane B len| = 0 via q/r tie: A has gap F count, B has q, r: require the unit counts equal (the existing RasterUnits check).
 C11 G7: x_sl-based waiting room (>= 2TL units from SL split to last signal before S stub): input from branch router, treat as lower bound on branch not W section.
Procedural checks (counterexample loop): G9b prio reach at x_nw, G8, G3 full, G9 shape, signal pass legality, G4 order. 
Objective on W side: minimise tiles = bridge tiles + lane rows + plain W row extent: minimise (x_sl_max... = x_ls + 3 start) .. min(x_nw, x_sb, 2), i.e. W-row extent W = x_start - x_end, and 2*(n+2)+... Then cost. Compare with generator on test spec.
Seed switches: C1,C2,C5 (COOP D110/D113), C6 (COOP D108), C4 margin (hedge dropped; GAME part kept).
Coupling to routed parts: x_sl feeds the W->S start; x_he/x_hw feed under_spans; x_nw feeds N->W goals; x_s feeds N->E windows (E side outside S2).
