# G3 R3c: a kept clash's order is a choice of the joint model (DESIGN-G3 §G3.6, D249)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
`SolveJunctionJointLoop` keeps every occupancy pair a recording detects as a clash row, `b.left − a.right >= gap`, in the order the detection point gave it. The first time the loop detects at a joint solution (L_R x L_RR round 3, L_R x LL_RR round 1), many unrelated tracks overlap at that point. Their order there is arbitrary, so the 99-114 rows kept in that order contradict the static rows, and the joint goes infeasible. The joint model is an integer program, and it can choose the order.

D249 (orchestrator default): every kept clash that is not a tie gets one binary `o`:
- `o = 0`: b after a, `b.left − a.right >= gap`, today's row;
- `o = 1`: a after b, `a.left − b.right >= gap_reversed`.

Ties stay as they are.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-r3c <HEAD given at launch>`. Rebuild `openttd_test` in `build` and `build-rel` (delete stale `.pch` files if the build complains about them). Leave untracked files alone.
- Another agent works in `.claude/nodes2` on the `overs` part of `junction_joint.cpp` and the "bridge over S-bend" block of `junction_placement.cpp`. Never touch that worktree. Keep your edits to the sites named here.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/r3c/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `src/junction/placement/junction_placement.h`: `JunctionSystemColumn`, `JunctionSystemClash`;
- `src/junction/placement/junction_placement.cpp`: `add_occupancy` and its `add_relation` (grep "recorded_clashes.push_back"), and the Occupant's `left`/`right` columns and `bits`;
- `src/junction/placement/junction_joint.cpp`: the clash rows (grep "The clashes:"), the conflict reporting (grep "clash >= 0"), and `SolveJunctionJointLoop` (how clashes are kept by `key`/`reversed`);
- `src/tests/junction/placement/junction_joint.cpp`: the helpers and every test that reads clashes (grep "clash").

## Changes
1. **`JunctionSystemClash`** gains:
   - `JunctionSystemColumn a_left;` and `JunctionSystemColumn b_right;`, a's left and b's right columns, formed like `a_right` and `b_left`;
   - `int gap_reversed;`: the gap when a lies after b. It is 2 when the reversed pair touches (b's bits reach SW, a's reach NE, the mirror of the existing `touch` test), else 1; for a tie, equal to `gap`;
   - `bool tie;`: whether it was recorded as a tie.
   
   `add_relation` fills them where it records the clash.
2. **The joint's clash rows.** For a clash with `tie == false`:
   - add a binary `o`, `model.Var(0, 1)`, marked integer as the model marks its other binaries;
   - the existing row gains `− BIG o` on its left side: it holds when `o = 0`;
   - a second row, `a.left − b.right >= gap_reversed`, built from `a_left` and `b_right` exactly as the first is built from `b_left` and `a_right`, with the same per-bend `BIG (1 − z)` relaxation. It gains `− BIG (1 − o)`, so it holds when `o = 1`;
   - both rows carry the clash index, so a conflict names the clash's reason;
   - write the pair once in a comment.
   
   A clash with `tie == true` keeps today's single row.
3. Nothing else changes: not the recording's relations, the fixed path, the loop's keeping rule (by key or reversed), or the point rule.

## Tests (`src/tests/junction/placement/junction_joint.cpp`)
1. **"joint: a clash's order is the joint's choice"** (`[junction][milp]`, red first).
   - A hand-made `JunctionPlacementSystem` with two points beside the origin, and one clash between two occupants of width 0 whose recorded order (b after a) contradicts a relation `x[a] − x[b] >= 3`.
   - Build it the way the existing hand-made system tests build theirs. If none exists, use the smallest recording available (T) and add the clash and the relation to it, and say which you did.
   - CHECK that the joint solve is Optimal, with a after b.
   - Red: today it is Infeasible. Record the message.
2. **"joint: choices round-trip"**, **"joint: reproduces the prototype ..."** and the other `[milp]` tests stay as they are.
   - If one changes because the joint now picks the reverse order of a kept clash on T or LL_RR x L_R, STOP and report: the clash, both objectives, and whether the fixed path places at the new choices.
3. **"joint: the loop ends settled or names a reason"** stays. WARN the three other hubs' outcomes, rounds and last log lines.

## Do not
- Make ties disjunctive.
- Change the loop, the recording's relations or the fixed path.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- Identity or an existing test changes.
- A clash's reversed gap cannot be derived from the occupants' bits.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt sweep build-rel` (budget 150 s);
- `tools/junction/jt identity build-rel`. If a gate is over budget, re-run it once and report both times. Give every `jt` call a Bash timeout of at least 300000 ms.

Update `src/junction/placement/CLAUDE.md`, standalone and without D-ids or references: the joint model chooses the order of each occupancy pair it keeps (either track may come first, at its own gap); a tie keeps its recorded offset. One commit on wt-r3c: `Feature: Junction joint model chooses the order of occupancy pairs`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- members added;
- per hub (all five): the loop outcome, rounds, and the last log line;
- gate, sweep and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
