# TC1: junction test cleanup and the [junctionmap] budget

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
Worktree `/home/pete/OpenTTD/.claude/tc1`, branch `wt-tc1`, head given at launch. Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache, as their CMakeCache shows); build `openttd_test` only. Tests only through `tools/junction/jt` with bracketed tags (`docs/junction-tool/TESTS.md`).

## Changes (tests and TESTS.md only)
1. Remove leftover diagnostic prints from `src/tests/junction_map.cpp` (and any other junction test file): every `std::cout` line whose text starts with one of `PORTEDGE`, `LANESLOPE`, `LANEHOUSE`, `BRIDGEHOUSE`, `HUBBLOCKED`, `HILL`, `TUNNEL`, `FIELD`. Keep the `BENCH ...` prints (they are the benchmark output). Keep every assertion. If a print is the only use of a variable, remove the variable too.
2. `[junctionmap]` runs 4.94 s in Debug against its 5 s budget. Measure each of its tests' durations with jt's slow-test lines (or `--durations yes` in its log). Move the tests that construct a T map with a terrain or check line-router details (golden trace, flat identity stay in `[junctionmap]`) to a new tag `[junctionmapedges]`, replacing `[junctionmap]` in their tag lists, so that both tags run under 4 s in Debug. Report which tests moved. Add `[junctionmapedges]` to TESTS.md with its Debug and Release times; update `[junctionmap]`'s row.

## Do not
No change to any non-test source, to any assertion, or to test names. No `pkill`, broad `kill`, bare `git stash`.

## Gates
0 warnings; `jt fast build '[junctionmap]' 5`, `jt fast build '[junctionmapedges]' 5`, `jt fast build-rel '[projection]' 5`, `jt fast build-rel '[projectionedges]' 5`, `jt gate build-rel` pass. One commit: `Codechange: Junction tests drop diagnostic prints, split [junctionmap]`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
Commit; prints removed (count per prefix); tests moved; jt lines; `git status --porcelain`, `git log -1`.

When you have reported, stop.
