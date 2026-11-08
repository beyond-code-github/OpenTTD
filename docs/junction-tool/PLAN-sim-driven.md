# Simulation-driven junction design

Status: design note, no code yet (2026-11-08). Engineer's direction: "the constraints should really come from the simulation rather than hard rules. My rules are just proxies for known-throughput issues"; and earlier, "the simulator - and the throughput - is the judge" (D116).

## 1. Why change

Today's generator is generate-and-test. It sweeps parameters (`e_slack`, `w_slack`, `bridge_margin`, `lane_style`, `prio_tl`, `sb_back`, the gap-0/gap-1 fallback), builds every candidate in full (skeleton, routing, signals), runs the checker, and scores what survives. The speed pass counted about **1,920 full candidate builds per generation** on the test spec.

That causes three problems:
- **Speed:** most of the time goes on candidates a rule kills at the end.
- **Manual adjustments:** each piece of engineer feedback (D110, D117, D118) became another sweep parameter or a "tight first" ordering. The generator only learns about the rules through rejection.
- **Proxy rules treated as law:** the co-op rules (signal before a split, bridge margins, split gaps) stand in for throughput effects. Nothing measures the throughput itself, so we can't tell which rules are needed, too strict, or missing.

## 2. Two kinds of rule

The audit (`AUDIT-assumptions.md`) already labels each rule GAME, COOP or OURS.

