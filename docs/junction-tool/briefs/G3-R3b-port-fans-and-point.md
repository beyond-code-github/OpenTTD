# G3 R3b: port fans follow the rows in the joint model; the loop detects only at a two-axis point (DESIGN-G3 §G3.6 F4, D246)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
`docs/junction-tool/RECON-fidelity.md` found two causes of the joint loop's failures on L_R x L_RR and L_R x LL_RR:
1. **A W/E port fan (bend source "port_rows") is a constant in the joint model**, held at its recorded size. Its size is really the row span from its stack item to its port row. Round 0 records it as 0, because the rows are unknown there. So the model plans with an empty fan that the solved rows do not give. Its zero-branch rows are also hard, because the bend has no zero variable.
2. **When round 0's recording has no X relations** (its Y solve failed), the joint's `x` is meaningless, all zeros. The loop still hands it to the next recording as the detection point. Detection there sees every track overlapping, and keeps dozens of wrong clashes for good.

After this step:
- A port fan's length is a variable, tied to the rows like the "rows" S-bends: L = σ (port row − y(item)), with σ, the fan's sign, as data (§G3.6 F4).
- The loop detects only at a point that has both axes.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-r3b <HEAD given at launch>`. Rebuild `openttd_test` in `build` and `build-rel` (delete stale `.pch` files if the build complains about them). Leave untracked files alone.
- Never touch the other worktrees under `.claude/`.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/r3b/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/RECON-fidelity.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.6 (lines 95-170, F4 on the fan sign);
- `src/junction/placement/junction_placement.h`: `JunctionSystemBend`, `JunctionBendSizes`, `JunctionPlacementSystem`;
- `src/junction/placement/junction_placement.cpp`:
  - `measure` (the port fan's m and hand);
  - `record_system` (where bends are recorded; grep "bend_id" or "source");
- `src/junction/placement/junction_joint.cpp`:
  - `SolveJunctionJoint`: the length variables by source, the zero-branch handling, and the row linking a "rows" S-bend to y (grep `b.source == "rows"`);
  - `SolveJunctionJointLoop`;
- `src/tests/junction/placement/junction_joint.cpp`: the helpers, the loop tests.

## Changes
1. **`JunctionSystemBend`** gains `int port_row = 0; ///< For a "port_rows" fan, the row of its port at the recording; else 0.` `record_system` fills it from `port_row[el.slot]`, as `measure` reads it.
2. **The joint model, a "port_rows" bend:**
   - The length is a variable, as for "rows": `L = model.Var(0)`, a zero variable `z` in [0, 1], `L >= z`, `L <= BIG z`, and `zero[b.id] = z`. So its zero-branch rows relax like those of a "rows" S-bend.
   - The row link, next to the "rows" link:
     - when the recorded `m != 0`: `σ = sign(b.port_row − y_item at the recording)`, and the row `σ (b.port_row − y[b.item_pt]) − L = 0`. The y at the recording is `b.port_row − σ m`: use the sign that `measure` gives as the hand, and write in a one-line comment how σ relates to it;
     - when the recorded `m == 0`: two rows, `L − (b.port_row − y[b.item_pt]) >= 0` and `L + (b.port_row − y[b.item_pt]) >= 0` (L ≥ |span|). The next recording fixes the sign.
   - `fixed_k` and `one_row` stay constants.
   - If `b.item_pt < 0` for a port_rows bend, STOP and report it.
3. **The loop's point** (`SolveJunctionJointLoop`): the next recording's `at` is:
   - the previous recording's `solved` when non-empty;
   - else the joint result's `x`, `y`, but only when the system handed to that joint solve had at least one X relation (axis 0);
   - else no point (`nullptr`).
   - Log "point: solved", "point: joint" or "point: none" on the round's line.

Nothing else changes: not the recording, the fixed path, the other sources, or the clash rules.

## Tests (`src/tests/junction/placement/junction_joint.cpp`)
1. **"joint: a port fan's size follows the rows"** (`[junction][milp]`, red first).
   - L_R x LL_RR at the loop's round-0 choices (all floors).
   - Solve the joint on the no-point recording, and record again at its choices with no point.
   - For every "port_rows" bend of the second recording, CHECK that its recorded `m` equals the joint's `length` for that bend id. At least one bend must have m >= 1.
   - Red: today the joint holds the recorded 0, and the second recording reads its rows. Record the message.
2. **"joint: the loop ends settled or names a reason"** stays. T and LL_RR x L_R still settle and place. WARN the three other hubs' outcomes, rounds and last log lines, as today.
3. "joint: choices round-trip" must stay exactly as it is. On T and LL_RR x L_R every port fan's joint L must equal its recorded m. If not, STOP and report the bend and both objectives.

## Do not
- Change the fixed path, the recording's relations, the curve-gap counters, the clash rows, or the t model.
- Make the port offset a variable.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- Identity or an existing test changes.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt sweep build-rel` (budget 150 s);
- `tools/junction/jt identity build-rel`. If a gate is over budget, re-run it once and report both times. Give every `jt` call a Bash timeout of at least 300000 ms.

Update `src/junction/placement/CLAUDE.md`, standalone and without D-ids or references: the joint model sizes a port fan from its rows (the span from its stack item to its port row, the side as data); the joint loop detects only at a point with both axes. One commit on wt-r3b: `Feature: Junction joint model sizes port fans from the rows`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- members added;
- per hub (all five): the loop outcome, rounds, and the last log line, with its point kind;
- gate, sweep and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
