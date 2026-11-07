# 1b generator: route search instead of templates (D87)

This replaces PLAN-1b §5.3 (hand-written templates and a slack sweep). The rule checker stays the judge. The generator searches, places signals and prios, and keeps only the candidates the checker passes.

**Why a search.** A template tuned for L_R at TL4 breaks when the gaps, TL or CL change, and the sweep test covers TL 3–6, SD 2–4 and gaps 0–3. A route search adapts to all of them. It also carries straight over to terrain and obstacles at place time (D78) and to the endpoint solver.

## Pipeline (canonical frame, §1 of PLAN-1b)

1. **ML lines:**
   - The E track (row 0) and W track (row `ml_gap+1`) run straight across a working area of 64 tiles.
   - The result is cropped to the footprint at the end, so the ML legs come out flush (D58).
2. **Branch routes,** in planarity order, each by `FindJunctionRoute` (D1) against everything laid so far:

   | # | Movement | Starts | Goals |
   |---|---|---|---|
   | 1 | E→S | any E-track piece | the S-out bottom piece `(xO, H−1)` `Y_SE` |
   | 2 | W→S | any W-ML piece | an axis piece of E→S on column xO (a merge) |
   | 3 | N→E | the N-in bottom piece `(xI, H−1)` `Y_NW` | any E-track piece (a merge) |
   | 4 | N→W | an axis piece of N→E on column xI (a split) | any W-track piece (a merge) |

   - xI is fixed and xO = xI + branch_gap + 1. H (the height) is swept.
   - The W-track tiles are passed as **crossable**: a route may cross them at ground level, at the cost of the ML bridge that will lift that row (step 3).
   - Branch–branch crossings use the router's own bridges.
   - Each finished route is appended along its target line to the leg edge, as the reference design files do.
3. **ML bridges (D80/D85):**
   - Every run of crossed W-track tiles becomes a bridge: heads on the plain tiles just outside the run, and nearby runs merged into one when the heads would collide.
   - The bridge is then doubled: lane B goes on the outer row, out by `RFL`, its own bridge, back by `LFR`, at equal length.
   - Branch routes are then re-run with the lanes and bridges fixed. Crossing is allowed only under the spans, and W→S may start from lane B. If re-routing fails, the candidate is dropped.
4. **Signals and prios** (D2, `PlaceJunctionSignals`): PLAN-1b §6 as amended by D74/D75/D79/D81/D82/D83:
   - required signals first: switch isolation, bridges, queue bays at k·TL back from each merge signal
   - then the SD fill, anchored at merge signals, never on the piece right before a required signal
   - then the prios, on the free side
5. **Check and score:**
   - `CheckJunctionRules`; keep the clean candidates.
   - Score by unit-price cost, then open space inside the bounding box (D76).
6. **Sweep:** H from its minimum upward, both prio sides (D70), and a few crossing-cost weights. The cap is the candidate count and the 200 ms budget (D62). With no clean candidate the result is `NoLayout`, naming the rule group that failed most often.
7. **Road side:** mirror for left-hand (D66).

## Steps

| Step | What | Acceptance |
|---|---|---|
| D1 | `FindJunctionRoute`: A* over pieces with moves F/L/R/B<n>, tile rules, a curve grammar from CL and TL, start and goal sets, crossable tiles, cost from unit prices, deterministic | unit tests on synthetic grids; G3-clean output |
| D2 | `PlaceJunctionSignals`: the signal and prio pass | the doubled reference's routes, with no signals or prio tracks, come out checker-clean |
| D3 | `GenerateJunction`: steps 1–3 and 5–7, plus errors | the outer test goes green and loses `[!mayfail]`; the sweep test |
| E | game side (PLAN-1b §7) | |
| F | manual and rig checks | |
