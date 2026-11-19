# Brief TM3p (probe): do shape choices move the scorecard? Every crossing angle the shape program allows, solved

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch probe-st` (at 3de02f1d4f: the strip/coverage probes merged with feature/junction-tool, so `PlanarComponentLayout::empty` exists). Commit only on probe-st. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 120 minutes. One heavy job at a time. Each probe run `timeout -s KILL 900`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
D357 (DECISIONS.md): the track-map work is judged as architecture, and compaction is expected from the shape search (TM3, DESIGN-tracks §16.2); this probe checks that early. Today each variant is solved on at most two shapes (the default and `SolvePlanarShape`'s least-turning one). The shape program has a free choice per channel crossing that D354 points at (the engineer: the D lanes "could be diagonal under the bridges"): the angle the under channel crosses at, `a` in 1..3 eighths (`row({{hu, 1}, {ho, -1}, {integer(1, 3), -side}, ...})` in `SolvePlanarShape`). The probe solves every distinct shape those angles give and reports the scorecard (area, track, wasted, empty) per shape.

## Read first
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_shape.{h,cpp}` (`SolvePlanarShape`).
- `src/tests/junction/planar/planar_shape.cpp` `LoopVariants` (how flips and the over choice give channels, `BuildPlanarPaths`, `PlanarLineSides`).
- `src/tests/junction/planar/planar_components.cpp`: how the T2 and T3 mapped tests get their topology and spec; `SolvePlanarMappedVariant` in `planar_components.h`.
- `src/junction/planar/CLAUDE.md`: "Component solver" (variant tag `o{over}t{ties}f{flips}s{steps}`), "Shape program".

## Changes
1. `SolvePlanarShape` gains a last parameter `const std::vector<int> *angles = nullptr` (doc comment: per channel crossing, 0 leaves the angle free, 1..3 fixes it in eighths). When given (its size must equal the crossing count, else fail with a clear text), the `integer(1, 3)` variable of crossing x gets lower and upper bound `angles[x]` when `angles[x] != 0`. With `nullptr` the function is exactly today's.
2. A hidden probe test in `src/tests/junction/planar/planar_components.cpp`, tags `"[junction][planarprobe][.]"`, name `"planar probe: every crossing angle's shape solved"`. It reads `JUNCTION_PROBE_HUB` (`T2` or `T3`, topology and spec as the mapped tests build them) and `JUNCTION_PROBE_TAGS` (comma-separated variant tags). Per tag:
   - parse `o`, `f` (regex `o(\d+)t(\d+)f(\d+)s(\d+)`); `channels = FlipPlanarTriangles(BuildPlanarChannels(topology), f)`; `def = BuildPlanarPaths(topology, channels, o)`; `sides = PlanarLineSides(topology, channels)`;
   - n = `channels.crossings.size()`; if n > 4, print `PROBE <tag> skipped: n crossings` and go on;
   - for every angles vector in {1,2,3}^n: `shape = SolvePlanarShape(topology, channels, sides, def.over, &turns, &why, &angles)`; skip an empty shape; skip a shape whose `headings` and `crossing_piece` equal an earlier one's for this tag;
   - solve the default (`shape` null) and each distinct shape with `SolvePlanarMappedVariant(topology, spec, tag, &shape, layout, &why)`, timing each;
   - print one line each: `PROBE <tag> <angles joined by ',' or 'default'> turns <t> area <a> size <WxH> track <t> wasted <w> empty <e> ms <ms>` or `PROBE <tag> <angles> turns <t> failed <why>`.
   The test asserts nothing beyond the environment being set.
3. Nothing else.

## Measure (tags: the three best of `<S>/et-pv-<hub>.txt` for T2 and for T3)
1. Plain: `JUNCTION_PROBE_HUB=<hub> JUNCTION_PROBE_TAGS=<tags> build-rel/openttd_test "planar probe: every crossing angle's shape solved" > <S>/tm3-<hub>.txt 2>&1`.
2. With `PLANAR_STRIP=1 PLANAR_ROWSIDE=1`: the same into `<S>/tm3s-<hub>.txt`.
3. Per hub and run: per tag the default's scorecard and the best shape by area, by empty and by wasted (angles, turns, area, track, wasted, empty, ms); the number of distinct shapes and of failures; the best over all tags against the default best.

## Do not
- Change the composer, the solve, the default shape or how variants pick shapes. Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- One commit on probe-st: `Probe: Junction every crossing angle's shape solved (planarprobe)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; what was added; the tables of 3; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
