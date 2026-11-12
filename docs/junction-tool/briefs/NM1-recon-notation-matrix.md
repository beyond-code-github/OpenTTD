# NM1 (recon by experiment): which notation pairs the pipeline builds today, and where each one first fails (engineer: "At what point will we be able to put any combination in there and have it work?")

Recon by experiment: change code in your worktree to measure, commit nothing; save the experiment as a patch in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/nm1.patch and leave the worktree clean. Keep your context small: output to files, report only what matters. Never use git stash. Never start a game client or touch the engineer's game or rig servers.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-nm1 <HEAD>` (given at launch). Other agents work in /home/pete/OpenTTD/.claude/nodes2 and .claude/ng1: never touch them. Use absolute paths.
- LR1 (9927400a71) added `MakeJunctionHubTopology(main, branch)` (src/junction_topology.{h,cpp}): groups and movements from two parsed line notations (`ParseLineNotation`, src/junction_line.h). The window seeds the pipeline from it for any pair; the pipeline (MakeJunctionGraphSeed: paths, graph, order, structure, programs, cut, placement, render, signals) refuses some shapes with "not ruled yet" and other reasons.

## Experiment
A hidden test `[.nm1]` that, for every main notation with L, R in 1..3 and gap 1..3 and every branch notation with L, R in 1..3 and gap 1 (main gap varies; branch gap 1 only), at the test spec (MakeTestJunctionSpec, flat ground):
1. builds the topology, runs MakeJunctionGraphSeed and records: success, or the first failing stage and its error text (normalised: numbers stripped);
2. on success: render + signals + CheckJunctionRules: clean or the first rule/tile; box size; seed time (ms, Release);
3. groups the results: per failure reason (normalised) the count of pairs and 3 examples, and per stage.
Also run the successful ones through PlanJunctionHover on 5 real fixtures (planned count, ms max) as a first terrain signal.

## Output
docs/junction-tool/RECON-notation-matrix.md (untracked, ~60 lines): the matrix as a compact table (rows = main, columns = branch: OK / stage letter), the failure groups with counts and examples (file:line of each refusal), and what each group needs (a rule to design, a decision for the engineer, tunnels D220, size/time).

## Report
10 lines: pairs tried, built clean, failure groups with counts, the biggest blockers and what each needs. `git -C /home/pete/OpenTTD/.claude/nodes status --porcelain` (clean). When you have reported, stop.
