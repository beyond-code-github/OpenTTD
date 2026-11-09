# PLAN S2: skeleton solver for the W-side doubled section

Source: PLAN-sim-driven.md §5 S2, RECON-solver-vars.md (§6 is the starting model), DECISIONS D103, D105, D108–D110, D113–D118, D137–D141.

## 1. Why

The refine tier tries every combination of the sweep parameters:

| Parameter | Values |
|---|---|
| e_slack | 0..3 |
| w_slack | 0..3 |
| bridge_margin | 1..3 |
| crossing_cost | 2 values |
| lane_style | 2 values |

That is 192 candidates per height, and each candidate runs up to 2 gaps × 6 sb_back routings. So it does at least 576 routings per spot, and A* routing takes 70–91% of the time. Under the profiler the three cases took 6, 24 and 43 s.

The probe (TL5, D138 train, 196 km/h, prio 15) shows most of that work is wasted. Most candidates die on a route or on the span check before the checker ever sees them, and only one parameter set builds at all (h=17, e=2, w=1, m=1, style 1).

The sweep also hides the rules. e_slack, w_slack and bridge_margin are hedges (margins-from-maths). The stagger in D118 can't be expressed because both lanes' heads are forced to the same x, and "first clean wins" depends on the order candidates are tried rather than on the score.

## 2. What S2 must deliver (the engineer's acceptance, 2026-11-09)

1. **D118 stagger:** the shifted lane's near head is one tile closer to the lane split, with exactly one signal tile on each leg at both ends and no adjacent signals (needs D117).
2. **No padding beyond the rules:**
   - S24's `+1`, e_slack, w_slack and bridge_margin all go.
   - Each becomes a constraint whose constant is derived from a named rule and pinned by a test.
3. **STUB = 1 (D115).**
4. **Prio coverage is an input bound,** not a swept value. Moving the slider just re-solves.
5. **Hover well under 1 s** at the test specs and on the NUTS rig.
6. **Footprint (D114) the same as or smaller than today's generator** on every test spec, and checker-clean.

## 3. Model

Positions are integer x along the W row, with travel in −x, so upstream means larger x.

### Variables (per candidate)

| Variable | Meaning |
|---|---|
| x_sl | SL (W→S) split tile |
| x_ls | lane split |
| x_heA, x_heB | near head of lane A and of lane B (**separate**, so the D118 stagger can be expressed) |
| x_hwA, x_hwB | far head of lane A and of lane B |
| x_lm | lane merge |
| x_nw | N→W join |
| x_sb | shift-back start (style 1 only) |
| style | 0 or 1: discrete, solved once per value |

### Inputs (fixed per candidate)

| Input | Where it comes from |
|---|---|
| Crossing set per row: zmin_r, zmax_r | pass-1 routing. A diagonal crossing gives different x on the W row and on the outer row, and that difference is what produces the stagger. |
| cover | spec.prio_tiles, from the slider |
| nmax | bridge_span_ok |
| CL | curve spacing in tiles (DeriveCurveSpacing) |
| x_cap | xI − 2 |
| Rule offsets k_* | pinned constants, see S2c |

### Constraints

Every constraint has the form xi − xj ≥ k, so the whole model is a difference-constraint system.

| # | Constraint | Rule |
|---|---|---|
| C1 | x_sl − x_ls ≥ k_split | D110(3b): one signal tile |
| C2 | x_ls − x_heL ≥ k_entry(L, style), for each lane L | D113, D82 and D117: one signal-legal tile per leg. The stagger falls out of this: lane B's S-bend uses up its own tiles, so its tightest head lands one tile from lane A's. |
| C3 | x_heA − x_hwA = x_heB − x_hwB; 1 ≤ x_heL − x_hwL − 1 ≤ nmax | D118 equal length; G5 span |
| C4 | x_hwL ≤ zmin_row(L) − k_head and x_heL ≥ zmax_row(L) + k_head | GAME: a head may not sit on a crossing. Replaces bridge_margin; the probe says k_head = 1. |
| C5 | x_hwL − x_lm ≥ k_exit(L, style) | D113 at the exit; D118 "one signal tile" |
| C6 | x_lm − x_nw ≥ cover + k_w | D108/G9c. Replaces w_slack; the probe's minimum accepted value is 1. |
| C7 | x_nw ≤ x_cap | D90 window |
| C8 | style 1: x_nw − x_sb ≥ k_sb, x_sb ≥ 2 | D105, D110(4), G10 |
| C9 | consecutive bends on the W path at least CL apart | G3 |
| E1 (E side, same pass) | x_ne − x_s ≥ cover + k_e | D108. Replaces `1 + e_slack`. The probe's minimum is 1 + 2 = 3; D108's text says sd + 1, which is 3 when sd = 2. S2c confirms which rule gives it. |

### Solve

Bellman–Ford on about 10 variables, with x_cap and the crossing set as anchors. It gives the tightest positions, which is the D114 objective on the W row's extent. A negative cycle means no layout exists, and the solver reports which constraint made it infeasible (for example, span > nmax).

