# TL1a (recon by experiment): which seed floors do not scale with train length and speed

Read-only recon by experiment: you may change spec values and constants in your worktree to measure, but commit nothing and leave the worktree clean (`git checkout -- src`). Keep your context small: send command output to files and report only what matters. Never use git stash. Mark anything not measured or read as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-tl1a <HEAD>` (given at launch).
- Engineer: "the train length maths are supposed to be coded into the graph". SF1b found the LL_RR seed has CurveSpeed (rule 3) violations at tl 5 ((25,7), (44,15), (27,13)), and more incl. rule 7 at tl 6-7. Every seed test runs at the test spec (tl 4, reference_speed 160). Placement (src/junction_topology.cpp, the block ~:1240-1420) mixes derived floors (g1/g2 from JunctionCurveGaps: CS1's d_min, SF1b's k_out; prio even_prio) with fixed constants (2, 1, STUB, ...).

## Read first
- DESIGN-G1.md §11.8 (gap table), §12.4; RECON-seed-floors.md; src/junction_chain.cpp JunctionCurveGaps; the placement block; MakeTestJunctionSpec and how tl, curve_speed, reference_speed, prio_tiles derive (JunctionSpec, SizeJunctionPrio if used).
- src/tests/junction_topology.cpp SignalledOf and "the LL_RR seed is clean".

## Experiments
1. Sweep the seed pipeline (SignalledOf: placement, render, signal tokens, prio element) for the T and LL_RR over tl 3..8 x reference_speed {100, 120, 160, 196} (derive prio_tiles and anything else the spec derives from them the way the game does; say how). Per cell: placement ok or error text; CheckJunctionRules violation counts per rule; box size.
2. For each distinct violation (dedupe across cells by rule and the graph elements involved): the two curves (or elements) involved, which edges/tokens they belong to, the units between them vs the rule's minimum, and the placement constraint (file:line, the `along`/`eq` and its constant) that sits between them. Mark: DERIVED-but-wrong, CONSTANT (does not scale), or MISSING (no constraint between them at all).
3. For each CONSTANT or MISSING one: a PROPOSED derived floor from the rules (JunctionCurveGaps pairs, signal slots, prio n), and check it by experiment (apply it, rerun the sweep): does the violation disappear without new ones?
4. Release time of the seed pipeline at the largest cell.

## Output
docs/junction-tool/RECON-train-length.md (untracked, ~100 lines): the sweep table (compact), the violations table, the proposals with their check results; anything only a constant explains marked UNEXPLAINED.

## Report
The conclusions, the proposals, `git status --porcelain` (clean), `git log -1`. When you have reported, stop.
