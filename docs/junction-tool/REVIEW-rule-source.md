# REVIEW-rule-source: review of DESIGN-G3-vectors.md §G3.6 "One rule source (D240)" (at daffae682b)

Read-only review against the code at daffae682b. Abbreviations: jp = src/junction/placement/junction_placement.cpp, jj = src/junction/placement/junction_joint.cpp, jh = src/junction/tool/junction_hover.cpp. "INFERRED" marks a conclusion from reading the code, not from a run.

## Findings, by severity

**F1 [wrong] The loop can stop on a layout that is not legal: tie rows allow overlaps, and kept keys freeze the gap.**
- Evidence: a tie is an overlap at the solution: occupants on one flat tile (jp:1334-1343) or overlapping groups (jp:1358). Its gap is `apart = flat_left(b) - flat_right(a)`, which is <= 0 (jp:1296, 1302). The joint's clash row is one-sided, `>= gap` (jj:247, 263), so a tie row allows the overlap. The key holds the occupants' forms and the tie flag but not the gap (jp:1299-1302), and "a key seen again keeps its first form" (G3.3a b-6). Group bits are ORed (jp:1347-1357), so the same key can need gap 2 (touch) in a later round while gap 1 is kept. Legality is a separate test (jp:1316-1330) and is not part of "detection adds nothing".
- Counterexample (INFERRED): round 0 of the new loop has static rows only, and the cost objective packs the pieces. Every overlap between unrelated pieces is detected as a tie, kept with apart <= 0, and allowed again in round 1. Detection then finds no new key and the loop "converges" with illegal shares.
- Fix: detect a tie only for a pair that `JunctionTracksMayShareTile` allows. An overlap that may not share gets a separation row whose side follows a stated rule (G3.2's sweep order where it is defined, else the occupant walk order, named). A kept gap may only grow. The end test is: every freshly detected row holds at the solution, and legality (jp:1316-1330) holds.

**F2 [wrong] Detection reads an arbitrary optimal vertex, and the seed's x stops being the hover's x.**
- Evidence: the objective's only tie break is EPS on L (jj:352). Runs are priced by |Δ| (jj:319-324), so many x (and y) are equally optimal. Today the seed and the hover both take the least solution plus the D215 push (jp:504-551), and `[topology]` pins "placement on flat facts is the seed (T, LL_RR)" (TESTS.md, T2).
- Consequence: the order of every kept lazy row depends on which vertex HiGHS returns. A model change flips orders. The seed renders a different x from the hover on flat ground, so R5's identity oracle cannot hold.
- Fix: after each MILP solve, keep L, t and the port offset, and fix y by a second solve (L fixed, minimise Σy). Then take x from the hover's own X solve at (L, y): least solution plus push. That system is a difference system and feasible, because the MILP point satisfies it. Detect at that point and output it.

**F3 [missing] Under the hover's least objective the heads do not hug: the order pins are still needed there.**
- Evidence: the hug inequalities (jp:1610-1614) only bound the heads. The equalities in `hug_heads` (jp:1365-1380) are what make the heads hug under a least solution. The joint drops them (jj:220) and lets the span price pull the heads tight (jj:307-315), but the hover keeps least plus push (jp:504-551). For b >= 1 of a series cut, the near head only has `>= previous far head + floor` (jp:1580) and lies in no row change's rigid group. So the least solution leaves it at that floor, not at crosser - 1. Copy 1's heads follow it through the stagger (jp:1578-1579). For b = 0 the D215 push of the lane split happens to restore the hug.
- Counterexample (INFERRED as to which leg has g >= 2): LL_RR x L_R's cut has series bridges ("6 doubled bridges", TESTS.md R5; SB1 middles <= 5). At the hover, its second bridge's near head stands at the floor and the span is longer than the seed's. That can exceed `max_bridge_middle` (jp:2554).
- Fix: the source emits the hug pins as a lazy family: per hug, equalities to the outermost crosser tile at the seed solution, keyed. Only the hover reads them; the joint keeps the inequalities.

**F4 [wrong] A fan's hand is a sign the solution picks, so the fan family is not static once the port offset is a variable.**
- Evidence: `refresh` sets the fan's hand from sign(y_item - y_port) (jp:614). The curve walk reads it the same way (jp:880). `fan_m = |port_row - y_item|` (jp:2234). `JunctionCurveGaps` depends on the hands: same hand min(2TL-1, max(2,CL)), opposite hands 2 (placement CLAUDE.md "Curve gaps"). G3.3a says "the sign fixed by the side", but the side fixes the heading (hin), not the sign. `best_offset` minimises the largest |fan| (jp:2212-2226), so the fans on one side point both ways.
- Consequence: with the offset as a variable, a fan of size 0 or 1 at the seed can flip its hand. That changes its curve pairs and its tile forms, so neither the curve counters nor the clash forms are affine in it.
- Fix: before R2c, fix each fan's sign σ_e as data (from notation order against stack order, or from the seed) and add the static row σ_e (port_row - y_item) >= 0. A flip is then a named structure choice.

**F5 [missing] The fixed path makes validity checks that neither the static rows nor the final detection replace.**
- Evidence:
  - Missing from both: `max_bridge_middle` (jp:2554, doubled only), `max_tunnel_length` (jp:2560), "a crossing edge misses its bridge" (jp:2492, 2495, 2522, 2539), "Z row" (jp:1619), the blocked bridge span (jp:2565-2583), port rows inside the box (jp:2300-2305) and the curve walk (jp:2587).
  - The draft says the final detection is "the same occupancy and legality test as today's". Today's final test is only `add_occupancy(vx, true)` (jp:2389-2392). Clearance is never re-checked on the settled placement.
  - `CheckJunctionRules` is not in graph-pipeline acceptance: its callers are only jss:618 and 629 (signal pruning, jss = src/junction/signals/junction_signal_pass.cpp) and legacy. So src/junction/CLAUDE.md "the search still keeps it in acceptance" looks stale (INFERRED).
- Fix: make the span caps static rows (|x_far - x_near| - 1 <= cap) and the box row static (offset + notation >= 0, once the offset is a variable). Define acceptance after the loop as: detection clean, legality, render, signals, named in Consumers 1.

**F6 [missing] Retiring `JunctionPlacementShared` also retires the near reject, and the facts builder has no named home.**
- Evidence: the struct carries the seed's y and the near-reject data (near_groups, near_runs, near_cons, near_lo; jp:2617-2649), and the hover reads it at jh:85 and 106. P5b measured the reject at HOVERNEAR mean 18.9 -> 9.9 ms, and LL_RR 93.2 -> 35.2 (TESTS.md). The terrain facts are built inside `build()` (jp:1893-2196), which R6 deletes.
- Fix: keep (or rename) the struct as "the seed's evaluated rows": the static and lazy X rows at (L, t, y) plus the near data. Move the facts builder into the evaluation, not into the retired set.

**F7 [wrong] Kept stale rows reach the hover.**
- Evidence: b-6 says "a kept clash whose occupants later stand on different rows is stricter than the game needs" (G3.3a). The G3.6 hover reads "the static and kept lazy rows". Today the hover re-derives its pairs at the seed's choices from the flat order (jp:1877-1886): current pairs only.
- Consequence: fewer offsets plan. A drop in HOVERPLAN or HOVERNEAR is a stop (src/tests/junction/CLAUDE.md).
- Fix: the hover takes the static rows plus a fresh detection at the final seed solution. Lazy rows added at one offset stay local to that offset, so the plan at an offset does not depend on the near search's visiting order.

**F8 [missing] LL_RR x LL_RR: the doubled-row families are leg-shaped, and the kept span code assumes one bridge per band edge.**
- Evidence:
  - The doubled block (jp:1544-1625) runs for any row edge with a doubled bridge (jp:836-838). It starts the lane split on `exit_item` (jp:1556-1557) and runs it from the chain's first tile (jp:1549, 1573).
  - For DI1's in edge e5 (copies = SBend(LaneSplit) from A = Lane to B = Copy, D241), the split must start on A. INFERRED: today it gets size 0 on B.
  - `collect_bridges` takes doubled legs only (jp:1400-1411), so the clearance misses X-axis bridges on in and out edges (DB1b is still to do, STATUS night 3).
  - Several bridges per band edge: refused at jp:275. `bridges_of[e][0]` is used for every Heads element (jp:1392, 2519-2520), and `heads_of` returns the first Heads only (jp:2446-2447). So Consumer 3's "existing span code, which is kept" must change.
- Fix: write the doubled families per role from the node shapes (A/B items), and each Heads element from its own Over token. Add a hand-derived static oracle for e5. Also run R2's static oracle on L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR: their static relations are recorded ("record 0 clashes" means the recording ran, STATUS line 1).

**F9 [missing] Clearance has no defined side at 0, and the two axes disagree today.**
- Evidence: in X, side 0 adds no row (jp:1427); in Y, side 0 demands both sides, a contradiction (jp:2275-2279). The draft's detection returns rows "with the pair and side it shows", and at 0 there is no side. At the hover y is fixed, so a head that enters an X-axis bridge's span on the bridge's row can only be cleared in X, by a disjunction.
- Fix: one rule for side 0 in both axes (a named failure, or the side by graph order). At the hover, a Y clearance becomes the X row "head outside the span, on its side at the seed".

**F10 [missing] The bridge-cut search is a consumer of the rounds that the draft does not name.**
- Evidence: `CutJunctionTrackBridges` calls the rounds path for each cut and keeps the smallest box (src/junction/junction_pipeline.cpp:164-170). The cycling hubs measured 40-210 ms per joint round (STATUS night 10).
- Fix: name the cut search as a consumer: the joint loop per cut, ranked by the D235 cost, not by the box. Measure the seed time on LL_RR x LL_RR (several doubled bridges multiply the cuts).

**F11 [wrong] The migration oracles are weaker than they look.**
- Evidence:
  - R2 compares at the seed's values only: one L cannot tell a fan tag (a = 1, b = -1) (jp:1637) from (0, m - 1). The two-length pattern already exists in the tests (src/tests/junction/placement/junction_joint.cpp:545-549: record again with one m + 1).
  - The recorded curve-gap relation is ceil(need/2), the largest per (v, u) (jp:1834-1868). The counters are one row per pair over the entry variables E (jj:95, 152-172). The comparison must reduce and merge before it can equal.
  - R3: detection at the recording's own flat state with the same walk is equal by construction. At the settled vx the pairs differ. Name the state; R4 is the behavioural test.
- Fix: run R2 at L and L + 1 for each sized bend, state the curve-gap reduction, and state R3's state.

**F12 [wording] Contradictions and names.**
- G3.2 says occupancy and the hug use the sweep's band order, "never the least solution's order". G3.6 takes the order from the joint solution without mentioning G3.2. Reconcile: order from G3.2 where it is defined, lazy only for which pairs share a row.
- G3.3a item 2 calls the crosser piece topological and linear; G3.6 lists the hug as lazy. For a run crosser the hug is static; only a bend crosser needs the lazy part.
- `junction_rules` already names the checker (src/junction/junction_rules.{h,cpp}; root CLAUDE.md). Choose another name, for example `junction_rule_rows`.
- "rows change only by an edit named by an X failure" (placement CLAUDE.md) is about tile rows (Y), not constraint rows.
- The static list omits band gap (jp:1462), port columns and S port spacing (jp:1481, 1488), row items (jp:1554), lane split run (jp:1572) and the caller floors (jp:1790, 1889; the minimality test uses them). The variable list omits the counters' E.
- "Bridge over S-bend" reads the raster of max(m, 2) (jp:1758). A crossed free S-bend has floor 1 (jp:2295; jj:200), but P3 covers L = 2..64. State what holds at L = 1.

## Answers to the questions
1. **Split.** All other families classify as the draft says:
   - Static: stack origin and rows, band gap, S edge, S port row, port columns, S port spacing, row nodes, waiting room, prio element (jp:1538, 1588, 1729), row items, lane split run, series bridge heads, fan size (subject to F4), joiner Corner floor and choice, Corner size, S-bend size, bridge heads, chain run, bridge over S-bend (constant rows, jp:1768-1781), curve gap as counters, box bound.
   - Lazy: occupancy, the hug inequalities, clearance in X and Y.
   - Neither: the facts, the D215 push and the caller floors.
   - Corrections:
     - Misclassified: the fan's hand (F4, lazy); the hug pins (jp:1375-1378), which the hover needs (F3); the tie gap, which is an observed constant (F1).
     - Missing: the span caps and the box row (F5).
2. **Retired or rebuilt?** The rounds, the Y solve at fixed choices and `plan_clearance`'s two flat solves are truly retired. The lazy half is today's occupant walk with its input moved from the flat solve to the joint point. D240 allows that, but the draft should say so. The oracles are unsound or too weak as written (F11) and blind to doubled in and out edges (F8).
3. **Hover reduction.**
   - At fixed L, t and y, the occupancy, hug and clearance X rows and the X links reduce to two points; every Y row becomes a constant check.
   - The counters telescope over consecutive horizontal runs, because each step uses the bend's first point plus offsets (jj:132-139). A vertical run drops its x terms (VX = 0). So a horizontal, vertical, horizontal route leaves four points until the run's cross-axis equality (jp:599) and the bend links are substituted; one heading sign is then needed (today's refusal, jp:943). Exits go horizontal then vertical, joiners vertical then horizontal, and mains are horizontal, so no route in the five hubs has that shape (INFERRED).
   - Y today: `PlanJunctionHoverNear` solves it once per call (jh:85; jp:2306-2310); `PlanJunctionHover` solves it itself (jp:2312-2316).
   - Behaviour change: yes. The hover's rows come from the loop, not from the flat order at the choices. Per-offset repair replaces today's offset failure (jp:2391). See F2, F3 and F7.
4. **The lazy loop.** It terminates (finite keys, rows only added), but termination is not validity (F1). A wrong order and a stale row both appear as an infeasible set. Report, for each clash in the set, whether its row fact held at the last feasible solution. Today the fixed path checks legality only at the end (jp:2389-2392) plus the post-solve checks in F5, never clearance.
5. **LL_RR x LL_RR.** It needs doubled in and out families per role, clearance over them, each Heads element tied to its own Over token, span code for several bridges per edge, and the cut search on the joint (F8, F10).
6. **Simpler.**
   - Convert `build()` in place instead of porting to a new module. `SystemTag` already carries each static row's (bend, a, b) (jp:316-324, 1637-1684). Then:
     1. make every bend size an input, with no `refresh` reads of vy (jp:614, 618, 623, 625);
     2. move `add_occupancy`, `hug_heads`, `add_clearance_x` and `plan_clearance` into one detect function;
     3. the record becomes the rule list.
   - R2a-c then become one identity step under `jt identity`, with no second implementation to keep in step.
   - Keep `JunctionPlacementShared` as the hover's carrier (F6). P2 built it for exactly this.

## Suggested tests
1. "the loop ends legal": after the loop on all five hubs, legality (jp:1316-1330) holds and a fresh detection adds no row and tightens no gap. It should be red on ties.
2. Keep "placement on flat facts is the seed (T, LL_RR)" green through R4 (needs F2).
3. "every hover row has two X points" over all hubs, counters included after substitution; a horizontal, vertical, horizontal route is refused by name.
4. "fan signs are data": each fan's sign at the joint optimum equals its static σ.
5. R2 at L and L + 1 for each sized bend, on all five hubs where the family is recorded.
6. "a hover plan does not depend on visiting order": the plan at offset o alone equals the plan reached at o by the near search.
7. A static oracle for LL_RR x LL_RR e5, hand-derived: lane split on A's row, heads on A and B, merge onto B.
8. "span cap is a row": a forced long doubled bridge names `max_bridge_middle` in the infeasible set.

## Verdict
The direction holds and matches D240: one source, static rows affine in the lengths, lazy rows detected and kept, and the hover evaluating the same rows at fixed lengths. The rounds, the shared Y solve and `plan_clearance` really go. As drafted, though, it is not ready to brief, for four reasons:
- The loop's stopping rule does not imply a legal layout (F1).
- It detects at an arbitrary MILP vertex, which breaks seed-equals-hover (F2).
- The hover's least objective loses the hug without the pins (F3).
- The fan family is classed static although its hand comes from the solution (F4).

The remaining findings are omissions that turn into stops later: validity checks, the near reject, stale rows, the doubled in and out edges of LL_RR x LL_RR, the cut search and weak oracles. Converting `build()` in place reaches the same end state with less risk than porting it to a new module.
