# Brief G3l (probe on probe-g3e): does any straight-leg choice per fork make LL² feasible?

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch probe-g3e` (at the hash in your launch prompt). Commit only there. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. One heavy job at a time. Each run under `timeout -s KILL 900`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Read first (main worktree `/home/pete/OpenTTD/docs/junction-tool/`): STATUS.md first bullet and DECISION CARD F; `wip/BRIEF-G3j-fork-probe.md` (its step 2 is the mechanism here, with the leg chosen by a mask instead of the slot layer); `<S>/g3j-forks.txt` (the fork dump: the six LL² forks and their legs).

## Why
Card F asks how to choose which leg of a fork runs straight. Before building any chooser, we need to know whether some choice makes LL² feasible at all.

## Change (probe only, `TrackComposer` only)
- `PLANAR_FORKMASK=<n>` (decimal, parsed with a manual digit loop: `std::atoi`/`strtol` are banned): number the fork nodes (a split whose `node_legs` has no straight leg) in the order they are found when walking the nodes in index order. For fork i, bit i picks which leg runs straight: 0 = the left leg (the leg G2 rule 2 gives -1), 1 = the right leg. That leg's working heading list is its base list (undo rule 2 for it: no leading diagonal, its crossing pieces shifted back), exactly as G3j step 2 describes. The other leg keeps rule 2's diagonal. The merges are unchanged.
- The probe test prints one line per fork: index, the node, the in line, the straight leg.

## Measure
1. LL² `o0t1f0s1` via the probe test (`JUNCTION_PROBE_HUB=LL2`, `JUNCTION_PROBE_TAG=o0t1f0s1`, `PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1 PLANAR_SHAPE=default`): for every mask 0 to 2^k-1 (k = the fork count; k = 6 gives 64 runs), record in `<S>/g3l-masks.txt`: the mask, ok or the failure reason, and for ok runs the area, size and ms. Use a shell loop, one run at a time.
2. If any mask solves: for the best mask, describe per fork which leg is straight and whether that leg is on the same side as its later merge's straight leg (this tests card F option 3's rule). Then run that mask on the other 11 tags (`<S>/ll2-12.txt`) through the probe test: ok or reason, area.
3. If no mask solves: for masks 0 and the one whose failure the IIS names fewest groups, `PLANAR_IIS=1`, with the groups classified as in `wip/BRIEF-G3g-binding-probe.md` measure 3.

## Do not
- Add a variant bit to the variant space, an optional piece, or anything outside the probe variable. Change the slot layer, the map, G2's rules for anything but forks, the solve or `MappedComposer`. Touch ports 4077/4079 or start a game client. Use git stash.

## Commit and report
- One commit on probe-g3e: `Probe: Junction fork straight legs from a mask (PLANAR_FORKMASK)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; the fork list; measures 1-3 (summarised: masks ok, best, the rule check); any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
