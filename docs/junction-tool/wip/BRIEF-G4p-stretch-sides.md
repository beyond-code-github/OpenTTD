# Brief G4p (probe on probe-g3e): the lazy clash loop takes sides per stretch from the track map

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch probe-g3e` (at 806c6c3b78). Commit only there. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 120 minutes. One heavy job at a time. Each run under `timeout -s KILL 900`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Read first (main worktree, `/home/pete/OpenTTD/docs/junction-tool/`): `wip/BRIEF-G3f-size-probe.md` and `wip/BRIEF-G3e-slotbend-probe.md` (what probe-g3e holds); DESIGN-tracks §16.7 "Separation" and §16.8 step G4.

## Why
With `PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1`, T2 o1t3f1s0 solves at 2621 (96x65) against `MappedComposer`'s 862 (94x29) (`<S>/g3f-T2-tracks.txt`): the E main line bows 17 rows north, the S branch's lines run 40 rows; dropping curve rows changes little. The lazy loop decides a clash's separation side from whole topology lines only (`SolveLazy1`, `sides->Side(lp, lq)`), so the track composer's copies, rows and stretches fall back to trial order. §16.7: the lazy loop's sides come per stretch from the map (`PlanarTrackMap::Side(p, ps, q, qs)`), every strand segment mapped to its track and stretch.

## Read first (code)
- `src/junction/planar/planar_components.cpp`: `SegKey`, `SolveLazy1` (the three uses of `sides->Side`), `SolveLazy`, `SolveComposedVariant` (how it passes `&composer.sides`), `TrackComposer` (how each event is routed to a segment, the doubled-piece parts, S-bend cuts, copy strands).
- `src/junction/planar/planar_regions.h`: `PlanarTrackMap` (`tracks`, `part_of`, `expanded`, `events`, `Side`).

## Changes
1. **Segment sides in the lazy loop.** `SolveLazy1` and `SolveLazy` take one more parameter, `const std::function<int8_t(const SegKey &, const SegKey &)> *seg_side` (doc: side of p's segment relative to q's segment's travel, +1 right, 0 unknown). In `SolveLazy1` a local `side_of(clash)`: `seg_side` given and non-zero for `(clash.first, clash.second)` → it; else today's line rule (`sides`, `LineOfStrand`, `Side(lp, lq)`, with today's exclusions) → it; else 0. The three places that use `sides->Side(lp, lq)` use `side_of(clash)` instead (a clash is side-decided exactly when `side_of` is non-zero). Every other caller passes `nullptr`.
2. **Both composers** get a public member `std::function<int8_t(const SegKey &, const SegKey &)> seg_side; ///< Side of one segment relative to another from the track map; empty when not known.` `MappedComposer` leaves it empty. `SolveComposedVariant` passes `composer.seg_side ? &composer.seg_side : nullptr` (and `next->...` likewise in the ladder passes).
3. **`TrackComposer` fills it** (always, not behind a variable) from candidates per segment:
   - **Parts of a line L:** the map tracks t with `part_of[t] == L`, plus track L itself when it has events or ends of its own in `expanded` (find out which on T2 o1t3f1s0 and report it), chained in travel order by their `expanded` ends (each part's to node holds the next part in its `out`). Cumulative event counts: part k covers line events `c_k .. c_k + n_k - 1`, `n_k = map.events[part].size()`.
   - **Positions:** position i of L is the gap after its event i - 1 and before event i (0 .. its event count). Position i maps to every part k with `c_k <= i <= c_k + n_k`, as `(part k, stretch i - c_k)`.
   - **A line segment** spans positions `i0 .. i1` (`i0` = L's events routed to earlier segments of the strand, `i1 = i0 +` its own events); its candidates are the union over those positions.
   - **A copy strand's segment** (strand named after its map track): candidates `(copy track, s)` for the positions of its own events the same way (its first segment, the step, spans position 0 only; `d1` spans 0 .. its event count).
   - **A row strand** (`prio:<line>`, the map row track of that name if any): one segment, candidates `(row track, 0 .. its event count)`.
   - Any other strand or segment: no candidates.
   - `seg_side(p, q)`: over every candidate pair, `map.Side(tp, sp, tq, sq)`; the common sign when all non-zero values agree and at least one is non-zero, else 0.
4. Probe test hook (declared in `planar_components.h` beside `PlanarTrackComposerStrands`, doc comment): `int PlanarTrackComposerSideCount(const PlanarTopology &topology, const JunctionSpec &spec, const std::string &tag, int &segments_without);` builds the `TrackComposer` as `PlanarTrackComposerStrands` does and returns the number of ordered pairs of segments of different strands with a non-zero `seg_side`; `segments_without` receives the number of segments of topology-line, copy and row strands with no candidates.

## Tests
1. `"planar track composer: T2 o1t3f1s0 segment sides from the map"` (tags as the other track composer tests): `PlanarTrackComposerSideCount` > 0 and `segments_without == 0`. Red first: write the hook returning 0 before filling `seg_side` (record the assertion).
2. The strands test stays green.

## Measure
1. The probe test (`PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1`, `JUNCTION_PROBE_ASCII=<S>/g4p-T2-tracks.txt`) into `<S>/g4p-probe.txt`: the slotbend line (area, size, track, wasted, empty, ms). With `PLANAR_TIMES=1`, the rounds' "decided by sides / by trial" counts into `<S>/g4p-times.txt` (summarise: total decided by sides, total by trial).
2. `pv` T2 and T3 with both variables (`<S>/g4p-pv-T2.txt`, `-T3.txt`): ok / failed by reason, best by area (area, size, track, wasted, empty, ms), largest ms; beside `<S>/g3e-pv-T2.txt` and `<S>/g3a-after-T2.txt` (and T3).
3. Identity: `pv` T2 without `PLANAR_COMPOSER` against `<S>/g3a-after-T2.txt`: no column but ms may change.
4. Look at the new ASCII and say in at most 6 lines where the empty area now is.

## Do not
- Change the separation rows themselves, `SepOptions`, the order of the fallback, the composer's rows, the map or the slot bends. Touch ports 4077/4079 or start a game client. Use git stash.

## Stop conditions
- The identity check changes any column but ms: stop and report the variants.
- Track L's role (part or not) is not decidable from `expanded` on T2: report what you see and stop before step 3's code.

## Commit and report
- One commit on probe-g3e: `Probe: Junction lazy loop sides per stretch from the track map`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; red evidence; what track L is in the map; measures 1-4; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
