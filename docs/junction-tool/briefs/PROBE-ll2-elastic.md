# PROBE: the elastic joint on LL_RR x LL_RR (REVIEW-G4-overprescribed Q3 "Probe E"; D276-D280)

You execute this brief and nothing else. It is a probe: nothing is merged. Make exactly the changes listed, in test code only. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## Goal
Find which of our rule groups stand between LL² and a legal placement, by giving every row of the joint model a slack and minimising the total slack. Read `docs/junction-tool/REVIEW-G4-overprescribed.md` Q3 "Probe E" first: it is the specification (rows, groups, two phases, instances, what each outcome means). This brief adds variants and fixes the outputs.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-probe-ll2 <HEAD given at launch>`. Leave its untracked files alone. Release `build-rel` exists; build only `openttd_test`.
- Output files: `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/probe-ll2/`.
- Another agent works in `.claude/o4c2`. Never use git stash; never start a game client (the engineer's client is running); never touch ports 4077/4079.

## Read first (offset/limit)
- `REVIEW-G4-overprescribed.md` Q3 (lines 79-134).
- `src/junction/placement/junction_joint.cpp` (jj): the row-building sites the review names (:120, :279, :289, :343, :376) and how `JunctionMilp` is called; `src/junction/placement/junction_milp.h`.
- `src/tests/junction/placement/junction_joint.cpp` (tjj) 1280-1430 (`MakeLoopStart`, `Ll2Start`, `ChoicesWithForms`, `RecordAt`, the keep topology at 1312-1322, the place-at-choices check at 1291-1298).

## Changes (test-only; a probe patch)
- An elastic variant of the joint solve, reachable only from a probe test (a test-local copy of the solve function, or a test-only flag in the joint's options struct if one exists; if neither is possible without changing a public header, stop and report): one nonnegative continuous slack per row on its binding side, grouped by the review's group table; phase 1 minimises total slack; phase 2 caps it at phase 1's value and minimises the flat cost.
- One probe test case tagged `[.probe]` in a new file `src/tests/junction/placement/junction_probe_ll2.cpp` running the instances below, writing one result file per instance.

## Instances (the review's three, plus D278-D280 variants)
1. LL² any, form 100100000, port offset 24, round 1 recorded at the round-0 joint point.
2. LL² keep, round 0 and round 1.
3. LL² keep at LL_______RR on both lines (gap 7), round 0 and round 1.
4. LL² keep at gap 7 with `spec.prio_tiles = 9` (the coop standard prio).
5. LL² keep at gap 7 with every single-bridge band crossing set to a tunnel, the way the tests that use tunnels set it (grep the tests for `tunnel = true`); if setting tunnels is not possible at LL², report why and skip.
For each instance and round: the phase-1 total slack; per group the rows with slack, their sum and maximum and up to 5 reason strings; the phase-2 flat cost; the joint's span (columns x rows of its points); solve times. At zero total slack: place at the joint's choices, render, run `CheckJunctionRules(layout, spec, topology)`, and list its violations by rule; save the rendered layout's ASCII.

## Output
- `docs/junction-tool/PROBE-ll2-elastic.md`: a summary table (instance x round: total slack, groups with slack, span, checker violations), then per instance the details above, then which of the review's outcomes (a)-(d) each instance shows.
- Save the probe as a patch: `git -C /home/pete/OpenTTD/.claude/nodes diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-ll2-elastic.patch` (include new files), then a WIP commit on the branch so the worktree is clean.
- If any instance reaches zero slack and renders: also save the layout's JSON (the existing layout JSON writer, if one exists; grep `junction_export`) for a rig build later.

## Do not
- Do not change any non-test source file except, if unavoidable, a test-only option the brief allows; do not change any existing test.
- Do not merge or push.
- Budget: stop after about 2 hours of runs and report what finished.

## Report
The summary table, the outcome per instance, the patch path, `git status --porcelain` and `git log -1` of the worktree. When you have reported, stop.
