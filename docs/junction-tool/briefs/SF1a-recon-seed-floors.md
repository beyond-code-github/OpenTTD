# SF1a (recon by experiment): the seed's out-fan and doubled-leg floors next to their features (D212, D215)

Read-only recon by experiment: you may change constants in your worktree to measure, but commit nothing and leave the worktree clean (`git checkout -- src`). Keep your context small: send command output to files and report only what matters. Never use git stash. Mark anything not measured or read as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-sf1a <HEAD>` (given at launch).
- Engineer's rig findings on the LL_RR seed (DECISIONS D212, D215: a row change sits next to the feature it serves):
  1. Out-fans start JUNCTION_SHIFT_BACK_TILES = 4 tiles after the joiner merge (junction_topology.cpp ~:984, ~:1368 `along(heading, rp.fof, njm, JUNCTION_SHIFT_BACK_TILES, true)`): "where these prio joins end there's no curve or train speed reason we can't fan back in right away". The 4 is a constant copied from the old generator (D145 era), not a derived rule.
  2. A doubled leg's lane split sits right after the exit split, well before its bridge heads (~:1300-1312: `along(heading, rp.lane_split, nex, 2, false)`, `along(heading, rp.nh0[0], rp.lane_split, 2, false)`, minima, with the comment that series bridges need the slack, DESIGN-G1 §11.9b); likewise the lane merge after the last heads (`along(heading, rp.lane_merge, rp.fh1[g - 1], 2, false)`).
- Projection cannot fix either (P3b: the doubled SBend shares its class with the bridge heads; the out-fan's lean stops at this floor). So the seed's floors must be the rule minima, placed next to the feature.

## Read first
- DESIGN-G1.md §11.8-§11.9b, §12.4 (prio element: row upstream of the merge, step 0 beside M); RECON-prio-element.md §3-4; src/junction_topology.cpp the placement block ~:1280-1375; src/junction_chain.cpp JunctionCurveGaps; the checker rules the experiments hit (src/junction_rules.cpp, by violation name).
- src/tests/junction_topology.cpp: SignalledOf, "the LL_RR seed is clean", the T placement test.

## Experiments (T and LL_RR, test spec, SignalledOf pipeline, CheckJunctionRules with each topology)
1. **Out-fan:** replace JUNCTION_SHIFT_BACK_TILES in the out chain by k = 1, 2, 3, 4. For each k and topology: placement succeeds? box size; every checker violation (rule, tile, the row/lane and which side the fan moves relative to the prio row and the joiner). Per out row: the least k that is clean, and which rule binds at k - 1 (with the tiles).
2. **Doubled-leg lane split:** make `along(heading, rp.nh0[0], rp.lane_split, 2, ...)` exact (true), and separately `along(heading, rp.nh1[0], rp.sb1_last, 1, ...)` as it is. Does CutJunctionTrackBridges still find a cut (which cuts tried, which placement error)? Checker violations? If exact 2 fails, the least exact distance that places and is clean.
3. **Doubled-leg lane merge:** the same for `along(heading, rp.lane_merge, rp.fh1[g - 1], 2, ...)` exact.
4. With the best of 1-3 together: T and LL_RR clean? box sizes old -> new; Release time of the pipeline.

## Output
docs/junction-tool/RECON-seed-floors.md (untracked, ~80 lines): a table per experiment; for each floor, the binding rule at its least clean value with evidence (rule name, tiles, which tracks touch), and a proposed formula in terms of rule constants (cl, tl, prio n, signal slots, occupancy) — marked PROPOSED; anything that only a constant explains marked UNEXPLAINED.

## Report
The tables' conclusions, the proposed formulas, `git status --porcelain` (clean) and `git log -1`. When you have reported, stop.

## Addition (orchestrator, after P3b: every kind-4 S-bend is also rigid with neighbouring features in the template, so D215 must hold in the seed for all row changes)
5. **Every row-change token** (Fan, SBend, incl. doubled legs' SBends and the kind-4 S-bends of P3b's report: edges 12, 14, 15, 16, 17, 18 on LL_RR): for each, its feature per D215 (the node or bridge it serves: kinds 1-4 in briefs/P3b-every-row-change-leans.md), the placement constraint between it and that feature (file:line, the `along`/`eq` and its constant), its seed distance to the feature vs that constraint's minimum (at the minimum, or how many tiles early/late), and whether the least-x solve explains the gap (heading +x vs -x). A table, one row per token.
