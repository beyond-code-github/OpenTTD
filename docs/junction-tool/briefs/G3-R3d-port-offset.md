# G3 R3d: the port offset is a variable of the joint model (DESIGN-G3 §G3.6 R1 inputs, D252)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
`docs/junction-tool/RECON-llrr-round0b.md` traces LL_RR x LL_RR's round-0 infeasibility to the port fan link that R3b added: `σ (port_row − y(item)) = L`.
- `port_row` is a constant: the recorded `port_offset` plus the slot's notation. So edge 2's fan caps stack row y4 at 26, while edge 1's 26-column S-bend needs y4 ≥ 35.
- In the fixed path, the port offset is a choice (`JunctionPlacementChoices::port_offset`). The joint model copies it.

D252 (orchestrator default): the port offset becomes an integer variable `O` of the joint model, and every port row in the model is `O + n_s`, with `n_s = port_row[s] − port_offset` at the recording.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes2`. First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-r3d <HEAD given at launch>`. Rebuild `openttd_test` in `build` and `build-rel` (delete stale `.pch` files if the build complains). Leave untracked files alone.
- Main at the launch head fails 3 known tests in the gate, which another agent (TT3) is fixing in `.claude/nodes`:
  - "topology: an L_R main with a multi-lane branch is refused, not crashed";
  - "bridge cut: the reported failure is the default cut's";
  - "joint: a port fan's size follows the rows".
  
  Expect exactly those, and report any other failure. Never touch `.claude/nodes`.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/r3d/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/RECON-llrr-round0b.md`;
- `src/junction/placement/junction_placement.h`: `JunctionSystemBend` (`port_row`), `JunctionPlacementSystem` (`port_offset`, `port_row`);
- `src/junction/placement/junction_placement.cpp`: `set_port_rows`, the fixed path's box check (grep "would lie on row"), and every read of `port_row[` or `port_offset` (grep). Confirm that no recorded relation's constant reads them. The port fan link in the joint and the box check are the only places they enter. If a recorded relation's constant does read them, STOP and list it;
- `src/junction/placement/junction_joint.cpp`: the port fan link (grep `b.source == "port_rows"`), `result.choices.port_offset`, the objective's tie-break terms (`EPS`).

## Changes (`junction_joint.cpp` only, unless the STOP above fires)
1. One integer variable `O`, with bounds `[max over slots s with n_s >= 0 of (−n_s), recorded port_offset + 64]`. `n_s = system.port_row[s] − system.port_offset`, over the slots where `system.port_row[s]` is a real row; read how `set_port_rows` skips the slots of other sides. The lower bound is the fixed path's box check ("would lie on row"). Objective `+ EPS O`, so ties keep the least offset.
2. The port fan link uses `(b.port_row − system.port_offset) + O` in place of `b.port_row`, in both the `m != 0` equality and the `m == 0` pair of inequalities.
3. `result.choices.port_offset` is the solved `O`.
4. Nothing else changes.

## Tests (`src/tests/junction/placement/junction_joint.cpp`)
1. **"joint: the port offset is the joint's choice"** (`[junction][milp][.sweep]`, red first).
   - LL_RR x LL_RR, round 0 at `JunctionFloorChoices`, recorded with no point, through `SolveJunctionJoint`.
   - CHECK that the status is not Infeasible.
   - WARN the solved offset against the recorded one, and the loop's outcome, rounds and last log line through `SolveJunctionJointLoop`.
   - Red: today Infeasible, with the conflict naming the stack and port rows. Record it.
2. **"joint: choices round-trip"** stays as it is: on T and LL_RR x L_R the solved offset equals the recorded one. If not, STOP and report both objectives.
3. "joint: the loop ends settled or names a reason": T and LL_RR x L_R still settle. WARN the others.

## Do not
- Change the fixed path, the recording, the port rows' notation, or the hover.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- A recorded relation reads `port_row` or `port_offset` in its constant.
- Identity, or an existing test other than the 3 known ones, changes.
- Anything not covered.

## Gates
- `tools/junction/jt fast build '[milp]' 10`: the known failure only;
- `tools/junction/jt gate build-rel`: the 3 known failures only;
- `tools/junction/jt sweep build-rel` (pass `240` as the budget argument: `jt sweep build-rel 240`);
- `tools/junction/jt identity build-rel`.

Give every `jt` call a Bash timeout of at least 400000 ms.

Update `src/junction/placement/CLAUDE.md`, standalone and without D-ids or references: the joint model chooses the port offset (the rows of the W/E ports move together), at least the box's first row. One commit on wt-r3d: `Feature: Junction joint model chooses the port offset`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- LL_RR x LL_RR: round-0 status, offset (recorded, solved), loop outcome, rounds, last line;
- the other four hubs' loop outcomes;
- gate, sweep and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop; leave no background processes.
