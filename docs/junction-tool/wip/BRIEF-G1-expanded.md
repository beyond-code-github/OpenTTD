# Brief G1 (landing): the track map exposes its expanded topology

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch -c wt-g1 feature/junction-tool` (at HEAD_HASH). Commit only there; the orchestrator merges it. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 45 minutes. One heavy job at a time. Tests via `tools/junction/jt` with the budgets in `docs/junction-tool/TESTS.md`.

## Why
DESIGN-tracks §16.7-16.8 step G1: the track composer builds from the track map, and needs each track's ends and the nodes between tracks. `BuildPlanarTrackMap` already builds them in a local `PlanarTopology t` (the expanded topology, reordered to track order) and drops it.

## Read first
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_regions.h` (`PlanarTrack`, `PlanarTrackMap`) and `planar_regions.cpp` `BuildPlanarTrackMap` from ~546 to its end (the local `t`, its reordering, `result.events = t.events`).
- `src/junction/planar/planar_topology.h` (`PlanarTopology`, `PlanarLine`, `PlanarEnd`, `PlanarNode`).
- `src/tests/junction/planar/planar_tracks.cpp` (the track map tests and their setup).

## Test (red before the code; record the failure message)
In `src/tests/junction/planar/planar_tracks.cpp`, `"planar track map: the expanded topology holds every track with its ends"` (same tags as `"planar track map: tracks"`), on T2 o1t3f1s0 form A (set up as the existing tests do):
- `map.expanded.lines.size() == map.tracks.size()`, and for every track i `map.expanded.lines[i].name == map.tracks[i].name`;
- `map.expanded.events == map.events` and `map.expanded.crossings.size() == map.crossings.size()`;
- for every track of kind `Copy`: both ends are nodes (`!from.port`, `!to.port`); its from node's `in` holds a track whose `part_of` is the copy's `line`, and its to node's `out` holds such a track;
- for every track of kind `Row`: its from end is a node whose `in` holds a track whose `part_of` is the row's `line`; and its to end is a node.
Expected red: a compile error naming `expanded` (record it), then green.

## Changes
- `planar_regions.h` `PlanarTrackMap`: member `PlanarTopology expanded; ///< The expanded topology: its lines are the tracks (same order), with their ends, the nodes between them (copy and row splits and merges included), crossings and events.` placed after `part_of`.
- `planar_regions.cpp` `BuildPlanarTrackMap`: `result.expanded = t;` right after `result.crossings = t.crossings;`.
- `src/junction/planar/CLAUDE.md`, "Track map": in the first fact's list of what `PlanarTrackMap` holds, add `expanded` (the expanded topology, tracks as its lines with ends and nodes).
- If the test shows a copy or row whose ends are not as stated: stop and report which track and what its ends are (do not change the expansion).

## Do not
- Change the expansion, the map, the sides or any solver code. Touch ports 4077/4079 or start a game client. Use git stash.

## Gates
- The new test red then green; `tools/junction/jt fast build-rel '[planar]'` (count and time).
- One commit: `Add: Junction track map exposes its expanded topology`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit hash; red evidence; members added; gate results; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
