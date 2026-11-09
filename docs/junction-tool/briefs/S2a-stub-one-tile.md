# S2a: branch stubs one tile (D115)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/s2a`, branch `wt-s2a`, head given in your launch prompt. You are the only writer there. Other agents work in sibling worktrees under `.claude/`; never touch them or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1` (ccache is shared, so it is fast after the first build). Debug only if the gate asks.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8` (8 jobs: other agents share the machine). Iterate with your own tag; full `"~[.]"` only at the gate.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (read the rows named below; it is newer in the main worktree than in yours).
- Plan: `docs/junction-tool/PLAN-S2.md` §4 S2a. Decisions D115, D58, D90(4).

## Why (settled)
D115: one plain tile between a branch's edge-most split or merge and the frame edge. Today the test spec's layout (rendered by the test "GenerateJunction: the W side is tight" with `-s`) has 2 plain tiles below the S merge and 3 below the N split. In `src/junction_generator.cpp` (~245) `STUB = 3`, `ys = yb - STUB`, the merge/split goals are the stub pieces on rows `ys+1 .. yb-1`. With `STUB = 2` the only stub row is `yb-1`, so the W->S merge and the N->W split sit on `yb-1` and the edge row `yb` is the one plain tile.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (naming, comments).
- `src/junction_generator.cpp` 200-300 (frame, stub, fences) and the height sweep in `GenerateJunctionUnchecked` and `BuildJunctionLibrary`.
- `src/tests/junction_generator.cpp`: the test above and its helpers.

## Test (red first, `[junction][generator]`)
"GenerateJunction: branch stubs are one tile (D115)": generate `MakeTestJunctionSpec()` on flat ground. Find the two columns that have track on the layout's bottom row (the N and S stubs). For each, count tiles from the bottom row upwards whose tracks are exactly `TrackBits{Track::Y}`, stopping at the first that is not; CHECK the count is 1 for both. Red today: 2 and 3 (record the values).

## Changes
1. `constexpr int STUB = 2;` with its comment rewritten: the stub is the branch's entry piece row plus the one plain edge tile below the edge-most merge or split (D115).
2. Nothing else in the generator. Fences relative to `ys` stay as they are. Do not change the height range.
3. Other tests whose pinned values change because the layout is one or two rows shorter (sizes, heights, tile counts, golden layouts): update the expected values only, and list every changed expectation (test name, old, new) in the report. Do not weaken or delete any assertion.

## Do not
- Change the router, checker, signal pass, sweep parameters or the height range.

## Stop conditions
- With `STUB = 2` the test spec no longer generates, or the checker rejects the stubs (report the violation).
- The count is not 1 for one of the stubs after the change.
- A test fails for a reason other than a pinned size/height/count value.

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "~[.]" 8`.
3. Commit: `Change: Junction branch stubs are one tile`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test (the failure message seen before the code existed)
- Types, functions, members, constants added or removed: names only
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
