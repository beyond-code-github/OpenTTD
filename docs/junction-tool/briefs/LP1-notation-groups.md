# LP1: line notation with direction groups (D142)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/lp1`, branch `wt-lp1`, head given in your launch prompt. You are the only writer there. Other agents work in sibling worktrees under `.claude/`; never touch them or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1` (ccache is shared, so it is fast after the first build). Debug only if the gate asks.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8` (8 jobs: other agents share the machine). Iterate with your own tag; full `"~[.]"` only at the gate.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (read the rows named below; it is newer in the main worktree than in yours).
- Plan: `docs/junction-tool/RECON-linepainter.md` LP1 (line 81). Decisions D142 (the ruling), D135b, D126, D143 (not this step: recorded so you don't act on it).

## Why (settled)
D142: in LL_RR the tracks of one direction are adjacent (gap 0; on diagonals S = 1, OpenTTD's doubled track); the underscores are the only gap, between the left and right groups. Today `JunctionLineSpec {tracks, gap}` spaces every neighbour by `gap`, and `TurnStrings`/the start offsets use `i * RightVector(gap, ...)`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- `src/junction_line.{h,cpp}` (all; 246 lines), `src/tests/junction_line.cpp` (all; 194 lines).
- `src/junction_generator.cpp` 30-66 (`ParseJunctionNotation`) and `JunctionNotationError` in `src/junction_generator.h`.

## Tests (red first, `[junction][line]`)
1. "ParseLineNotation reads direction groups": `"L_R"`→{tracks 2, gap 1, left_tracks 1}; `"LR"`→{2,0,1}; `"LL_RR"`→{4,1,2}; `"LLL__RR"`→{5,2,3}; `"L___R"`→{2,3,1}; and error `Invalid` for `"L____R"` (gap above 3, the largest LP2 supports), `"R_L"`, `"L_"`, `"_R"`, `""`, `"L_L_R"`, `"LxR"`. Red: a stub returning `Invalid`.
2. "JunctionLine: LL_RR groups are adjacent": `RasterizeJunctionLine({4, 1, 2}, {{20, 20}, Trackdir::X_NE, "FFFF"})`: at every piece index, track offsets across the line are 0, 1, 3, 4 tiles from track 0 (adjacent within a group, gap 1 between groups). And on a diagonal start (pick one diagonal trackdir that the existing "straight in all directions" test uses), the diagonal counts from track 0 are 1, 4, 5 (S = 1 within a group, 3 between, cumulative), checked the way the existing diagonal check does with `DIAGONAL_SPACING`. Red: today's uniform spacing gives 0, 2, 4, 6.
3. "JunctionLine: every start and turn combination" additionally runs its programs for `{4, 1, 2}` and `{3, 1, 1}` (L_RR).

## Changes
1. `JunctionLineSpec` gains `uint8_t left_tracks = 1; ///< Tracks in the left group (D142); the gap is only between track left_tracks - 1 and left_tracks, other neighbours are adjacent.` Update the struct comment. Existing `{2, gap}` initialisers stay valid.
2. In `junction_line.cpp`, replace every `i * RightVector(gap, heading)` use (start offsets and `TurnStrings`) with a file-local `static LineVec TrackOffset(const JunctionLineSpec &spec, int i, Direction heading)` = the sum over neighbours j < i of `RightVector(j == spec.left_tracks - 1 ? spec.gap : 0, heading)`. Update the file's geometry comment (the "Further tracks add the same difference again" sentence) to say each track's offset is the sum of its neighbours' spacings.
3. `struct JunctionLineNotation { JunctionLineSpec spec; JunctionNotationError error = JunctionNotationError::None; };` and `JunctionLineNotation ParseLineNotation(std::string_view text);` in `junction_line.{h,cpp}`, with doc comments citing D142. Grammar: one or more `L`, zero to three `_`, one or more `R`, nothing else; `tracks = L + R`, `left_tracks = L`, `gap = underscores`. Everything else is `Invalid`. Leave `ParseJunctionNotation` unchanged.
4. Existing tests that build lines with more than two tracks (`{3, 1}`, `{3, 0}`) now mean L_RR (`left_tracks` 1): update only assertions that pinned the uniform spacing of track 2, and list each changed assertion in the report.

## Do not
- Touch the junction tool, its notation parser, the router, or D143 (narrowing).
- Add bridges, a router, GUI or settings.

## Stop conditions
- A turn program that passed for `{3, 1}` before has no integer solution with the grouped offsets (report the program, start and spec).
- Any change outside `junction_line.{h,cpp}` and `src/tests/junction_line.cpp` seems needed.

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction][line]" 8`, then `ptest.sh build-rel/openttd_test "~[.]" 8`.
3. Commit: `Add: Line painter notation with direction groups`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test (the failure message seen before the code existed)
- Types, functions, members, constants added or removed: names only
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
