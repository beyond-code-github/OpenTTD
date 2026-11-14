# G4 2a1: a band edge passes under a single bridge square (D256, D258; DESIGN-G4 §4, §8)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Today a band edge crossed by a single bridge emits `SBend(hand)`, `Under/Above(x)`, and placement ties the bridge to that S-bend's diagonal ("bridge over S-bend"). The engineer ruled (D256, D258 in `docs/junction-tool/DECISIONS.md`) that it passes under square:
- one crossing: `Corner(hand)`, `Under/Above(x)`, `Corner(-hand)`: the edge turns east-west, runs under the bridge on plain track, and turns back;
- consecutive single-bridge crossings of the same hand on one edge: one run under all of them, `Corner(hand)`, `Under/Above(x1)`, `Under/Above(x2)`, ..., `Corner(-hand)`. A crossing of the other hand closes the run and starts a new one;
- the Corners are ordinary chain bends under every curve rule (JunctionCurveGaps, CL from the train); the run between them is as long as those rules and the floors need.

Placement gains the static family "bridge over a horizontal run". The old over-S-bend code stays in this step (unreachable afterwards); step 2a2 deletes it.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-2a1 <HEAD given at launch>`. `build` and `build-rel` exist (HiGHS configured); delete stale `.pch` files if the build complains. Leave untracked files alone.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2a1/`.
- The F5 probe patch (programs part only is useful): `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/f5/probe.patch`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part);
- `docs/junction-tool/DESIGN-G4-static.md` §4 (lines 129-149) and §8 2a1 (lines 204-215);
- `docs/junction-tool/DECISIONS.md`: D256, D258;
- `src/junction/topology/junction_topology.cpp` ~1195-1225 (band programs, b. Crossings);
- `src/junction/placement/junction_placement.cpp`: how a Corner element of a band chain is built (grep `K::Corner`, ~1734-1738), `run()` (~1817), the over-S-bend block (~1812-1870, `build_why = "no crossed sbend"`), and how a single bridge's Heads element is found for a crossing (grep `K::Heads`);
- `src/junction/topology/CLAUDE.md` and `src/junction/placement/CLAUDE.md`.

## Changes
1. **Programs** (`junction_topology.cpp`, band programs, step b): where `bridge_over(x, e)` emits `SBend(hand)` then `Under`/`Above`, emit `Corner(hand)`, `Under`/`Above`, `Corner(-hand)` with the same hand rule (`s_side_slot(e) < s_side_slot(x) ? +1 : -1`), both Corners index `Step::NO_ROW`. When the previous crossing of this edge also took this form with the same hand, and nothing was emitted after its closing Corner, drop that closing Corner and append only `Under`/`Above`, `Corner(-hand)` (D258).
2. **Placement** (`junction_placement.cpp`, in `build()` next to the over-S-bend block): for a single bridge (Heads element of edge e over x) whose crossed edge x has, around its Under/Above token for e, a Corner before and a Corner after with only Under/Above tokens between them, emit the family "bridge over a horizontal run", citing D256, and skip the over-S-bend block for that crossing. With Corner1 the Corner before and Corner2 the Corner after, and the run heading east (mirror for west):
   - X phase: `x(Corner1.last) + 1 <= x(heads column)` and `x(heads column) <= x(Corner2.first) - 1`: the bridge column is strictly inside the run;
   - Y phase: `y(north head) + 1 <= y(Corner1.last)` and `y(Corner1.last) <= y(south head) - 1`: the run's row is strictly between the heads;
   - each relation with `why("bridge over run", e, ...)` in the style of its neighbours, so the recording carries them.
   If the elements are not as described (for example a Corner element sized 0 or missing), STOP and report what you found.
3. **CLAUDE.md** (standalone facts, no D-ids or references):
   - topology: replace the fact that a band edge crossed by a single bridge S-bends under it with: a band edge crossed by a single bridge passes under it square (a Corner, the crossings, a Corner back); consecutive crossings of the same hand share one run;
   - placement: replace "A single bridge over an S-bend may cross it at any sideways column ... chooses which." with: a single bridge over a band edge crosses its horizontal run: the bridge's column lies strictly inside the run and the run's row strictly between the heads; the Corners obey the curve rules.

Nothing else changes in `src/junction/`.

## Tests (outside-in, red first; record each red message)
In `src/tests/junction/topology/junction_topology.cpp` unless said otherwise.
1. **"programs: a single-bridge band crossing is Corner, Under, Corner"** `[junction][topology]`: on T, LL_RR x L_R and L_R x LL_RR, every Under/Above token of a single bridge in a band edge's program has a Corner before it and a Corner after it with only Under/Above tokens between, and no SBend with role None remains in any band program. Red: today the token before is an SBend.
2. **"programs: consecutive same-hand crossings share one run"** `[junction][topology]`: on LL_RR x LL_RR (pairing any) the one edge with consecutive same-hand single crossings (D258: 1) has `Corner, Under/Above, Under/Above, Corner`. Red today.
3. **"placement: a single bridge crosses its band edge's run"** `[junction][topology]`: at the T and LL_RR x L_R seeds (`MakeJunctionGraphSeed`), for each single bridge over a band edge: its column strictly inside the crossed edge's run and the run's row strictly between its heads; `CheckJunctionRules` clean. Red today: no run exists (the crossing is on an S-bend).
4. **"joint: LL_RR x LL_RR gets past 'no crossed sbend'"** in `src/tests/junction/placement/junction_joint.cpp`, `[junction][milp][.sweep]`: round 0 at `JunctionFloorChoices` records (no "no crossed sbend" failure); WARN the joint status and objective. Red: today the recording fails or is infeasible; record which.

## Existing tests
- The two tests whose subject is the over-S-bend family, "joint: a bridge over an S-bend may move along it" and "joint: a bridge may pass over the S-bend after its crossing" (`junction_joint.cpp` ~1208, ~1233), have no S-bend crossing left to test: delete them, and say so in the report.
- Pinned numbers that move because the crossings change form (D235 allows re-pins case by case): the joint objectives at the seeds (T 27300, LL_RR x L_R 86350; `junction_joint.cpp` ~55-98), system digests, `[topologysweep]` cells, identity hashes (`LEVELHASH`). Update each to the new value only after checking the hub's seed is `CheckJunctionRules` clean, and list every changed number (old -> new) in the report and in the commit message body.
- A planned count in a sweep (`HOVERPLAN`, `HOVERNEAR`, `LEVELHASH` planned) that drops is a STOP (report the offsets), not a re-pin.
- Any other failing test: STOP and report it.

## Do not
- Delete the over-S-bend code, `JunctionSBendColumnForm`, `choices.over`/`over_after` or their joint rows (that is 2a2).
- Change the joint model, the loop, the hover or the pipeline.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- A seed that was `CheckJunctionRules` clean is no longer clean, or no longer seeds.
- A planned count drops.
- The chain elements around an Under token are not as described in change 2.
- Anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`: all pass, report the time;
- `tools/junction/jt sweep build-rel`: all pass within its budget;
- `tools/junction/jt identity build-rel`: all pass after the listed re-pins.

Give every `jt` call a Bash timeout of at least 400000 ms.

One commit on wt-2a1: `Feature: Junction band edges pass under single bridges square`, the body listing every re-pinned number, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence per test;
- functions and relation families added;
- re-pins (old -> new), and the seeds' box sizes before and after;
- LL_RR x LL_RR round 0: recording failure (if any), joint status and objective;
- gate, sweep and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
