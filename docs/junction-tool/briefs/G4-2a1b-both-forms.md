# G4 2a1b: every single-bridge crossing carries both forms; the form is data (D260, D261; DESIGN-G4 §11)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
A band edge crossed by a single bridge can pass under it by an S-bend (today) or square (Corner, run, Corner), and the joint model will choose (D260). This step makes the programs carry both forms and lets placement place either form **given as data** (`choices.form`, `choices.run`). The default is the S-bend form everywhere, so every layout, objective and level hash stays the same. Step 2a2 (later) lets the joint choose.

The design is `docs/junction-tool/DESIGN-G4-static.md` §11 (lines 272-418). It is your specification: §11.1 (programs, data), §11.2 (placement, the parts that apply at a fixed form: sizes, runs, both families; the q-guarded and curve-gap combination rows are 2a2's, not yours), §11.5 (fixed path and hover read the form; r fixed; default S-bend), §11.6 2a1 (changes and red tests). Read D260 and D261 in `docs/junction-tool/DECISIONS.md` (last rows).

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. It holds the uncommitted WIP of a stopped step (square-only crossings). First save nothing; run:
  `git -C /home/pete/OpenTTD/.claude/ng1 checkout -- . && git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-2a1b <HEAD given at launch>`,
  then apply the WIP as your starting point: `git -C /home/pete/OpenTTD/.claude/ng1 apply /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2a1/wip-stopped.patch`. It contains the square run family ("bridge over run") in `junction_placement.cpp`, the square-only programs, CLAUDE.md edits and tests. Keep what §11 keeps, change what §11 changes (the programs now emit both forms; the two deleted over-S-bend tests are restored; the CLAUDE.md lines are rewritten below).
- The design pass's probe (it implemented a form selector inside the programs and the token-0 fix, to measure) is a useful reference, not a template: `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g4a/probe_full_on_wip.patch`.
- `build` and `build-rel` exist (HiGHS configured); delete stale `.pch` files if the build complains. Leave untracked files alone.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2a1b/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part);
- DESIGN-G4 §11 (above) and §4 (lines 129-149);
- `src/junction/topology/junction_topology.h` (`Step`, `Step::Role`), `junction_topology.cpp` ~1195-1225 and ~1260-1275 (edge ends);
- `src/junction/placement/junction_placement.h` (`JunctionPlacementChoices`), `junction_placement.cpp` ~275-300 (joiner_div), the over-S-bend block ~1812-1870, the WIP's run family;
- `src/junction/placement/junction_render.cpp` ~110-125 (empty Fan skip);
- `src/junction/placement/junction_joint.cpp` (how a zero-size bend's length is held: the S fan pattern ~205-211);
- both CLAUDE.md files (topology, placement).

## Changes (as §11 specifies; named here so nothing else is added)
1. **Programs:** `Step::Role::Cross`. Per single-bridge band crossing: `SBend(h, Cross)`, `Corner(h, Cross)`, `Under/Above(x)`, `Corner(-h, Cross)`; a D258 group is these quadruples in sequence (§11.1). Role None no longer appears on a crossing's S-bend.
2. **Edge ends and joiner_div** (topo ~1266, jp ~282): an end token is a Diverge S-bend or a Corner that is not Cross; joiner_div means the start end names this Corner.
3. **Data:** `JunctionPlacementChoices::form` and `JunctionPlacementChoices::run`, shaped and indexed as §11.1 says (`form[e][tok]` at a group's first Cross token, 0 S-bend / 1 square, empty = all S-bend; `run[e][tok]` the square run's length r). Round-trip them wherever the other choices are copied (grep `over_after` for every site).
4. **Placement at a fixed form** (§11.2 sizes, runs, families; §11.5): one chain over all tokens; the unchosen form's bends are zero-size (no bend: no pieces, bits, occupancy, curves); run floors between consecutive present elements; runs between absent Corners are 0; the square family (the WIP's rows) for square groups, with r as the run length; the over-S-bend family for S-bend groups, its before/after search skipping Cross Corners; the "no crossed sbend" refusal stays.
5. **Render, route walk, ARR1** skip absent Cross tokens; ARR1's previous/next bend means the previous/next present bend. sym_routes treats a square crossing as a fixed span (§11.5).
6. **Joint** (`junction_joint.cpp`): only what §11.6 2a1 lists: an absent Cross S-bend's length held at 0 (the fan pattern), and absent Corners at 0. No q, no guarded rows, no curve-gap combination rows (those are 2a2).
7. **The recording names an X-build refusal** instead of returning `pt_count` 0 (today `SolveJunctionJoint` then crashes): the recording's failure list carries the refusal's text, and `SolveJunctionJoint` returns a non-optimal status naming it when the system has no points.
8. **CLAUDE.md** (standalone facts, no D-ids or references):
   - topology: a band edge crossed by a single bridge carries two forms of the crossing, an S-bend under the bridge and a square run (a Corner, the crossings, a Corner back); consecutive crossings of one hand share one run; the placement's choices pick the form; edge ends and node shapes never name a crossing's bends;
   - placement: replace the WIP's line with: a single bridge over a band edge crosses either the edge's S-bend (at a column its diagonal allows, the S-bend just before or just after) or its square run (the bridge column strictly inside the run, the run's row strictly between the heads); the form and the run length are choices; the unchosen form's bends are zero-size.

## Tests (red first; record each red message)
The five red tests of §11.6 2a1, named as there:
1. "programs: a single-bridge crossing carries both forms" (T, LL_RR x L_R, L_R x LL_RR, and the LL_RR x LL_RR group) `[junction][topology]`;
2. "edge ends and joiner_div never name a Cross token" (L_R x LL_RR e11; LL_RR x LL_RR e5, e20, e26) `[junction][topology]`;
3. "LL² at form 100100000 records round 0 without failure and its joint is Optimal at 268600" `[junction][milp][.sweep]` (the form string is the 9 crossings of LL_RR x LL_RR (pairing any) in the order §11.0 / `p3_ll2_group.txt` uses; read it there). If the recording fails at a different place than "a bridge over a run that is not a run", record what it is;
4. "LL² crossing 5 alone square records" `[junction][milp][.sweep]` (red: "path 6 has two horizontal headings"; green: the recording has no failure and r is the run's length);
5. "ARR1 at the S seeds names no absent piece" (T e4, L_R x LL_RR e11) `[junction][topology]`.
Restore the two over-S-bend tests the WIP deleted (`junction_joint.cpp`: "joint: a bridge over an S-bend may move along it", "joint: a bridge may pass over the S-bend after its crossing") unchanged, and keep the WIP's other new tests only if §11 still holds them true (state which you kept).

## Re-pins
Layouts, level hashes, identity and objectives: none (the default is the S-bend form). Recording sizes and system digests grow by the zero-size Cross elements: re-pin those only, listing each (old -> new), in the report and the commit body. Any other changed number is a STOP.

## Do not
- Add q, guarded rows, curve-gap combination rows, or any joint choice of the form (2a2).
- Retire t, w, `choices.over`, `choices.over_after`, `JunctionSBendColumnForm` or the over rows.
- Touch `.claude/nodes2` (reserved for the engineer's game) or any worktree but ng1.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- Any layout, objective, level hash or identity number moves.
- A seed that seeds today stops seeding, or its `CheckJunctionRules` changes.
- Anything §11 leaves open for this step.

## Gates
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`: all pass, report the time;
- `tools/junction/jt sweep build-rel`: all pass within its budget;
- `tools/junction/jt identity build-rel`: all pass unchanged.

Give every `jt` call a Bash timeout of at least 400000 ms.

One commit on wt-2a1b: `Feature: Junction single-bridge crossings carry an S-bend and a square form`, body listing the re-pinned digests, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read; the commit; red evidence per test; types, members and roles added;
- re-pins (old -> new);
- LL² at 100100000: recording, joint status and objective; crossing 5 alone square: recording;
- gate, sweep and identity counts and times;
- stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
