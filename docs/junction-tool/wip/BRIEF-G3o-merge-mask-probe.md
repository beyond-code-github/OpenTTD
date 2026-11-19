# Brief G3o (probe on probe-g3e): straight legs at merges too: does a fork + merge choice make LL² feasible?

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch probe-g3e` (at the hash in your launch prompt, G3l's commit). Commit only there. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 120 minutes. One heavy job at a time. Each run under `timeout -s KILL 900`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Read first: `docs/junction-tool/wip/BRIEF-G3l-fork-mask-probe.md` (main worktree) and the G3l commit (`git show <launch hash>`): `PLANAR_FORKMASK`, its fork definition (rule-0 legs `hd.legs[n].first == UINT32_MAX`) and how it restores a leg's base list; `src/junction/planar/planar_paths.cpp` (`BuildPlanarLineHeadings`: rule 2 for forks and the merge diagonal rule); `<S>/g3l-masks.txt` and `<S>/g3l-iis-59.txt`.

## Why
G3l: no fork mask solves LL² o0t1f0s1. The smallest IIS (mask 59) is one fork leg's diagonal plus two forced merge diagonals in series (CBj1, CBj2) against ports pinned 2 apart. A merge whose in-legs have no straight leg gives both in-legs a diagonal, as a fork does to its out-legs. We need to know whether choosing one straight in-leg per merge, together with the fork choice, makes LL² feasible.

## Change (probe only, `TrackComposer` only)
- `PLANAR_MERGEMASK=<n>` (decimal, the same manual digit loop as G3l): number the merge nodes whose in-legs both get a forced merge diagonal from `BuildPlanarLineHeadings` (the merge counterpart of G3l's fork definition), in node index order. For merge j, bit j picks the in-leg that runs straight (0 = `in[0]`, 1 = `in[1]`). That leg's working heading list loses its trailing merge diagonal (its base list's end restored), mirroring G3l's restore at the leg's end instead of its start. The other in-leg keeps its diagonal.
- If a crossing sits on the removed diagonal, fail the variant with `a crossing sits on the removed merge diagonal of <leg>`, as G3l does. Do not re-place it.
- The probe test prints one line per merge: index, node, both in-legs, out line, the straight leg.

## Measure
1. The merge list for LL² o0t1f0s1 (m = its count). If m > 8, stop and report the list.
2. Enumerate in this order, one run at a time, through the probe test (`JUNCTION_PROBE_HUB=LL2 JUNCTION_PROBE_TAG=o0t1f0s1 PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1 PLANAR_SHAPE=default`), one line per run into `<S>/g3o-masks.txt` (forkmask, mergemask, ok or the reason, and area, size and ms when ok):
   - (a) `PLANAR_FORKMASK=63` with every merge mask 0 to 2^m-1;
   - (b) if (a) has an ok run: the best merge mask with each of the 8 G3l survivor fork masks (42, 43, 46, 47, 58, 59, 62, 63);
   - (c) if (a) has none: the 7 other survivor fork masks, each with every merge mask, stopping at the first ok run or at the time box.
3. If any run solves: per fork and per merge, which leg is straight, and whether that leg lies on the same side as the straight leg of the node it pairs with (the merge its fork's legs reach, or the reverse). This tests a fixed rule. Then run the best (forkmask, mergemask) pair on the other 11 tags of `<S>/ll2-12.txt`: ok or reason, area. Compare with `<S>/lo1-pv-LL2-after.txt` (MappedComposer best 2162.25).
4. If none solves: `PLANAR_IIS=1` for the run with the fewest IIS groups among those reaching the solve, with the groups classified as in `wip/BRIEF-G3g-binding-probe.md` measure 3.

## Do not
- Add a variant bit to the variant space, an optional piece, or anything outside the probe variable. Change the slot layer, the map, G2's rules other than through the masks, the solve or `MappedComposer`. Touch ports 4077/4079 or start a game client. Use git stash.

## Commit and report
- One commit on probe-g3e: `Probe: Junction merge straight legs from a mask (PLANAR_MERGEMASK)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; the merge list; measures 2-4 (summarised); any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
