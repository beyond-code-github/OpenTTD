# ST1a (recon by experiment): which relations placement is missing, found by stretching every relation (DESIGN-G1 §14.3, §14.7 T1b)

Read-only recon by experiment: you may change code in your worktree to measure, but commit nothing and leave the worktree clean (`git checkout -- src`; delete any scratch file you add). Keep your context small: send command output to files and report only what matters. Never use git stash. Mark anything not measured or read as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-st1a <HEAD>` (given at launch).
- T1 merged: PlaceJunctionTrackGraph solves on JunctionPositions; every relation carries a JunctionPlacementReason (rule, edge, node, row); the overload with JunctionPlacementChoices solves once with the choices fixed. TL1b: every floor is derived; the least solution is clean at every tl/speed swept.
- Projection will move points off the least solution (terrain facts push them later). The relations must keep the layout legal at ANY solution, not only the least. Plan §14.3 suspects three gaps: curve-gap pairs not covered by a floor, occupancy (unrelated tracks touching; today left to the render), and bridges (the copy-0 heads "hug" crossers by a pick, `choices.hug`). This recon measures which are real, before any is designed.

## Read first (once each, offset/limit)
- DESIGN-G1.md §14.2-§14.5; RECON-train-length.md (its method and table style); src/junction_topology.h JunctionPlacementReason, JunctionPlacementChoices, JunctionTrackPlacement; src/junction_topology.cpp the placement helper (the `ge`/`eq` lambdas, reasons, `solve`, the fixed branch); src/tests/junction_topology.cpp the `[topologysweep]` tests and SamePlacement.

## Experiments (Release; scratch hidden test case; T and LL_RR; cells: the test spec, tl 3 v100, tl 6 v196 with sized prio as in [topologysweep])
1. **Stretch one relation at a time:** per cell, take the seed placement p (CutJunctionTrackBridges) and its choices. For every relation of the fixed solve that is a lower bound only (a `ge` not half of an `eq`), in X and separately in Y: re-solve with the fixed choices and that relation's k raised by s in {1, 2, 5} (a scratch hook in the helper is fine). Per run: solve ok or the error; render (RenderJunctionTrackPlacement) ok or the error; signal tokens and prio element as SignalledOf; CheckJunctionRules violations by rule.
2. **Dedupe:** each failure keyed by (the stretched relation's reason, the violated rule, the graph elements the violation touches: map its tile to route piece tags). Table: key, cells and s where seen, an example (tile, message), and the class: CURVE GAP (which JunctionCurveGaps pair; the two curves' edges/tokens; units vs min), OCCUPANCY (which two edges share or touch a tile), BRIDGE (crossers outside the heads, heads not hugging, a span over spec.max_bridge_middle), RENDER (render error text), OTHER (rule name).
3. **For each class:** the missing relation in terms of placement points and the reasons' elements (proposed, as TL1a did); where it is a simple difference, check it by experiment (add it, rerun experiment 1): the class disappears without new violations and the least solution is unchanged (SamePlacement against p). Mark PROPOSED / CHECKED / UNEXPLAINED.
4. **Bridges specifically:** in stretched runs, is `choices.hug` still the outermost crosser tile pair? If not, show one case (bridge, crosser, values). Then describe what fixed relations would make bridge + heads + crossers one rigid group (the crossers' order along the bridge, the spacing between consecutive crossers, the heads to the outermost): measured spacings at the seed, and whether each is a rule floor (which rule) or a constant.
5. Counts: relations stretched per cell, runs, failing runs; Release time of the whole sweep.

## Output
docs/junction-tool/RECON-stretch.md (untracked, ~120 lines): the counts, the deduped failure table, per class the proposals with check results, the bridge section.

## Report
The conclusions in 10 lines, the proposals, `git status --porcelain` (clean), `git log -1`. When you have reported, stop.
