# DESIGN-tracks: the solve on tracks, structure from the topology

Status: draft for the engineer (2026-11-17). Builds on DESIGN-pieces (required pieces, gap sums) and D335 (gaps between channels).

## 1. Why
Three fixes on 2026-11-17 had one root:
- **Jogs:** the model had no gap between neighbouring channels; the channel graph knew it (D335).
- **Clash sides:** the model guesses which side two tracks keep, round by round (LL²: 7-10 rounds, 14-17 s a variant, a skewed layout locked in by early picks); planarity knows the left-to-right order everywhere.
- **Rules by position:** blocks emit fixed fragments and rules pick "the second piece of strand X" (the fan rule broke on 2c; the 2e merge steps probably likewise).
The model is tracks plus patched relations. The topology is a subdivision of the hub into regions between neighbouring lines; the model ignores it.

Engineer, 2026-11-17: "Should we just be keeping the channels, blocks etc concepts higher order and just letting the solve loose on tracks"; and earlier "I'm sure it's something to do with how we're modelling channels, either they're too rigid or they don't change size".

## 2. Layers
1. **Topology and channel graph** decide what is decidable and list the genuine choices:
   - each track's heading sequence (route turns, width changes, gap steps, crossing pieces; DESIGN-pieces §3);
   - the regions and their boundary lines (below);
   - the choices (over/under, ties, flips, candidate steps, region choices the graph leaves open) as variants.
2. **Track solve:** every track a sequence of required pieces (heading fixed, length free, grid rules); linear constraints only. It places; it does not choose shapes or sides.
3. **Blocks** (fray, join, prio row, doubled bridge, crossing stack) contribute only their coop constraints on named pieces (by role, never by index): spacing, coverage, equal bridges, the stagger, tight heads.

