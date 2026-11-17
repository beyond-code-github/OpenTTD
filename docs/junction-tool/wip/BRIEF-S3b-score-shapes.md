# S3b: score both shapes per variant (D341)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Code read with offset/limit. Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/S3b/`.

## State
Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}` (S3a: `PLANAR_SHAPE=program` switch in `SolveMappedVariant`). Read DECISIONS D341, DESIGN-tracks §12, `src/junction/planar/CLAUDE.md`, `SolveMappedVariant` and `SolvePlanarMapped` in `planar_components.cpp`, `planar_shape.h`, `src/tests/junction/planar/planar_components.cpp`.

## Test first
- New `[junction][planarcomponents][slow]` test: `planar components: T2 takes the shape program's shape where it is smaller`: `SolvePlanarMapped` on T2 gives `layout.area <= 862.25 + 0.01`. Red today (906.25). Record the failure.

## Change
- `SolveMappedVariant` with no shape given: env `PLANAR_SHAPE` unset: solve with the default shape; then `SolvePlanarShape` for the same channels and over; if it succeeds and its paths differ from the default's, solve again with them (fresh composer; the whole solve incl. ladder passes); keep the outcome with the smaller area (a failed solve never wins; tie: default). `=default`: default only. `=program`: as S3a. The outcome gains `bool program_shape` (which won), and `PlanarComponentLayout` gains `bool program_shape = false;` ("the winning variant used the shape program's shape") copied for the winner.
- CLAUDE.md: replace the S3a switch line with the D341 behaviour and the two forcing values. No D-refs.

## Gates
Build; `jt` gate (budget 30 s: report time); the new test and the T2/T3 mapped [slow] tests. pv on T2, T3, LL² (201 tags in `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt`), `-j 6`, before vs after: expected best T2 862.25, T3 1933.25, LL² 1654.25; no variant's area may get worse and none that solved may fail (stop before committing otherwise); median/max ms before/after.
Commit: `Change: Junction variants score the default and the shape program's shape` + body + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Commit; red evidence; suites and gate time; pv table (solved, best area, better/same/worse, median/max ms); stops; `git status --porcelain`, `git log -1`. Under 15 lines. When you have reported, stop.
