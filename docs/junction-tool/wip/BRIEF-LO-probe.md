# Brief LO (probe): lane offsets at least, not equal; slot S-bends free length

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, on `wt-tracks`. Run `git switch -c probe-lo probe-et` (probe-et is at 4fb697b41f: probe-fl2 plus box caps, `PLANAR_LATE` letters r, c, g, f exist); commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. Single-variant runs `timeout -s KILL 30`; pv `timeout -s KILL 600`; end to end `timeout -s KILL 60`. One run at a time. No `PLANAR_IIS`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
The engineer: every fix must make the composer less strict, never add a case (D355). Today every lane's offset from lane 0 inside a bundle or core is held EQUAL to its slot value (`bundle offset`, `flex`, `run offset` rows), so the slot layer's spread (for example the D bundle carrying its join spread, D1-D3 6 tiles, back to its port) is forced on the geometry, and each slot change is an S-bend of exactly 2|delta| that is refused on a diagonal. The probe replaces the equalities by a minimum spacing and frees the S-bend lengths; the lazy clash loop and the rungs still keep tracks legal, and port spacing stays fixed by its own `port order` rows.

## Read first (probe-et, `src/junction/planar/planar_components.cpp`)
- `Offs`, `SlotOf`, `IvName`, `SlotBend` (~850-910); `Flex` (the `std::vector<Seg> Flex(lanes, h, offs, jog)` overload, ~1103); `Bundle` (~1130-1195, the `bundle offset` rows and the doubled piece's step back `M.AddSeg(l, Turn(h, -sg), 2, 2)`); `build_core` (~1363-1470: `SlotBend` calls, the step back after `Double`, `run offset`); the `PLANAR_LATE` / `LateFlag` helper from the FL probe.
- `src/junction/planar/CLAUDE.md`, "Slot layer".

## Changes (behind new letters of `PLANAR_LATE`)
- **`o` (offsets at least):** define the minimum spacing of consecutive lanes j-1, j of a channel over a list of intervals as 2 x (the position of lane j minus the position of lane j-1 in the interval's `tracks` list, left to right, copies counted), the largest over the intervals. Then, with `o`:
  1. `Bundle`: each `bundle offset` row `Eq(Dot(r, seg_j) - Dot(r, seg_0), 2 * off_j)` (both: the lead's `a`, the tail's `b`) becomes `Ge(Dot(r, seg_j) - Dot(r, seg_{j-1}), min_j)` with the minimum over the intervals that offset was read from (`ivs` for the lead, the tail interval for the tail).
  2. `Flex`: its two offset rows per lane the same way (minimum over the core's `lead_ivs`). Pass what `Flex` needs as a parameter; no member.
  3. `run offset` stays EQUAL (it feeds the join's entry gaps, a fixed coop shape).
  If lane positions in a `tracks` list are not increasing with j (lane order left to right), stop and report the channel and interval.
- **`s` (S-bends free):** with `s`:
  1. `SlotBend`'s diagonal has lower bound 0 and no upper bound (direction unchanged); on a diagonal heading it emits nothing and returns true instead of failing.
  2. The step back after a doubled crossing (`M.AddSeg(l, Turn(h, -sg), 2, 2)` in `Bundle`, and the same in `build_core` after `Double`) has lower bound 0 and no upper bound.
  The copy's own step (`Double`'s two `Turn(h, sg), 2, 2` segments and `Bundle`'s pair) stays exact.
If a change needs a new type or member, or a change outside these functions, stop and report.

## Measure (T3 test `"planar components: T3 from its topology text (mapped) at the hand composition's area"`, T2 test `"planar components: T2 from its topology text (mapped) at the probe's area"`)
1. `PLANAR_ONLY=o1t3f1s0 PLANAR_TIMES=1 PLANAR_LATE=<x>`, x = (unset), `o`, `os`, `rgos`, `rfgos`, on T3 and on T2: area, size, track, ms or failure with reason (baselines: T3 unset 1796.25, `rfg` 1692.25; T2 unset 932.25).
2. `pv run build-rel T3` and `pv run build-rel T2` with the best x of 1 by T3 area (`--out <S>/lo-pv-<hub>.txt`): built/failed with reasons grouped, best variant and area, max ms; per variant side by side with `<S>/fl2-pv-T3.txt` / `<S>/fl2-pv-T2.txt`.
3. End to end T3 with that x: `PLANAR_LATE=<x> PLANAR_ONLY=o1t3f1s0 PLANAR_LINEDUMP=<S>/lo-lines.txt JUNCTION_COMPONENTS_TOPOLOGY=T3 JUNCTION_COMPONENTS_OUT=<S>/lo-T3.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/lo-e2e-T3.txt 2>&1`: plan result, `MAPPED`, `VIOLATION` lines grouped by rule; E1-3 and D1-3 heading runs (compressed, `<S>/runs.sh`) from the dump.
4. The same end to end for T2 (`JUNCTION_COMPONENTS_TOPOLOGY=T2`, outputs `lo-T2.json`, `lo-lines-T2.txt`, `lo-e2e-T2.txt`), without `PLANAR_ONLY`: best variant, area, `VIOLATION` lines grouped.

## Do not
- Change the slot layer, paths, signal pass, checker, `Fray`, `Join`, `Double`, `run offset`, or any composer rule other than the above. Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- Commit on probe-lo: `Probe: Junction lane offsets at least and free slot S-bends`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Reply with the measurements, any stop, the commit hash, `git status --porcelain`. When you have reported, stop.
