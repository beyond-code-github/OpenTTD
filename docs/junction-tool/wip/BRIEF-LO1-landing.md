# Brief LO1 (landing): lane offsets at least their spacing; side-decided clashes by trial when the batch fails

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch -c wt-lo1 feature/junction-tool` (at 0828fe9fa6 or a docs-only successor). Commit only there; the orchestrator merges. Build dir `build-rel`, target `openttd_test` only.
- Time box: 90 minutes. One heavy job at a time. Tests via `tools/junction/jt` (budgets in `docs/junction-tool/TESTS.md`); sweeps via `tools/junction/pv` (columns: tag area WxH ms track wasted reason).
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
D355 (fixes loosen, never special-case) and the probe evidence: on the probe line, holding each lane's offset from lane 0 EQUAL to its slot value forced the slot layer's spread onto the geometry (e.g. a join's spread carried back along a whole bundle); making it AT LEAST the spacing gave T3 1796 -> 1715 and T2 932 -> 915. The same probe found that the lazy loop's sides round adds every side-decided separation at once and fails the variant when the batch is infeasible, with no fallback. Both are now landed on the main line, unconditionally (no environment switch).

## Reference code (read with `git show`, do not cherry-pick)
- `git show bc1d3427f4 -- src/junction/planar/planar_components.cpp`: the `o` letter only (`MinGap`, its use in `Bundle` and `Flex`). Ignore everything behind the `s` letter and the `PLANAR_LATE` plumbing.
- `git show 321977a8fc -- src/junction/planar/planar_components.cpp`: the `PLANAR_SIDETRIAL` block only. Ignore `PLANAR_SIDEANY` and the `unseparable` print.
- Read first: `CODINGSTYLE.md`; on wt-lo1 `planar_components.cpp` `Offs`, `Bundle`, `Flex(lanes, h, offs, jog)`, `build_core` (`run offset` stays), the lazy loop's sides round (`decided by sides`); `src/junction/planar/CLAUDE.md` "Slot layer" and "Line sides".

## Changes (planar_components.cpp, unconditional)
1. A composer member function `MinGap(c, ivs)` as in bc1d3427f4: per lane j >= 1, 2 x (position of lane j minus position of lane j-1 in the interval's `tracks` list, copies counted), the largest over the intervals; it fails the model (`M.Fail`) with a clear text when lane positions do not increase with j.
2. `Bundle`: both `bundle offset` rows per lane become `Ge(Dot(r, seg_j) - Dot(r, seg_{j-1}), min_j)` (lead: its `ivs`; tail: the tail interval, or `ivs` when the piece is not doubled).
3. `Flex(lanes, h, offs, jog)`: its offset rows the same way, with the minimum over the core's lead intervals passed in as a parameter.
4. `run offset` stays `Eq` (it fixes a prio join's entry gaps, a coop component shape).
5. The sides round: when the batch of side-decided separations is infeasible, try each side-decided clash on its own over the directions its side allows, in `SepOptions` order, keeping the first that stays feasible (cumulative); if none is kept, fail as today ("infeasible with topology sides").
- `src/junction/planar/CLAUDE.md`, same commit: in "Slot layer", the composer reads each lane's offset from the previous lane as at least its spacing in the interval's track list (copies counted), not equal to the slot difference, except the run into a join (`run offset`, equal); in "Line sides", the sides round's fallback in one sentence. Remove any sentence these supersede.

## Measure (red/green evidence for this landing is the sweep comparison)
1. BEFORE the change, on wt-lo1: `pv run build-rel T3 --out <S>/lo1-pv-T3-before.txt` and the same for T2 and LL2 (LL2 may take long: `timeout -s KILL 900`; if it exceeds that, record "timed out" and continue).
2. After: the same three into `...-after.txt`.
3. A per-variant side-by-side table per hub (area, track, wasted, ok/reason) and: variants built before/after, best variant and its area/track/wasted before/after, max ms.
4. `jt` fast planar tags and the T2/T3 mapped tests: counts and times.
Stop condition: if any variant that built before fails after, do NOT stop; finish the measurements and list those variants with their failure reasons first in the report.

## Do not
- Touch the slot layer, paths, `Fray`, `Join`, `Double`, the signal pass or the checker. Add `PLANAR_*` switches. Touch ports 4077/4079 or start a game client. Use git stash.

## Commit and report
- One commit: `Change: Junction lane offsets are at least their spacing; side-decided clashes by trial`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; members added; the tables of 3; lost variants first; gate counts and times; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks`. When you have reported, stop.
