# LR0: LL_RR junctions, read-only recon for planning (side workstream)

You execute this brief and nothing else. Read only: no source changes. If something is not covered, stop and report. Keep your context small: read with offset/limit and scoped grep; never cat whole large files.

## Goal
The orchestrator will design multi-lane (LL_RR, then up to LLLL_RRRR, D126) T junctions in the node model (DESIGN-nodes.md, DESIGN-N3.md). Collect, with file:line or decision id for every claim, what is already ruled, built and measured, so the design starts from facts.

## Read first
`docs/junction-tool/DESIGN-nodes.md` §2, §4a, §4b, §7, §9 (offset/limit); `docs/junction-tool/DESIGN-N3.md` (short); `docs/junction-tool/FABLE-nodes-perf.md` §1.3 (bundle edges) and §3.2.

## Collect (write `docs/junction-tool/RECON-llrr.md`, at most ~150 lines)
1. Rulings: every DECISIONS.md row that bears on multi-lane lines or junctions (at least D38, D60, D71, D72, D126, D135, D142, D161, D162, D176, D184; grep for lane, LL_RR, multi, bay, balanc, shifter, track choice). One line each: the ruling quoted short, and what it implies for an LL_RR T.
2. Code that already handles more than one lane per direction: notation parsing (ParseJunctionNotation, JunctionLineSpec, MultiLane errors), the line painter / line router multi-track support (LP*, O4*), lane geometry (RECON-lane-geometry.md, D145), B's two lanes in today's T (BuildParallelogramLanes, lane split/merge nodes), the checker's rules that assume one lane (grep CheckWaitingRoom, CheckDeadlock, CheckPrio, JunctionTMovements). For each: what it does with 2+ lanes today, file:line.
3. The movement set of an LL_RR T: what DECISIONS/plans say about which lanes connect to which (track choice, D71 "trains choose between tracks", balancing, waiting bays) and what is not yet ruled. List every open question you find, phrased as a question for the engineer, with the evidence that it is open.
4. Measurements on file for multi-lane (STATUS.md, FINDINGS-*.md, PERF*.md): numbers with sources.
5. Constraints the node model must add for bundles (FABLE §1.3, D176 whole bays, D71 waiting room): list them as the rule, its source, and whether it is a difference constraint, a discrete choice or neither.

## Do not
No source or test changes, no builds, no runs. No commits except one at the end on feature/junction-tool in /home/pete/OpenTTD adding only RECON-llrr.md: `Doc: Junction LL_RR recon`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Another agent commits on worktree branches; you commit only that file, by path. No bare `git stash`.

## Report (short)
Five lines: counts per section, the open questions (one line each), commit hash. When you have reported, stop.
