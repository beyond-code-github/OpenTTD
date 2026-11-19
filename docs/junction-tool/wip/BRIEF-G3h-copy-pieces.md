# Brief G3h (probe on probe-g3e): a copy whose crossings lie on several pieces doubles per piece, in series

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, ON `probe-g3e` (verify the head with `git log -1`; it ends with `Probe: Junction track map: one copy per crossed joiner inside a prio join (D322)`), built. Stay on probe-g3e; commit only there. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. One heavy job at a time. Each run under `timeout -s KILL 1800`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Read first (main worktree `/home/pete/OpenTTD/docs/junction-tool/`): STATUS.md's first bullet under "Where things are"; `wip/BRIEF-G3c-copies.md` (the doubled piece definition); DECISIONS D312, D322, D351.

## Why
With `PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1` the track composer solves T2 (best 920) and T3 (best 1929), but every LL² variant fails with `a copy over several pieces: F1"3` (`<S>/orc-d322-pv-LL2.txt`): a full-share line on a turning channel is over several crossings on different pieces. `MappedComposer` doubles a turning channel per crossing piece; D322 allows doubling in series.

## Change (`TrackComposer` only)
Where G3c fails `"a copy over several pieces: "`: instead, the copy's crossings are grouped by the parent piece they sit on (working copy `cpiece`), and each group is built as its own copy on that piece by the G3c definition (same side `sg`, same checks per group: axis piece, crossings consecutive in the parent's events and over), in travel order. The first group's copy strand keeps the map track's name; each later group's strand is named `<parent name>"<the parent's event index of its first crossing>` (so `LineOfStrand` still maps it to the parent). `seg_side` candidates for a later group's strand: the map copy track's candidates (same track, its stretches over that group's events). Nothing else changes.

## Tests
- Extend the strands test only if T2 o1t3f1s0 has such a copy (report whether it does; if not, no test change). The existing tests stay green: `tools/junction/jt fast build-rel '[planar]'`.

## Measure
1. `PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1 tools/junction/pv run build-rel LL2 --tags <S>/ll2-12.txt --out <S>/g3h-pv-LL2.txt`: ok / failed by reason, best by area (area, size, track, wasted, empty, ms), largest ms; beside `<S>/lo1-pv-LL2-after.txt` (MappedComposer, best 2162.25).
2. T2 and T3 the same way into `<S>/g3h-pv-T2.txt`, `-T3.txt`: confirm no variant got worse against `<S>/orc-d322-pv-T2.txt` / `-T3.txt` (list any that did).
3. If LL² still fails on most variants: the reasons grouped, and for the commonest one `PLANAR_IIS=1` on one tag (use the probe test with `JUNCTION_PROBE_HUB` extended to `LL2` = `PlanarLL2Text()` if needed: that extension is allowed) with the IIS groups classified as in `wip/BRIEF-G3g-binding-probe.md` measure 3.

## Do not
- Change the track map, G2's rules, the slot bends, the solve or `MappedComposer`. Touch ports 4077/4079 or start a game client. Use git stash.

## Commit and report
- One commit on probe-g3e: `Probe: Junction track composer doubles a copy per parent piece in series`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; measures 1-3; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
