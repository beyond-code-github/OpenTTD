# G3 R4a: the seed falls back to the joint loop when no cut places (DESIGN-G3 §G3.6 R4, D250)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
`MakeJunctionGraphSeed` (src/junction/junction_pipeline.cpp) places through `CutJunctionTrackBridges`: per bridge cut, the fixed path's rounds. Big hubs (L_R x L_RR, LL_RR x LL_RR, ...) fail there. The joint loop (`SolveJunctionJointLoop`, src/junction/placement/junction_joint.cpp) can find lengths the rounds cannot.

D250 (orchestrator default): when no cut places, the seed runs the joint loop on the default cut (the first cut the search tries), from all-floor choices. If the loop settles, and `PlaceJunctionTrackGraph` at its choices places, that placement and cut are the seed's. Otherwise the default cut's error is reported, as today.

The loop runs only after every cut has failed, so every seed that places today is unchanged, and identity does not change.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes2`. First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-r4a <HEAD given at launch>`. Rebuild `openttd_test` in `build` and `build-rel` (delete stale `.pch` files if the build complains about them). Leave untracked files alone.
- Another agent works in `.claude/nodes` on the joint model's clash rows (`junction_joint.cpp`, "The clashes:"). Never touch that worktree. Keep your `junction_joint.cpp` edit to the new function below, placed after `SolveJunctionJointLoop`.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/r4a/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `src/junction/junction_pipeline.cpp`: `CutJunctionTrackBridges` and `MakeJunctionGraphSeed`;
- `src/junction/placement/junction_joint.h/.cpp`: `SolveJunctionJointLoop` and `JunctionJointLoopResult`;
- `src/tests/junction/placement/junction_joint.cpp`: how the loop tests build the all-floor round-0 choices (grep "All floors"), and `DefaultCut`;
- `src/junction/CLAUDE.md` ("Pipeline") and the test "topology: LL_RR x LL_RR under each lane pairing seeds or names a reason" (src/tests/junction/topology/junction_topology.cpp).

## Changes
1. **All-floor choices as a function** (`junction_joint.h/.cpp`):
   ```cpp
   /**
    * The joint loop's first choices when no placement exists (DESIGN-G3 §G3.6, R4): every free S-bend 1, every S fan 0, every joiner Corner k, read from a recording of the rounds.
    * @return The choices, or empty choices (no m) when the rounds recorded no bends.
    */
   JunctionPlacementChoices JunctionFloorChoices(const JunctionTopology &topology, const std::vector<JunctionLanePath> &paths, const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackStructure &structure, const JunctionTrackPrograms &programs, const JunctionSpec &spec);
   ```
   - Its body is the test's existing all-floor block, moved: the rounds-mode `PlaceTrackGraph` call with a system, then the floors from `system.tokens`, `system.bends`, `system.k` and `system.port_offset`.
   - `PlaceTrackGraph` is file-local to `junction_placement.cpp`. If it is not reachable from `junction_joint.cpp`, use the public entry point that records the rounds; if there is none, STOP and report.
   - The tests that build all-floor choices call the function instead of their own block. Their outcomes must not change.
2. **The fallback** (`CutJunctionTrackBridges`). Keep the default cut's structure, which is the first `cut` tried. At the point where today the function reports `default_error`, before reporting:
   - `choices = JunctionFloorChoices(..., default_cut, ...)`. If it is empty, report as today;
   - `loop = SolveJunctionJointLoop(..., default_cut, ..., choices)`;
   - if `loop.outcome == "settled"`: `PlaceJunctionTrackGraph(..., default_cut, ..., loop.choices, &why)`. If that places, return it, with `default_cut`, and clear the error;
   - otherwise keep `default_error`, and append "; the joint loop: " + `loop.outcome` (or + `why` when the loop settled but the placement failed).
3. Update `src/junction/CLAUDE.md`'s pipeline line about the bridge-cut search: when no cut places, the seed runs the joint loop on the default cut from all-floor lengths and takes its placement if the loop settles and the placement at its lengths succeeds; otherwise it reports the default cut's failure with the loop's outcome. Standalone, without D-ids or references.

## Tests
1. **"topology: a seed the cuts cannot place comes from the joint loop"** (`src/tests/junction/topology/junction_topology.cpp`, `[junction][topologysweep][.sweep]`).
   - For L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR (pairing any), each built as the existing tests build them, call `MakeJunctionGraphSeed`.
   - CHECK that it either seeds (routes non-empty) or its error contains "the joint loop:".
   - WARN each hub's result and its time in ms.
   - Red: today no error contains "the joint loop:". Record it.
   - If a hub seeds, also CHECK that `CheckJunctionRules` on its render passes, the way the existing seed tests check it (copy one). If it does not pass, STOP and report the hub and the violations; that would be a real finding.
2. Every existing test stays as it is. Identity must not change.

## Do not
- Change the order of the cut search, its cap, or what it accepts.
- Change `SolveJunctionJointLoop`, the joint model, or the fixed path.
- Touch the window, the hover, or the seed cache.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- Identity or an existing test changes.
- A seeded hub fails the rule checker.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt sweep build-rel` (budget 150 s; if the new test pushes it over, report per-case times and STOP);
- `tools/junction/jt identity build-rel`. If a gate is over budget, re-run it once and report both times. Give every `jt` call a Bash timeout of at least 300000 ms.

One commit on wt-r4a: `Feature: Junction seed falls back to the joint loop`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- functions added;
- per hub: seeded or the error, and the time;
- gate, sweep and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
