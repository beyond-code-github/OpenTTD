# G3.4a-2: land the bridge-over-S-bend column t on the joint loop (DESIGN-G3 §G3.4a, D240, D246)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
An earlier step, G3.4a (brief `docs/junction-tool/briefs/G3-4a-bridge-over-sbend.md`), wrote the column t of a single bridge over an S-bend as a choice of the fixed path and a variable of the joint model. Its Part A premise (P3, S-bend columns affine in t) held. It stopped only because round 0 of LL_RR x LL_RR was still infeasible on a partial recording. Its work is saved as `docs/junction-tool/wip/g34a-stopped.patch`.

Since then:
- R1 (bend sizes as inputs), R2 (`detect`) and R3a (`SolveJunctionJointLoop`, recordings at a point) have landed. R3a removed the partial-recording cause.
- The joint loop's conflicts on L_R x LL_RR and LL_RR x LL_RR still run through "bridge over S-bend".

This step ports the patch onto today's code and runs the loop with it.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-g34a2 <HEAD given at launch>`. Rebuild `openttd_test` in `build` and `build-rel` (delete stale `.pch` files if the build complains about them). Leave untracked files alone.
- Another agent works in `.claude/nodes2` on band-edge bridges (the non-row chain builder, `collect_bridges`, `heads_of`). Never touch that worktree.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g34a2/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- the G3.4a brief named above (all of it: its Parts A to C and names are the spec here, except where this brief overrides them);
- `docs/junction-tool/wip/g34a-stopped.patch` (all);
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.4a;
- the code the patch touches, at today's head;
- `src/junction/placement/junction_joint.cpp` (`SolveJunctionJoint`, `SolveJunctionJointLoop`) and `src/tests/junction/placement/junction_joint.cpp` (the helpers, "joint: choices round-trip", "joint: the loop ends settled or names a reason", `[.g3loop]`).

## Changes
1. **Port the patch.** Apply `g34a-stopped.patch`'s changes to today's code: Part A (`JunctionSBendColumnForm` and its P3 test), Part B (`JunctionPlacementChoices::over`, `JunctionSystemOver`, `overs`, the relation and SystemTag members `over` and `d`, the fixed-path relations at t) and Part C (the joint's t variables and terms, `result.choices.over`). Resolve conflicts by keeping today's code, with the patch's additions on top.
   - Where the patch and today's code disagree in a way that is not a plain conflict, STOP and report it.
2. **The bound at L = 1.** The fixed path keeps `t <= t0 + t1 max(m, 2)`. The joint row stays `t - t1 L <= t0`. It is stricter at L = 1, so every joint t is accepted by the fixed path. Write this in a one-line comment at the joint row.
3. **The loop carries t.**
   - `SolveJunctionJointLoop` hands the joint result's `choices`, which now include `over`, to the next recording, as it already does for `m`.
   - Its log line gains the sum of t - 1.
   - Equality of choices for "settled" and for the cycle rule includes `over`, through the existing comparison.

## Tests (`src/tests/junction/placement/junction_joint.cpp` and `src/tests/junction/junction_track_geometry.cpp`)
1. The patch's P3 test "S-bend columns are affine in the column", as in the patch.
2. **"joint: a bridge over an S-bend may move along it"** (`[junction][milp][.sweep]`). Its premise changes from the old brief:
   - LL_RR x LL_RR and L_R x LL_RR at all-floor choices, through `SolveJunctionJointLoop`.
   - CHECK that no round's conflict, and not the final outcome, contains "bridge over S-bend". This is the red check: run it before Part C exists, record the failure, then make it pass.
   - WARN each hub's outcome, rounds, the last log line, and every `overs` t at the last round.
   - If a hub settles, place it through `PlaceJunctionTrackGraph` at the result's choices, and WARN whether it places and the error.
3. **"joint: choices round-trip"** stays exactly as it is, with `over` all 0 on T and LL_RR x L_R. If the joint picks t != 1 there, STOP and report the bridge and both objectives.
4. "joint: the loop ends settled or names a reason" stays as it is: T and LL_RR x L_R settle and place.

## Do not
- Add a port-offset variable, fan signs, or anything for band-edge bridges.
- Change the loop's rules beyond carrying `over`.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Identity or an existing test changes.
- P3 fails at today's head.
- A non-trivial disagreement between the patch and today's code.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt fast build '[geometry]' 5`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt sweep build-rel` (budget 90 s; if it goes over, report the per-case times and STOP);
- `tools/junction/jt identity build-rel`. If a gate is over budget, re-run it once and report both times. Give each `jt` call a Bash timeout of at least 300000 ms.

Update `src/junction/placement/CLAUDE.md`, standalone and without D-ids or references: a single bridge over an S-bend may cross it at any sideways column the S-bend's diagonal allows; the joint model chooses that column. One commit on wt-g34a2: `Feature: Junction bridge over an S-bend chooses its column`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- types and members added;
- per hub (all five): the loop outcome, rounds, the last log line, and the t values;
- gate, sweep and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
