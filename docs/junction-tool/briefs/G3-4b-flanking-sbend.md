# G3.4b: a single bridge may pass over the S-bend after the crossing as well as before it (D248)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
`docs/junction-tool/RECON-llrr-round0.md` explains why the joint model of LL_RR x LL_RR is infeasible for every set of lengths:
- Edge 17's program is `SBend(19), Under(14), SBend(20)`, and both bends step east.
- The family "bridge over S-bend" ties edge 14's single bridge to the S-bend *before* the Under: bend 19, the one whose next token is the Under.
- The hug, the lane split runs and the head stagger of edge 29's doubled bridges put edge 14 at least 5 columns east of bend 19's end. So no t on bend 19 fits.
- The crossing fits on bend 20's diagonal, the S-bend *after* the Under.

D248 (orchestrator default): a single bridge passes over either S-bend next to the crossed edge's Under (or Above) token: the one before it, or the one after it. Which one is a choice:
- of the fixed path, defaulting to "before", today's;
- a binary variable of the joint model.

Identity does not change.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes2`. First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-g34b <HEAD given at launch>`. Rebuild `openttd_test` in `build` and `build-rel` (delete stale `.pch` files if the build complains about them). Leave untracked files alone.
- Another agent works in `.claude/nodes` on `junction_joint.cpp`'s length variables per bend source and `SolveJunctionJointLoop`'s point rule. Never touch that worktree. Keep your edits to the sites named here.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g34b/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/RECON-llrr-round0.md`;
- `docs/junction-tool/briefs/G3-4a-bridge-over-sbend.md` (the t model this step extends);
- `src/junction/placement/junction_placement.h`: `JunctionPlacementChoices` (`over`), `JunctionSystemOver`, `JunctionSystemRelation` (`over`, `d`);
- `src/junction/placement/junction_placement.cpp`:
  - the block "A single bridge passes over the S-bend it crosses";
  - `struct Cons`, `solve`, `flat_solve`, `record_system` (how `cons` and their tags become relations);
  - the choices check (grep "the choices do not fit");
- `src/junction/placement/junction_joint.cpp`: the `overs` part of `SolveJunctionJoint` (t variables, bound rows, the `over`/`d` term), `result.choices`, `SolveJunctionJointLoop`;
- `src/tests/junction/placement/junction_joint.cpp`: the helpers and the loop tests.

## Changes
1. **The choice.** `JunctionPlacementChoices` gains `std::vector<std::vector<int>> over_after; ///< Per edge, per token: for a single bridge's Over token, 1 when it passes over the crossed edge's S-bend after its Under (or Above) token, 0 the one before (DESIGN-G3 §G3.4b); empty means 0 everywhere.` The choices check accepts it empty, or shaped like `m`. Include it in the struct's equality.
2. **The fixed path block.** Per Heads element, find two candidates on the crossed edge `el.cross`:
   - `before`: today's, the non-fan SBend whose next token is the Under/Above of `e`;
   - `after`: the non-fan SBend whose previous token is that Under/Above.
   
   The chosen one is `after` when the choice says 1, else `before`. If the chosen candidate does not exist, fail with build_why "a bridge over an S-bend that is not there". The relations of the chosen candidate are exactly today's, with t as today.
   
   When the recording is on (`system != nullptr`) and the other candidate exists:
   - emit its relations as well, with the same t and form rules, as **record-only rows**. They are recorded, but never solved;
   - add `bool record_only = false;` to `Cons`, and have `solve` and `flat_solve` skip record-only rows;
   - tag both candidates' relations with which alternative they belong to: `JunctionSystemRelation` gains `int alt = -1; ///< When >= 0: the relation holds only when its over entry's alternative is alt (0 the S-bend before the crossing, 1 after).`, carried by a new `SystemTag` member `int alt = -1;`;
   - when a bridge has only one candidate, its relations keep `alt = -1`.
3. **The record of an over entry.** `JunctionSystemOver` gains:
   - `int bend_after = -1;` the after candidate's bend id, -1 when none. `bend` stays the before candidate's id, -1 when none;
   - `int t0_after = 0, t1_after = 0;` its diagonal form;
   - `int chosen = 0;` the choice at the recording.
   
   The `t` bound for the before candidate stays in `t0`, `t1`. If only the after candidate exists, its form goes in `t0_after`, `t1_after`, with `bend = -1`.
4. **The joint model.** Per over entry with both candidates:
   - one binary `w` (1 = after);
   - relations with `alt = 0` are relaxed by `+ BIG w` on their slack side, and those with `alt = 1` by `+ BIG (1 - w)`, the same relaxation form the model already uses for vanishing bends. Write each row once in a comment;
   - the t bound rows, `t - t1 L(bend) <= t0 + BIG w` and `t - t1_after L(bend_after) <= t0_after + BIG (1 - w)`.
   
   With one candidate: no `w`. Its relations and bound are as today, using whichever form exists.
   
   `result.choices.over_after` is filled from `w` (shaped like `m`), or left empty when every w is 0. `SolveJunctionJointLoop` carries it through `choices`, like `over`.
5. Nothing else changes.

## Tests (`src/tests/junction/placement/junction_joint.cpp`)
1. **"joint: a bridge may pass over the S-bend after its crossing"** (`[junction][milp][.sweep]`, red first).
   - LL_RR x LL_RR at the loop's round-0 choices (all floors).
   - CHECK that the round-0 joint solve's status is not Infeasible with a conflict naming "bridge over S-bend".
   - WARN the status, edge 14's chosen alternative and t, and the loop's outcome, rounds and last log line through `SolveJunctionJointLoop`.
   - Red: today's infeasible conflict. Record it.
2. **"joint: choices round-trip"** and **"joint: the loop ends settled or names a reason"** stay exactly as they are. If the joint picks `after` on T or LL_RR x L_R, STOP and report the bridge and both objectives.

## Do not
- Change the S-bend form, the t bound, or any other family.
- Touch the length variables per bend source or the loop's point rule (the other agent's change).
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- Identity or an existing test changes.
- A recorded relation whose meaning depends on the alternative cannot be tagged by `alt`.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt sweep build-rel` (budget 150 s);
- `tools/junction/jt identity build-rel`. If a gate is over budget, re-run it once and report both times. Give every `jt` call a Bash timeout of at least 300000 ms.

Update `src/junction/placement/CLAUDE.md`, standalone and without D-ids or references, in the line about a bridge over an S-bend: the bridge may pass over the crossed edge's S-bend just before or just after its crossing; the joint model chooses which. One commit on wt-g34b: `Feature: Junction bridge over either S-bend beside its crossing`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- types and members added;
- LL_RR x LL_RR: the round-0 status, the choice and t, and the loop's outcome, rounds and last log line;
- gate, sweep and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