This is a general solver rather than a closed-form chain because S3 adds the constraints it learns as extra edges.

### Pipeline per candidate (H, crossing_cost, style)

1. Pass-1 routing gives the crossing set. It is shared between the two styles.
2. Solve.
3. Rasterise the lanes from the solved positions.
4. Pass-2 routing.
5. Signal pass.
6. Checker.

If the checker rejects the layout, the ladder that exists today (one extra tile at entry or exit, then sb_back + 1) is replayed as tightened constraints and the solve runs again, at most 4 times. Every time the ladder fires it is logged, and those logs are S3's input.

**Effect:** 4 candidates per height instead of 192, with 2 pass-1 and 4 pass-2 routings, which is about 50× fewer route calls. If hover is still above 1 s, the next lever is to derive H from the solve (the rows needed) instead of sweeping it.

## 4. Steps (one agent each, test first; Sonnet)

| Step | What | Depends on | Can run alongside |
|---|---|---|---|
| **S2a** | STUB = 1 (D115). Fences at ys−1/ys−2 re-derived; test: N/S stubs are one tile, checker clean, every test spec still generates. | – | S2b, S2c, S2d |
| **S2b** | D117 in the signal pass. (a) A rule is satisfied by any signal in its stretch. (b) Remove a signal adjacent to another on the same path when every rule still holds. Test: the rig junction's lane A has no back-to-back signals at entry or exit. | – | S2a, S2c, S2d |
| **S2c** | Pin the offsets. On the test spec, move one position at a time and record the minimum the checker accepts and the rule that rejects one less: k_split, k_entry(L, style), k_exit, k_head, k_w, k_e, k_sb. The results become named constants with a comment citing their rule, plus DECISIONS D142. This also answers D117(c): which rule rejects the tight gap-0 entry. | S2b for k_entry and k_exit (signals) | S2a, S2d |
| **S2d** | The solver seam, in new files `junction_skeleton.{h,cpp}`. A difference-constraint solver, plus a W-side builder from the inputs in §3. Unit tests only, no map. Outer test: the test spec's inputs give the pinned minima. Infeasible when span > nmax, naming C3. | – (uses placeholder constants until S2c) | S2a, S2b, S2c |
| **S2e** | Lane programs from positions: per-lane heads, rasterised from the solved x (replaces the enumeration of q/p/s/t). Test: the built layout has one plain tile per leg at both ends, equal lane lengths, and passes G1/G5/D113. | S2c, S2d | – |
| **S2f** | Wire into GenerateJunction and BuildJunctionLibrary. Remove the e_slack, w_slack, bridge_margin, gap and sb_back loops; keep the ladder of §3. Side-by-side test against the old path on the test specs: footprint ≤ old, checker clean, the D118 shape. | S2a, S2e | – |
| **S2g** | Timing gate and cleanup. Hover time at 3 profile cases and on the NUTS rig; delete the old sweep code. The slider gets live generation below the maximum (LibraryUsable decision, see Q3). | S2f | – |

**Worktrees:** S2a to S2d run in four hand-made worktrees off `feature/junction-tool`, merged in order a, b, d, c. S2a and S2b touch different files; S2d only adds new files.

## 5. Open questions (engineer)

**Q1. Clusters (D109) in S2 or later?**
- Context: today one doubled section covers all the W-row crossings. At prio 15 the probe still builds, but only in style 1.
- Options: (a) in S2, as a discrete choice per cluster split; (b) in S4, with the E side.
- Recommendation: (b). S2 stays one section, and a negative cycle on C3 shows when clusters are needed.
- Reversible: yes.

**Q2. Keep the symmetric style (0)?**
- Context: at strict prio 15 it never builds, because its span exceeds 10. At prio 4 it wins by 4 tiles.
- Recommendation: keep it. It costs one solve, and the slider makes short prios real.
- Reversible: yes.

**Q3. Library below the slider's maximum.**
- Context: once hover is under 1 s, live generation is fast enough to use directly.
- Recommendation: S2g keeps the library for the strict maximum and generates live below it.
- Reversible: yes.

**Q4. Three-way split (D110 3a).**
- Context: the checker accepts it, but it doesn't fit either lane style.
- Recommendation: later, as a C1 variant with k_split = 0.
- Reversible: yes.

## 6. Later: bridge prio (engineer, 2026-11-09: "Would be good to figure out a bridge prio as well to save more space at some point")

Today the prio coverage is a separate run of track beside the ML (D108). The long N→E and N→W parallel runs in image 20 are exactly that coverage.

My reading of a bridge prio: the coverage, or the prio track itself, overlaps a bridge, so the held-train detection reaches over a bridge span instead of needing plain parallel track. Since a bridge is one block, a joiner held at its entry signal sees anything on the span.

Questions to settle before planning:
- Is that the meaning?
- What do G9/G9b need in order to prove it holds?

Fit: S4, beside D111 (bent prios). G9b is the judge (D116).
