# G3.3 SP: joint-solve scale probe on wider hubs (measurement only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Measure how the joint model scales on wider hubs. Pete wants real numbers in place of an extrapolation.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-g33sp <HEAD given at launch>`. Builds are configured with HiGHS.
- Another agent works in `.claude/nodes2`: never touch it.
- Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g33sp/`.

## Read first (once each)
- `src/tests/junction/placement/junction_joint.cpp`: the `DefaultCut` helper, and how test 3 records a failing topology and solves it;
- `src/junction/placement/junction_joint.h`;
- `src/tests/junction/topology/junction_topology.cpp` 340-380 (how LLL_RRR and LLLL_RRRR hubs are made).

## Work
One hidden test, `[.g3scale]`, in `src/tests/junction/placement/junction_joint.cpp`. It is never in a gate.
- Hubs, each under pairing any at `MakeTestJunctionSpec()`: T, LL_RR x L_R, LLL_RRR x L_R, LLL_RRR x LL_RR, LLL_RRR x LLL_RRR, LLLL_RRRR x L_R, LLLL_RRRR x LLLL_RRRR.
- For each hub, run the stages in order:
  - graph, order, structure and programs: on the first failure, WARN the stage and its error and go on to the next hub;
  - the seed: places, or not;
  - record the system, as test 3 does: with the seed's choices if it places, else at the default cut;
  - solve the joint model 5 times.
- Per hub, WARN one line:
  - the hub, and how far it got;
  - the bend, relation and route counts;
  - the joint status and objective;
  - the median solve ms over the 5 runs;
  - whether every L is integral in the LP relaxation (solve once with every variable continuous, if `JunctionMilp` lets you add variables as continuous; it does, through `integer = false`);
  - the conflict's length if the solve is infeasible.
- To build the relaxed copy, add a `bool relaxed` parameter to `SolveJunctionJoint` with default false, which adds every variable as continuous. This is the only production change allowed.
- Also time the seed (`PlaceJunctionTrackGraph`, median of 3) for comparison.

Run the test in Release (`build-rel`). If a hub takes more than 60 s in any stage, abort that hub, WARN it, and go on.

## Do not
- Change any rule, relation or stage.
- Commit any data file.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Gates
- 0 new warnings;
- `tools/junction/jt gate build-rel` (the hidden test is not in it).

One commit on wt-g33sp: `Add: Junction joint-solve scale probe (hidden test)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- the commit;
- the table, one row per hub, verbatim from the WARN lines;
- gate count and time;
- stops;
- `git status --porcelain` and `git log -1`.
When you have reported, stop.