## 3. Regions
- A region is the space between two neighbouring lines along a stretch: its left line, its right line, where it opens (a split, a box edge between ports) and where it closes (a merge, a box edge, a crossing where it meets another region over a bridge).
- Non-overlap is "every region is at least the spacing wide" at every piece boundary of its two lines: linear, no sides to guess, no lazy rounds.
- A jog is where a region pinned narrow at the edge must hold a band inside (D335's gap rule falls out).
- A crossing is two regions meeting at a bridge: the under line passes from one region side to the other; over/under stays a variant.
- Widths are measured across the travel direction of the stretch; a stretch where the two lines turn differently is split at the turn.

## 3a. The width profile (2026-11-17, after the merge-step fit)
Engineer: "I can't shake the feeling that the mere concept of stepping merges is a smell. It should just figure it out without a special condition"; "doesn't that stop us building truly diagonal hubs? The diagonals are still a special case".
- Steps are not per-block slots with conditions (fan, gap step, merge step). Before the solve, the topology and the variant give, per stretch, the lanes side by side and their spacing across the stretch's travel; a lane gets a sideways excursion exactly where its position across travel must change between stretches. Fans, gap steps and merge steps are all cases of this one rule.
- Headings are first-class: a stretch has any of 8 headings; width is measured across its own travel; an excursion is a 45-degree turn away from the stretch heading and back (diagonal pieces on an axis stretch, axis pieces on a diagonal stretch). Diagonal hubs are blocked only by the box (axis ports on axis edges, D313), a separate constraint.
- No rule names a joiner, fan, merge or gap. Per channel and piece, the corridor holds an ordered list of tracks: the channel's lanes, each doubled lane's copy (two slots, D333), and the lines entering or leaving the corridor at a node in that piece (a merge's joiner before it, a split's leg after it) on the side the node's leg order gives. Each track gets a slot (lane spacing units) per piece, packed in order from the pinned side (port edge, bundle offsets). A track's slot changing between consecutive pieces is its excursion (sign = direction); nothing else creates one.
- Engineer, on "a lane steps when a joiner lands between it and its neighbour": "that's a special case rule ... Surely a joiner already has an effect on the channel?" Yes: the joiner is a track in the corridor before the merge, and the step falls out of the slots.
- Not decided by slots: a step the room does not force (LL² EO2/WO2/SO2: the bridge copy fits the port gap; the free solver steps for area, 55/56). Zero-length pieces are out (D335); such steps are candidate steps (variant bits, as B's) or absent.
- Evidence so far: the merge rule fitted to one LL² variant matched it exactly and lost area on 182 of 189 others (05e0159b09); wip/ANALYSIS-merge-steps.md tests whether the free solver's choices follow "the lane's position across travel had to change".

## 4. Plan (riskiest first)
1. **Region check on T2 (no solver change):** build the region graph from the channel graph alone; for the best T2 layouts, compare its implied separations with every clash side the lazy loop chose and the gap steps. Pass: every chosen side is implied, none contradicted. Failure cases name what regions miss.
2. Same on T3 and LL² (LL² o49t0f1s0: does the region graph imply the stack the probe found, or a flatter order?).
3. Region widths replace the lazy loop and the hand-written side rules on T2/T3 (`pv check`: no variant worse; times).
4. Blocks rewritten as constraint contributors on role-tagged pieces.
5. LL².

## 5. Open
- How regions meet at a doubled bridge (four tracks wide, D333).
- Whether one-lane merges' stacking order (LL²: D's merges) is fixed by the graph or a variant.

## 6. Faces are corridors (check 2026-11-17, wip/facedump.patch)
Every face of T2 (32), T3 (65) and LL² (38) walked with the lines' travel directions: all but one per hub are two walls from one start vertex to one end vertex (or one wall against the box edge). The exception is the crossing triangle (B, D, F: three crossing lines bounding a cyclic face, `PlanarTriangles`), empty of lanes. So non-overlap can be written per corridor (left wall vs right wall), and a corridor's turning (bends plus vertex angles summing to one turn) is a flow over faces (topology-shape-metrics).

## 7. Ladders: rungs from a rough solve (2026-11-17)
- Corridor phase matching by heading runs fails on solved layouts (wip/PROBE-corridors.md): which parts of two walls face each other depends on where their bends fall.
- Chord drawing vs solved layouts (wip/CHECK-rungorder.md): crossings across a corridor agree on 40 of 51 checkable corridors (every one a diamond: one crossing per wall, a binary order); the 11 disagreements are near-ties. The bends (81 on T2, 155 on T3, 106-138 on LL²) the chord drawing cannot order at all.
- Direction (engineer: "starting with a cheap fuzzy approximate solve and then refining - I think there's legs in that"): (1) a rough solve (today's fixed-shape model) gives each corridor's rung order; (2) a refined solve keeps the walls apart by rungs (at every breakpoint of either wall, a point-to-facing-segment separation, linear), no clash loop, every free stretch may bend; (3) if a breakpoint moves past another, re-read the order and solve again.
- Probe (wip/PROBE-ladders.md, fixed shapes, rungs from the rough solve added up front): area unchanged on T2, T3, LL² o49t0f1s0, o1t3f0s0. LL²: round-0 clashes 18 and 21 -> 0, no later rounds (o1t3 1370 -> 853 ms); a second pass moved two rung points, a third settled. T2/T3: rounds unchanged; every surviving clash is C against F, whose corridor touches the box edge (box faces carried no rungs yet). Rung points where a crossing's segment runs past the wall are dropped (the segment's far end is off the wall).

## 8. Plan (2026-11-17, engineer: "Make sure we record everything we're planning, including the gemini optimisations")
Ordered; each step a short probe first, then built.
1. **Ladders everywhere:** rungs on box-edge corridors too (one wall against the box edge, or two walls with a box edge between them: the box edge is a fixed wall); bridge copies as walls in the corridor they stand in (the copy splits it). Pass: T2/T3/LL² best variants with zero clash rounds, same area.
2. **Clash loop retired:** the refined solve is rows from the rough solve's corridors; a re-read when a rung point leaves its facing segment, at most a few passes. The lazy loop stays only as a test oracle.
3. **Free bends on ladders:** every free stretch between components may bend (engineer: "Surely any free stretch of track between components should be able to bend"; "we just stop thinking about bends as choices"). The solver sets where and how far; the step rules (merge exits, gap steps, candidate step bits) go. The free-bend spike failed only through the clash loop (wip/SPIKE-freebend.md).
4. **Optimisations (from the Gemini exchange):**
   - tight per-piece bounds on optional lengths (big-M from the geometry: the most the piece could move given its corridor and the box), not a global cap;
   - at most one side per bend slot (left + right <= 1);
   - a hard anchor (one port coordinate fixed) instead of only the north-west pull, to remove the hub's sliding symmetry;
   - keep rows local; if the outline linearisation (dense) slows solves, measure the box-plus-track score as an alternative (D336 keeps outline);
   - shared switches per bundle stretch, not per lane;
   - variant symmetry cut (mirror pairs, dead candidate bits).
   Not adopted: a whole-hub tile-grid solve (multi-commodity routing with train-length rules is not network flow; D281's freer joint model did not finish in 18 min); a windowed grid pass stays an option for local refinement.
5. **Shape as a flow (later):** each face's bends plus corner angles sum to one turn; where tracks turn solved jointly over faces (topology-shape-metrics) instead of channel by channel.
6. **Prios that bend** (engineer: "Even Prios can bend actually, we just don't do that right now"): the prio element on a bending stretch, its rows beside the main line and signal spacing legal along the curve; today the prio leg is straight and axis for its coverage.
7. **Organic hubs:** ports free along a boundary or on diagonal arms (lift D313's box), a score that does not favour rectangles (user options: most square, thinnest, cheapest, D329), terrain; the chord drawing (round) as a rough start.
- Fix found on the way: the crossing triangle flip changes the composer's crossing order on F while `PlanarLineSides` is built from the unflipped topology, so face sides near the triangle are wrong on flipped variants (wip/PROBE-corridors.md). Build the face map from the flipped events.
- Lane splits as variants and the variant cut stay queued behind 1-3.
- Probes 2-4 (wip/PROBE-ladders2-4.md, DIAG-ladders.md): bridge copies as walls over-constrain (a deck may pass over ground track): dropped. The face map must use the flipped triangle's events (fixed in the probe). A face can have more than two runs (T2 face 22 walks F B F B when flipped): every boundary run keeps every other run's points on the face's inner side where they face it. Rung spacing from the clash rule: 2 along Right(h_target), 1 when the target is axis and the point's piece is diagonal or perpendicular.
- Probe 4 result (fixed shapes, rows from the rough solve, lazy loop as a safety net): round-0 clashes T2 80 -> 0 (all 28 solved variants), LL² 4740 -> 0 (all 189), T3 91 -> 6; area never worse, LL² better on 16 (up to -183); 0 rows violated on any rough solve. Left: T3 o1t2f1 AF3 vs D3 (one-wall box face, skipped) and, in the area pass, A vs B1" copy clashes on T2/T3 o1 f1 (copies are not walls).
- Probe 5 (wip/PROBE-ladders5.md): stretches of one run that are not neighbours in walk order face each other too (one-run box faces no longer skipped); bridge copies join the walls with deck middles (copy segments with a `DeckRec`) exempt against ground, and copy points off the wall dropped as the base's. Every round-0 clash, first pass and area pass, is gone on T2, T3 and LL² (201 tags); area never worse; 0 rows violated on rough solves.

## 9. Ladder build (2026-11-18): three steps, test-first
Probe 5's rule set, built properly. The lazy loop stays as the safety net until step (c) of §8.
- **L1 Face map from the variant (planar_regions).** `PlanarLineSides(topology, channels)` reads each line's crossing order from `channels` (the variant's flipped channels; today the argument is unused) and builds the face map from it. New public `PlanarFace{face, box, runs}`, `PlanarWallRun{forward, stretches (line, stretch)}`: maximal runs of one dart direction in walk order, ring edges ignored, the last run joined to the first when they match. `PlanarLineSides::Faces()` returns the faces without the outer one. Behaviour change: the composer's lazy-loop sides on flipped variants (measured with `pv check`).
- **L2 Rungs (planar_components).** `RungSpacing` (pure, in planar_model), wall segments per face run (stretch to segment range by the crossing records, off-wall points), bridge copies joined by the side test with deck middles exempt, groups per run (one line each), `LadderRungs(M, faces, solution)` -> rows; tested on the rough solutions of T2/T3: every row holds on the solution it came from.
- **L3 Two-pass solve (planar_components).** `SolveMappedVariant`: rough solve, rungs, rebuild + solve with the rungs (re-read up to 3 passes while a rung point leaves its span), the lazy loop kept. Default on, `PLANAR_NOLADDER=1` turns it off (toggle). `PlanarComponentLayout` gains the winning variant's first-round clash count and rung count; outer tests: T2, T3, LL² mapped give 0 first-round clashes.

## 10. More uses of the rough solve (2026-11-18, engineer: "Yes write them down!")
The ladder build costs a second solve; these turn the rough solve into speed and better layouts. Probe 1 and 2 first, after L2.
1. **Prune variants by bound.** Solve every variant cheaply (LP relaxation, or the rough solve without the area pass). The relaxation's area is a lower bound: drop a variant whose bound is at or above the best layout found (exact branch-and-bound over variants). Ranking by rough area and fully solving only the top few is faster, but a heuristic.
2. **Warm start.** Hand HiGHS the rough layout as the starting solution of the ladder solve; it nearly satisfies every rung, so worse branches are cut early.
3. **Search near the rough layout.** Bound each piece's length to a window around its rough value (Gemini's tight big-M, §8.4); a piece at its window's edge widens the window and re-solves, so nothing is lost.
4. **Better layouts by re-reading.** Repeat rough -> rungs -> re-solve while the area falls (the source of LL²'s 16 better variants): a cheap local search. Freeing one window of the hub at a time and re-solving it goes further (where a windowed grid solve could fit).
5. **Choices decided rough, then fixed.** Over/under, tie choices and lane splits become in-model choices in the rough solve and fixed for the refining one, so most variants disappear.
6. **Interactive port moves.** In the port-selection UI (no drag of a junction footprint; the player places and moves ports), the last layout's rungs are the rough start for the next solve: the re-solve after a port move is incremental. Engineer: "I love the idea of dragging ports around in the UI". Organic hubs take a free-form drawing (the chord drawing) as their rough start the same way.

## 11. Clashes deferred to a grid layer (2026-11-18, engineer: "Can we defer the clashes until a point where we have a grid?")
Layers, as terrain is deferred: topology -> shape and rough positions (continuous, linear) -> grid (tiles). The chip-routing split: global routing checks capacity only (each corridor wide enough for its lanes at the spacing: linear, from the topology and the width profile of D327/D333, no clash pairs), detailed routing puts tracks on tiles, where "one track per tile" is a plain linear rule on tile variables. The grid solve runs in windows around the sites where the rough layout clashes (everything outside fixed from the rough, so train-length and curve rules have fixed boundaries), not on the whole hub. Ladders are the halfway house: capacity rows with exact geometry. Risk: a window with no room sends a capacity raise back up (a coarse loop, rare if capacity is right). Probe after the bends probe: a windowed grid legaliser on the no-clash rough (LL² ~25 clashes per variant, wip/PROBE-noclash.md): do local windows clear them, at what size and time? D334 (grid owned by the geometry layer's pieces) still holds: the pieces are already tile-exact; the grid layer adds occupancy, not positions.

## 12. Shape layer (D340, 2026-11-18)
Spikes: wip/SPIKE-shapeflow{,2,3}.md, PROBE-flowturns.md. Steps (each checked against today's layouts):
- S0 recon: every place the composer decides a heading (DESIGN-pieces' four sources), mapped to stretch end headings.
- S1 shape input: the composer takes per-stretch end headings (a `PlanarShape`) and builds from them; today's rules produce the default `PlanarShape`; feeding it back reproduces today's layouts bit for bit (pv identical).
- S2 shape program: the spike-3 MIP as a module, crossing rules on every line crossing (not only channel crossings), output a `PlanarShape`; tested: today's shape is feasible, objective <= today's.
- S3 scored shapes: per variant the best few shapes plus today's, each solved by the length solve; pick by area. Measure on T2/T3/LL².
- S0 done (wip/RECON-shape-seam.md): every core/bundle heading is decided in `BuildPlanarPaths`; the shape is exactly `PlanarPaths::paths` (per channel: piece headings + the piece each crossing sits on). Hard-coded hit list (to clear in S2/S3 or bring to the engineer as coop conventions): fork-leg jog rule (planar_paths.cpp:272), perpendicular-under-prio (copied from the hand T3 D path, :249), detour tries 2/3 and pass cap 6, join `lo` 4/6, prio fray spread `Turn(h,-1)`, copy sides `j==0 ? -1 : +1`, `PlanarInnerMergeExitStep`, the 2-lane-only and axis-only refusals (planar_components.cpp:1250/1256).
- Judgement calls the shape program makes (engineer asked, 2026-11-18): (1) the cost: least turning, a choice, measured against area in S3a; (2) ties: among equally turning shapes the MIP returns whichever it finds (S3 to score them or break ties by a stated rule); (3) no U-turn: a channel starting at a node turns by at most 135° net (row added in S2). Derived: faces and crossing order (topology), port headings (arms), over axis (OpenTTD bridges), under not parallel (geometry), 45° steps. Inherited: component internal angles (coop shapes, incl. a node's out channel starting on its in channel's end heading), variant bits.
- S4 (engineer 2026-11-18: "Why do we still have so many variants? Wasn't part of this work to reduce options?"): absorb variant bits into the shape layer: ties (which channel runs straight at a site: a heading choice) first; candidate steps (optional jogs, on the hit list) become a derived rule or go; over/under as a shape-program choice with a bridge-length proxy cost (or keep the best 2-3 per crossing); triangle flips stay variants (they change the face map). Target: LL² from hundreds of variants to a handful.
- S4 recon (wip/RECON-ties-steps.md) CORRECTS the S4 line above: ties are NOT headings. A tie site has two other channels of equal turn; the bit picks which leg runs straight, which changes component internals only (fray/join lane order, prio spread, core offsets, gap steps, merge exit joiner side, ladder side), never a channel's headings; both choices have equal net turn, so the shape program cannot rank them. Bits: ties T2 2, T3 2, LL² 3; candidate steps T2 1, T3 1, LL² 0 (a step = one required diagonal before a core over crossings; D335 makes it yes/no; area choice, no derived rule found; program-shape bests use s0). So S4 as framed does not cut LL²'s variants (those are over bits x ties x flips). Next: measure offline from pv baselines whether best area depends on tie/step choice per (over, flip) group, before any rule.
- S4 offline check (pv default mode at d632dc357c, scratch S2bf before files): ties are real area choices: within each (over, flip, step) group the tie choice changes area every time (spread up to 83 T2, 129 T3, 144 LL²; no group indifferent). Steps split both ways (T2 s1 better 10 / s0 better 4; T3 7/7; LL² sample s1 better 32/32). So neither can be dropped or fixed by a rule without losing area; S4a/S4b NOT built (orchestrator, 2026-11-18). Cutting variants needs ranking/pruning (DESIGN-tracks §10.1: LP bound or rough-score ranking) or moving these choices into a cheap solve. Also found: the rebuilt wip/ll2-tags.txt (first 201 DRY lines, 152 unique) misses LL²'s best variant o49t0f1s0 (sample best 1895.25 vs 1654.25): rebuild it from the full built set before relying on LL² baselines.
- Rig verdict (engineer, 2026-11-18, old vs new T2/T3/LL² side by side): "I think almost universally the new shapes are better. T3 is slightly subjective - it's more compact but longer. I think we can do something there with the prios though" (T3 new 137x38 vs old 128x44; links to §8.6 prios that bend).

## 13. Prio freedom (D342, 2026-11-18; wip/RESEARCH-prio-freedom.md, wip/RECON-prio-straight.md)
Today: the ML lane under a prio is one axis segment `AddSeg(S, h, 2P+2)` ending at the merge, and the prio row a fixed stub strand 2P long beside it (planar_components.cpp ~243/369/980, rows ~290/406/1015): the ML is pinned straight, which is what makes T3 long. The mapped signal pass (junction_signals.cpp:136-175) and CheckPrio's shape clauses (junction_rules.cpp:1331-1403) assume one axis; G9b (CheckPrioReach) and clause 4/G9c are piece-based already.
Steps, each test-first:
- P1 checker: CheckPrio clause 1 per join piece (the join's own ML piece axis, not the merge's); drop the "single row beside the ML" and "row holds only axis or join" clauses (D342: the row may leave the ML between joins; G9b judges); keep streamlined joins (D75), no prio track on ML tiles except joins, and the merge-tile join without stub (D79/D81). Red test: a bent-coverage prio layout (coverage through a 90° bend, joins on axis pieces of both legs, row offset outside) that passes G9b but fails today's clause 1.
- P2 signal pass: prio signals walk the ML pieces (row = per-piece side tile), not `ml(s) = M - s*dir`.
- P3 composer: the ML under a prio becomes an ordinary stretch (shape layer may bend it); the prio row a parallel strand following it at the side offset where it has joins, free between joins; ladders keep it clear. Measure T3 length.

### 13.1 Prio row model (orchestrator default, 2026-11-18 evening; D227 toggle candidate; wip/PROBE-prio-bridge2.md, wip/PROBE-joiner-over.md)
Finding: both freed forms (main line over with coverage through its doubled bridge, probe A2: T2 684.25, T3 1534.25 composer areas; joiner over with the lanes S-bending under, probe B: T3 none solve, T2 967.25) fail at the same place: the prio row is a fixed straight 2P strand tied 2P before the merge, so it cannot follow a lane that bends or carries a bridge, and without it (A2) the lanes pack so tight that no row tile is free ("the prio element does not fit").
Default rule (one rule for both forms, D342 already allows it): the row is an offset copy of the lane's own coverage pieces, two half-units to the side away from the joiner (the side today's row takes), heading for heading, built as a strand the composer ties piece by piece to the lane (as bundle lanes are offset-parallel), clashing with every other track. It is gapped exactly where another track must cross it: a row piece beside a piece of the lane that crosses or is crossed (a deck of the lane, or a joiner landing through the row) is absent; joins sit on the plain axis pieces either side (the signal pass searches them, D345; G9b judges).
- Main line over (A): the lane's coverage pieces are the ones the join builds for it (pre-split run, the doubled stretch, tail), their lengths free, their sum at least 2P + 2 from the last real switch (the copy switches allowed, D347); the row piece beside the doubled stretch is absent (the joiner passes through there).
- Joiner over (B): the row follows the lane through its S-bend; under the joiner's deck middle it needs no gap (ground under a deck middle is free), only beside the deck heads.
Next: probe A3 (form A with this row) on T2/T3 through the signal pass and checker; then B3 if A3 lands. Engineer to confirm by eye in the rig, not before.
- Reservation revisited (engineer, 2026-11-18: "Is the whole concept of reservation something we need to revisit?"): the reservation exists because one coop component is split over two stages (the composer keeps room, the signal pass draws row and joins by search). Landing step after A3: the row becomes real track in the composer, owned by its lane like a doubled copy: the gapped offset copy with a join (two 45-degree pieces) at each end of each row piece, rendered; the signal pass then places signals (and any extra joins), G9b judging. That retires the prio reservation.

## 14. Scaling without the exponential (2026-11-18, engineer: "I think we need to fundamentally solve the exponential nature of this somehow with our architecture. Perhaps if there's some way we can use an existing LL^2 solve as seed for the LL^3 - it's essentially a modular design with repeating elements")
Multilevel: the discrete choices (over/under, ties, crossing order, steps, shape) live at the channel level and barely depend on lane count; width changes lengths, not combinatorics. So solve the hub narrow (few lanes: small, fast, all variants), lift the best few narrow decisions to the wide hub, and run only length solves at full width (with local re-decisions where the wide hub has crossings the narrow one lacks).
Evidence (pv baselines at 174497786e, default mode): T2 and T3 are the same hub at two and three lanes with the same variant bits: Spearman rank correlation of variant areas 0.945 over 28 variants; T2's best (o1t3f1s0) is T3's best; T2's top 5 are T3's ranks 1, 5, 4, 14, 3.
Open: the lift mapping when the narrow and wide topologies differ in crossings (LL3a lane-paired has 11 open crossings vs LL²'s 6: not a widened LL²); a topology family = one skeleton plus widths (fits D343/D344 topologies and the builder UI).
- Engineer agreed (2026-11-18): "You're right - if the layouts aren't modular then this isn't going to help." So multilevel needs topology families (one skeleton + lane widths); next: LL³ as a true widening of LL² (PSG 227 pattern at three lanes), check its variant bits match LL²'s, then test lifting LL²'s top 5 (after the two-lane limit is lifted). Bridge-length threshold (D346): from game rules or corpus, a function of signal distance (research running: wip/RESEARCH-bridge-length.md).
- Later (engineer, 2026-11-18: "lets put that on the list for later"): main line first as an ordering opinion: higher turn cost for main lines in the shape program; main-line crossing bits first in the variant local search; bits ranked by measured impact. Test against area and main-line straightness once the local search reports.
- Local search on LL² (2026-11-18, scratch driver ls/ls.py, binary at 0958786f36, default mode): one-bit-flip descent over all 8192 variants (o6 t3 f1 s3), 4 starts (default, default flipped, 2 random): ALL four converge to o17t0f1s0 at 1347.25 (better than the best known 1374.25 o49t0f1s0), after 2-4 moves each; 101 distinct variants solved of 8192 (1.2%), 17 of them unbuildable/failing. Global optimum unproven (no full sweep), but every start agrees. Evidence for searching variants instead of enumerating.
- Engineer (2026-11-18): "We can absolutely do some offline work later to help steer us right": offline full sweeps for ground truth, measured bit interactions (which bits only help together), tuning starts (LP ranking, narrow-hub winner, defaults) and moves (two-bit, perturbation).

## 15. Slot layer build (2026-11-18; engineer: "Lets crack on with designing the slot layer build"; spike wip/SPIKE-slots.md, review wip/REVIEW-sideways-modelling.md)
The slot layer owns sideways position. Components keep their fixed coop cross-sections but no longer make sideways steps, and nothing else creates an S-bend. This is §3a, built.

### 15.1 The layer (pure, before the composer)
- **Input:** the topology, the variant's channels (flipped), the resolved sites, `PlanarPaths::over`, `line_share`.
- **Intervals:** each channel is cut at its crossings in travel order into `pre`, `x<k>`, `mid<k>` and `post` intervals. Every channel has both `pre` and `post`.
- **Tracks:** each interval holds an ordered list of tracks, left to right:
  - the channel's lines;
  - a **copy** beside each full-share line in an `x<k>` where the channel is over, on the D312 side;
  - a **row** (D349) beside each lane of a prio join's straight channel in `post`, on the side away from the joiners.
- **Slots:** an integer slot per track per interval, one slot being one lane spacing (2 half-tiles). The rules:
  - consecutive tracks satisfy `s(right) - s(left) >= 1`; between lanes in the last interval of a prio fray's channel the gap is `>= 2` (the fray gap);
  - a copy is exactly 1 from its line;
  - port pins: `s = o(channel) + rank` in `pre` for an in port and in `post` for an out port;
  - a straight line across a site is linked by a step.
- **Objective:** minimise Σ|step| over consecutive intervals of a line and over site links, plus Σ width per interval (weights 1; §15.4 replaces the width weight). With the order fixed, this is a min-cost tension problem; HiGHS solves it in milliseconds.
- **Output** (`PlanarSlots`): per channel and interval, its tracks with their slots, each channel normalised so that its least slot is 0; per site link, its step. There are no yes/no variables.

### 15.2 How the composer uses it
- **Offsets:** in each interval, the lanes' sideways offsets are fixed relative to the channel's first lane: `Dot(Right(h), p_j) - Dot(Right(h), p_0) = 2 (s_j - s_0)` at the interval's pieces. These are difference rows. This replaces `JoinEntry` offsets, the `Flex` offsets and its candidate-step jog, the fray fan widths, and the gap steps.
- **S-bends:** a slot change of a line between two intervals (or across a site link) of Δ ≠ 0 is a fixed S-bend: an axis piece of free length, a 45-degree piece of length exactly 2|Δ| toward the sign, then an axis piece. Its position along travel is free, and the ladders keep it clear.
- **Components** (fray, join, doubled bridge, prio row) take pin slots from the layer: a doubled bridge's lane and copy sit on the slots of `x<k>`, and a join's lanes on those of the straight channel's `post`. They make no step of their own.
- **Diagonal intervals:** an S-bend required on a diagonal interval is refused in v1 ("a slot step on a diagonal piece is not composed") and counted. A later step makes it an axis-piece excursion (§3a).

### 15.3 Steps (riskiest first; each test-first, with pv on T2, T3 and LL² and the checker on the bests)
1. **S1, the layer:** `src/junction/planar/planar_slots.{h,cpp}`, `BuildPlanarSlots`. Tests: on T2 o1t3f1s0 and LL² o17t0f1s0 the gap changes equal the spike's (wip/SPIKE-slots.md run 4); one-track intervals work; the solve takes under 20 ms. No composer change.
2. **S2, probe (time-boxed, throwaway):** cores and bundles take their offsets and S-bends from slots, with candidate steps off. Measure T2, T3 and LL² areas, failures, checker results, solve time and variant count. This is the risky part: does the length solve keep the areas when slots, not search, place the steps?
3. **S3, land S2** per component: cores and bundles, then fray fans, then join exits, then the doubled bridge's lane step. Gap steps and candidate steps retire. Each lands with its pv diff.
4. **S4, D349:** prio rows become real strands on their slots; the stagger is removed (each lane's coverage may run over its own doubled bridge, D347); the reserved `prio_rows` retire. Measure T3.
5. **S5:** diagonal intervals (axis-piece excursions); n-lane bundles (the two-lane refusal goes, as the layer is n-lane by construction), then LL³ (ROADMAP items 5 and 6).
6. **S6:** the width weight per interval comes from the rough solve's corridor lengths (re-read loop), so area steps the room does not force are traded against corridor length (review Q2).

### 15.4 Open, decided at S2's measurement
- Whether unit weights lose area against today's searched steps. If so, S6 moves before S4.
- Whether the doubled bridge keeps today's parallelogram (the lane ends on the copy's line) or the copy returns to the lane. That is a coop convention question for the engineer if S2 shows that it matters.
