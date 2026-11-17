# S3a: solve with the program's shape (switch, measured)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Code read with offset/limit. Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/S3a/`.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}` (S2 landed: `SolvePlanarShape`, `PlanarShapeTurns` in planar_shape; S1: `BuildPlanarPaths(..., shape)`, `SolvePlanarMappedVariant`). Read DECISIONS D340, DESIGN-tracks §12, `src/junction/planar/CLAUDE.md`, `planar_shape.h`, and in `planar_components.cpp` `SolveMappedVariant`, `SolvePlanarMapped`, `PlanarDefaultShape`.

## Changes
1. `SolveMappedVariant`: when env `PLANAR_SHAPE=program` and no shape was given, compute `SolvePlanarShape(t, channels, over, ...)` for the variant (same channels and over as the default) and solve with it; if the program fails, solve with the default and say so in a stderr line under `PLANAR_TIMES`. Default (unset): unchanged. One CLAUDE.md line for the switch.
2. Tests tagging: `src/tests/junction/planar/planar_shape.cpp`: tag the T2 and T3 tests `[junction][planarshape]` so the gate runs them; the LL² part of the loops moves into its own test case tagged `[junction][planarshape][slow]`. If the gate then exceeds its 30 s budget, report the time and do not commit the tagging change (commit the rest).

## Measure (the point of this step)
pv on T2, T3 and LL² (201 tags in `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt`), `-j 6`, default vs `PLANAR_SHAPE=program`. Per hub: variants solved each way (and newly failing / newly solving with their reasons), best area each way, per-variant area better/same/worse with the 5 largest deltas each way, median/max ms, and for the best variant of each way its default vs program turns (`PlanarShapeTurns`).

## Gates
Build; `jt` gate; default pv must be identical to before (unset switch). Commit: `Change: Junction switch to solve with the shape program's shape` + body (with the measured table) + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Commit; suites and gate time; the measure table; stops; `git status --porcelain`, `git log -1`. Under 20 lines. When you have reported, stop.
