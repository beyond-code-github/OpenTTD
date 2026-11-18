# STATUS (current state only; rewritten 2026-11-18)

History lives in git. The previous STATUS: `git show d18117a214:docs/junction-tool/STATUS.md`. Planned work: ROADMAP.md. Rulings: DECISIONS.md, where later rows win and dead rows were removed on 2026-11-18. Current design: DESIGN-tracks.md.

## Where things are
- **Architecture:** the planar solver in `.claude/ng1`, branch `wt-planar` at c829db7742, pushed to fork. The old pipeline (topology, graph, placement, joint loop) is legacy: its code is in the tree, but it gets no new work.
- **Docs:** `feature/junction-tool`, pushed at session end.
- **Bests:**

  | Hub | Area | Variant |
  |---|---|---|
  | T2 | 862.25 | o1t3f1s0 |
  | T3 | 1933.25 | o1t3f1s0 |
  | LL² | 1347.25 | o17t0f1s0, found by local search; the rig still has 1374.25 (o49t0f1s0) |

- **Solver pipeline per variant:**
  - topology text → channels and face map;
  - shape: default shape and shape program, both scored (D341);
  - composer (fray, bundle, core, join components);
  - ladder length solve (MILP, HiGHS), with the lazy clash loop kept as a safety net;
  - signal pass (prio placed by search, D345).

  Variants: enumerated up to 6 bits, local search above that. Hard anchor (D348).
- **Solve times** are per variant, not per hub. A T2 variant takes about 200 ms on the default shape and about 340 ms on the program shape (mean over all variants, solved 4 at a time). The spread between variants is large, and it is mostly how quickly HiGHS rounds its relaxed answer (wip/SPIKE-slots.md context; STATUS history 2026-11-18).

## Current focus: the slot layer (ROADMAP item 1)
- The finding behind it: sideways position has no owner. Components hard-code S-bends, and anything unforeseen became an optional S-bend or a variant bit. Independent review: wip/REVIEW-sideways-modelling.md.
- The spike passed (wip/SPIKE-slots.md; branch `spike-slots` f4d1ef0e49, test only). On LL² and T2, every gap change between neighbouring lanes matches the solver, except today's staggered prio-join exits and turn geometry.
- NEXT: design the slot layer build (new DESIGN-tracks section: tracks per stretch from the topology, slots by a min-cost tension LP, components placed on slots, length solve with exact curves), test-first, landed in steps. D349 (prio rows as real tracks; neighbours bend round the prio; the prio may bend) lands through it.

## Waiting on the engineer
1. D193 vs D345: may the checker take part in acceptance (prio placement search), or is that a stopgap?
2. D119: are simulation-derived constraints still the long-term direction?
3. D206 vs D115a: does the 2-tile stub after a split stand?

## Parked (unchanged until the slot layer lands)
- **Ranking variants by the default-shape solve:** measured at about a 2x saving; the winner ranks 4th on T2 and T3, so the top 5 are needed.
- **The prio-join probes:** superseded by D349 through the slot layer.
- **LL³:** needs the n-lane composer (ROADMAP items 5 and 6).
- **Other workstreams** (station tool, line painter): parked since 2026-11-12.

## Operational
- **Rig:** servers on ports 4077 and 4079 belong to the orchestrator. Agents never touch them or start game clients, and only one GUI client runs at a time, on the engineer's normal config.
- **Tests:** `tools/junction/jt` (gate about 30 s against a 30 s budget; occasional flakes under load that pass when run alone) and `tools/junction/pv` (per-variant areas against a baseline).
- **The machine's CPU is unstable** (firmware pending): run one heavy job at a time.