| Kind | Examples | Role in the new design |
|---|---|---|
| Game mechanics (GAME) | Buildable pieces, legal signal positions, connected routes, bridge limits, no deadlock, G9b prio reach (a port of OpenTTD's signal logic) | Hard constraints. Never traded off. |
| Throughput proxies (COOP, most OURS) | D24/D113 signal before a split or bridge, D110 split gap, D103 bridge margin, D108 no split in prio coverage, D117, D118 | Seed constraints that speed up search. Each is kept only if the evaluator shows it matters. |

## 3. The layers

| Layer | Speed | Job |
|---|---|---|
| L0: hard rules (existing checker, GAME subset) | µs | Legality |
| L1: blocking-time evaluator (new) | ms | Throughput judge; source of learned constraints |
| L2: rig (real OpenTTD via `tools/ottd`) | minutes | Final judge; calibrates L1 |

### 3.1 L1: blocking-time evaluator

This is standard railway capacity analysis (blocking-time theory; UIC 406 compression).

**Inputs**
- A layout from the generator: tracks, signals and blocks, from the same block model as G9b and sigsim.
- Movements: the six (or more) origin→destination routes, each as a sequence of blocks with the length of track in each.
- A design train: from the picked train (D101) or the TL default, via `junction_train_model` (`StepJunctionTrain`, loaded mass, both acceleration models).
- A traffic mix: ML-heavy by default, the same as the rig's `--mix ml:N,bw:N,be:N`.
- A route-choice model: a simplified YAPF cost (curve penalties, the look-ahead red-signal penalty, lane balance). D100 showed route choice drives throughput, so this is needed from day one.

**Method**
1. **Running time.** For each movement, run the train model along the route (speed limits from curves and bridges) to get the times its head and tail pass each signal.
2. **Blocking time.** Each block is occupied from when the train needs the signal protecting it to be clear (its sighting/approach point plus one block of braking, following OpenTTD's path signals) until its tail clears the block. Along the route these form a staircase over time.
3. **Conflicts.** Two movements conflict on every block they share, including blocks that share a split or merge tile. Prio semantics (pre-signal combos) enter here: a joiner's Entry block is held while any ML train is in the coverage, which is exactly what G9b computes.
4. **Compression.** Pack the staircases of a cycle of the traffic mix as tightly as possible (a max-plus cycle time, or for small mixes a greedy schedule with a minimum-headway search). The result is the junction's minimum cycle time, plus per-movement measures.

**Outputs (the scores)**
- **ML delay:** the extra time an ML train spends compared with plain track at design speed. The target is zero: "doesn't slow trains down".
- **ML headway:** the minimum headway the junction allows on each ML, compared with the line's own headway (the D84 presets). The target is no worse than the line.
- **Branch capacity:** trains per hour each branch movement can sustain at the given mix.
- **Conflict explanation:** for each binding conflict, the pair of movements, the shared block(s) and the overlap time. This is what the learned constraints are built from (§3.3).

**Objective (replaces D114's order)**
1. Hard rules pass (L0).
2. ML delay = 0 and ML headway ≥ line headway (hard targets).
3. Branch capacity ≥ target (default: enough for the mix; user-tunable later).
4. Most compact (occupied tiles, D114).
5. Cheapest.

### 3.2 Calibrating against the rig

L1 is only trustworthy if it predicts the rig.
- **Scenarios:** the existing rig layouts (`circuit-LR-TL4*.json`), with and without each proxy rule's fix (for example the pre-D100 asymmetric bridges, a pre-D113 split straight into a bridge, a bridge 2 tiles longer than needed).
- **Comparison:** for each scenario, compare L1's predicted ML delay and branch capacity with the rig's measurements (`traffic` plus the `occupancy` GS command; stops per ML train, trains per hour per movement).
- **Acceptance:** L1 ranks the scenarios in the same order as the rig, and flags every known-bad layout from the session's history (the D100 lane bias, the D107 prio leak, the D113 waiting-bay block, the D103 long bridges).
- **Tests:** these become regression tests for L1.

### 3.3 Search: constraint solving with learned constraints (counterexample-guided)

**Variables**
- Structural positions along each row: SL split, lane split, near and far heads per lane, lane merge, shift-back, merge windows, prio coverage ends.
- Discrete choices: lane style, cluster boundaries (D109), and the bridge stagger (D118).

**Constraints**
- L0 game rules, encoded where they're simple (span ≤ bridge limit, head clears the crossing set, pieces legal).
- The proxy rules, as switchable seeds.
- Nearly all are difference constraints of the form "a ≥ b + k", which propagate in microseconds.

**Loop**
1. Solve for the most compact skeleton satisfying the current constraints. This is branch-and-bound, most compact first, with a small in-house solver; the domains are tiny, so there's no OR-Tools dependency.
2. Route the branches with the existing router, and place signals with the existing signal pass.
3. Run L0. A failure becomes a nogood ("not this combination"), and the loop goes back to 1.
4. Run L1. If a target is missed, turn the binding conflict into a learned constraint on the structure responsible (for example "the split at x and the head at x+1 share a block, so require a signal-legal tile between them"), add it, and go back to 1.
5. Otherwise accept the layout.

**What it buys**
- Margins come from the maths, not a sweep or a hedge (the engineer's standing rule).
- A proxy rule switched off is a direct experiment: does L1 re-learn it? Rules it never re-learns are dropped. Layouts that break a co-op rule yet meet the targets are new patterns.
- The checker stays the final judge of legality. Solver encodings of a rule are pinned to the checker by tests, so the two can't drift.

### 3.4 Pattern discovery (offline, later)

A quality-diversity search (MAP-Elites) over skeleton structures keeps the best layout for each shape (footprint, lane style, bridge count, prio length), judged by L1 and spot-checked on the rig. Its output fills the D96 instant-tier library, so the in-game tool stays within 100 ms by fitting library entries, while the refine tier runs the §3.3 loop on a worker thread.

## 4. Effect on the current queue

| Item | Change |
|---|---|
| SPEED pass (running) | Still worthwhile: the router and the signal pass stay in every design. |
| J1b (D117 + D118) | Hold the sweep-variant approach. Encode D117/D118 as seed constraints in step S2 instead, and use L1 to check they're needed. |
| D111 bending prios | Unchanged (signal pass and G9 work); L1 confirms the bent prio holds throughput. |
| D115 stubs, D58 crop | Unchanged; small. |
| AUDIT removal plan | Becomes L1-driven: switch off each COOP rule and measure. |
| D109 clusters | Becomes discrete choices in the S2 solver rather than another sweep axis. |
| M4 prio sized by solver | Folded into L1: prio length is a blocking-time result (D102 already is). |
| Rig traffic round | Becomes the L1 calibration (§3.2). |

## 5. Steps

Each step is one agent, test first, outside in.

- **S1: blocking-time evaluator (L1).**
  - Outer test: `EvaluateJunctionThroughput(layout, train, mix)` on the test spec reports ML delay 0, and reports a non-zero ML delay on a hand-made pre-D113 variant.
  - Inner tests: running time along a route; the block staircase; conflict detection; compression; the YAPF cost model.
  - Seam: layout plus train model only, no map access.
- **S1b: calibration.** Rig scenarios from §3.2, each with recorded measurements and L1's predictions. The ranking agreement becomes regression tests.
- **S1c: L1 as a scorer** in today's generator: score key 2 and 3 of §3.1 ahead of footprint. This gives immediate value with no search change.
- **S2: skeleton solver for the W-side doubled section** (SL split through to shift-back), with D110, D113, D117 and D118 as seeds. Run it side by side with the current generator on the test specs: same or smaller footprint, passes the checker, timing.
- **S3: learned-constraint loop** (§3.3 steps 3 and 4) for the W side.
- **S4: extend** to the E side, the prios and the clusters, then retire the parameter sweep.
- **S5: proxy audit.** Switch off each COOP seed and record what L1 learns. The results go into `AUDIT-assumptions.md` and DECISIONS.
- **S6: quality-diversity discovery** to fill the library (§3.4).

## 6. Open questions for the engineer

1. **Branch capacity target:** a fixed default (for example, enough for the rig's mix) or a user setting in the Junction window?
2. **ML delay:** strictly zero, or a small tolerance (for example under 1 s per train) when it buys a much smaller footprint?
3. **Route choice:** model YAPF only, or also NPF? NPF is non-default and probably out of scope.
4. **Calibration scenarios:** any known-bad or known-good layouts from #openttdcoop practice to add to the §3.2 set?
