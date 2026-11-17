# L1: face map from the variant's channels, faces with runs (DESIGN-tracks §9 L1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Code read with offset/limit. Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/L1/`.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`, Release `build-rel`.
- Read first: `/home/pete/OpenTTD/CODINGSTYLE.md` (if present, else the repo's coding style doc), `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` §6, §9; `src/junction/planar/planar_regions.{h,cpp}` (PlanarMap, FaceEntry, PlanarLineSides ctor ~182); `src/tests/junction/planar/planar_regions.cpp`; `src/junction/planar/planar_paths.h` (`FlipPlanarTriangles`, `PlanarTriangles`); `src/junction/CLAUDE.md` and `src/junction/planar/CLAUDE.md` if present.
- Reference code (a probe, not to be copied wholesale): `/home/pete/OpenTTD/docs/junction-tool/wip/probe-ladders5.patch`: the block starting `/* Probe (flipped map)` (reorders each line's events to the flipped channel order), the `stretch` field on FaceEntry, and the run building in the PlanarLineSides ctor.

## Tests (src/tests/junction/planar/planar_regions.cpp, tag [planarregions]), write first, see red
1. `planar regions: the face map follows the variant's channels`: T2 topology, `ch = BuildPlanarChannels(t)`, `REQUIRE(!PlanarTriangles(ch).empty())`, `fl = FlipPlanarTriangles(ch, 1)`. `PlanarLineSides(t, fl).Why()` empty, and some face of `Faces()` has 4 runs (the flipped T2 has a face walking F B F B: F2 and C2). With the unflipped `ch`, no face of T2 has more than 2 runs. Red before the change: `Faces()` does not exist, so first add `Faces()` and the types (returning runs from the unflipped map), see the 4-run CHECK fail as an assertion, then make the ctor read `channels`.
2. `planar regions: every face is walls in runs`: for T2, T3, LL² and every flip value `f < 2^triangles`: `Why()` empty; every face has at least one run; consecutive runs (cyclically, when more than one) alternate `forward`; every stretch of every line appears in exactly two (face, side) places across all faces plus the outer face's ring (state: every (line, stretch) appears at most twice across `Faces()`; record the counts you see in the report).
3. Existing [planarregions] tests stay green.

## Changes
- `planar_regions.h`: `struct PlanarWallRun { bool forward; std::vector<std::pair<uint32_t, uint32_t>> stretches; };` (forward: the run walks the face with forward darts, the face lies left of the line's travel; stretches as (line, stretch index) in walk order). `struct PlanarFace { int face; bool box; std::vector<PlanarWallRun> runs; };` (box: a ring edge bounds the face). `PlanarLineSides::Faces() const` returning `const std::vector<PlanarFace> &`, every face except the outer one. Doc comments on all, in the file's style.
- `planar_regions.cpp`: FaceEntry gains `stretch` (the edge's stretch, as the probe does). The ctor builds the faces' runs (maximal runs of one dart direction in walk order, ring edges skipped, the last run merged into the first when the directions match and there is more than one run). Before building the map, the ctor takes a copy of the topology whose `events[line]` follow `channels`: for each channel whose crossing order differs from `BuildPlanarChannels(t)`'s, the moved crossings' line-level events on each of the channel's lines are reordered to the channel's order (as the probe block does; if a line's moved events are not contiguous, set `why` and return). Use that copy for everything the ctor reads from events.
- The existing check that the lazy loop's sides come from `PlanarLineSides` stays untouched: the composer already passes the flipped channels, so this changes its sides on flipped variants. That is intended.
- Update `src/junction/planar/CLAUDE.md` (or the folder CLAUDE.md that documents PlanarLineSides) with the fact: the face map follows the variant's channel crossing order and exposes faces as runs of walls. No D-refs.

## Do not
- Do not touch planar_components.cpp, add rungs, copies, or any solver change. Do not add env knobs. Do not change PlanarMap's validation.

## Gates
- `cmake --build build-rel --target openttd_test -j 8 > scratch/build.txt 2>&1`; grep errors/warnings.
- `/home/pete/OpenTTD/tools/junction/jt` for the planar suites (read `/home/pete/OpenTTD/docs/junction-tool/TESTS.md` for the right invocation and budgets).
- Behaviour measure: BEFORE your change (on clean HEAD, build first) run `/home/pete/OpenTTD/tools/junction/pv run build-rel T2 --out scratch/pv-T2-before.txt -j 6`, same for T3, and LL2 with `--tags /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt`. AFTER: same runs to `-after.txt`, then `pv diff` each pair. Report per hub: variants solved before/after, best area before/after, every variant whose area or reason changed (count and the five largest deltas). This is a report, not a gate: commit even if areas move, unless a hub's best area gets worse or a variant that solved now fails, in which case stop before committing and report.
- Commit: `Change: Junction face map follows the variant's channels; faces as wall runs` + blank line + one-paragraph body + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit hash and subject; red evidence (each test, failure message); types/members added; suite results; pv results as above; stops; `git status --porcelain` and `git log -1`. Report under 20 lines. When you have reported, stop.
