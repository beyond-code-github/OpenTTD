# S2a: branch stubs one tile (D115)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/s2a`, branch `wt-s2a`, head given in your launch prompt. You are the only writer there. Other agents work in sibling worktrees under `.claude/`; never touch them or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1` (ccache is shared, so it is fast after the first build). Debug only if the gate asks.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8` (8 jobs: other agents share the machine). Iterate with your own tag; full `"~[.]"` only at the gate.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (read the rows named below; it is newer in the main worktree than in yours).
- Plan: `docs/junction-tool/PLAN-S2.md` §4 S2a. Decisions D115, D58, D90(4).

## Why (settled)
D115 as read in D115a (DECISIONS): below each branch's edge-most split or merge sit exactly two tiles, the signal tile then the boundary tile (D58, G10). Today (test spec, flat) the S stub meets this and the N stub has one tile extra (3 below the N->W split). A first attempt with only `STUB = 2` gave NoLayout: W->S merges into the S stub's side through the router's "exception 2" (`src/junction_router.cpp` ~394-406: a merge piece on a goal tile followed by a goal piece on the next tile), and with `ys = yb - 2` the goal rows `ys+1..yb-1` are the single row `yb-1`, so no merge fits. Diagnosis confirmed: with `STUB = 2` and W->S goals on rows `ys..yb-1` the test spec generates checker-clean, with 2 tiles below both the N split and the S merge.
The failed attempt's patch (test + STUB change) is `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/s2a-stub-attempt.patch`; reuse its test scaffolding but follow the test below, which differs.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (naming, comments).
- `src/junction_generator.cpp` 240-330 (stub, `stub_pieces`, the W->S request ~308, the N->W request ~324).
- `src/tests/junction_generator.cpp`: "GenerateJunction: the W side is tight" and helpers.

## Test (red first, `[junction][generator]`)
"GenerateJunction: branch stubs end one tile beyond the signal (D115, D58)": generate `MakeTestJunctionSpec()` on flat ground. Find the two columns that have track on the layout's bottom row (the N and S stubs). For each, walk upwards from the bottom row while the tile's tracks are exactly `TrackBits{Track::Y}`; CHECK the count is 2 for both, and CHECK the upper of those two tiles carries a signal. Red today: N column 3 (record both values).

## Changes
1. `constexpr int STUB = 2;` with its comment rewritten citing D115/D115a: the entry row, then below the edge-most merge or split the signal tile and the boundary tile.
2. `stub_pieces` takes the first row as a parameter; the W->S goals use rows `ys .. yb-1`; the N->W starts keep `ys+1 .. yb-1`. Comment the asymmetry (the W->S merge needs its goal piece on the next row, router exception 2).
3. Nothing else in the generator. Do not change the height range or the fences.
4. Other tests whose pinned values change because the layout is shorter (sizes, heights, tile counts, golden layouts): update the expected values only, and list every changed expectation (test name, old, new) in the report. Do not weaken or delete any assertion.

## Do not
- Change the router, checker, signal pass, sweep parameters or the height range.

## Stop conditions
- With the changes the test spec no longer generates, or the checker rejects the stubs (report the violation).
- The count is not 2 with a signal for one of the stubs after the change.
- A test fails for a reason other than a pinned size/height/count value.

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "~[.]" 8`.
3. Commit: `Change: Junction branch stubs end one tile beyond the signal`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test (the failure message seen before the code existed)
- Types, functions, members, constants added or removed: names only
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
