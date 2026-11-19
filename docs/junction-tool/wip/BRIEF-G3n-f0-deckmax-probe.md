# Brief G3n (probe, diagnosis on probe-g3e): the f0 deck-max class: what MappedComposer does that the track composer does not

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch probe-g3e` (at the hash in your launch prompt). Commit only there, and only if you add a probe print. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 75 minutes. One heavy job at a time. Each run under `timeout -s KILL 900`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Read first (main worktree `/home/pete/OpenTTD/docs/junction-tool/`): STATUS.md first bullet (the G3k finding); `wip/BRIEF-G3k-sides-probe.md`; `wip/BRIEF-G3c-copies.md` (the doubled piece and `PlanarPairDecks`); DECISIONS D312, D322, D351.

## Why
G3k: on T2 o1t0f0s0 and T3 o0t1f0s1, o0t0f0s0 and o1t0f0s1 (f0 = unflipped), the track composer fails because B's doubled deck pair has to span both D lanes and both F lanes under deck max (24), which squeezes each lane pair collinear. MappedComposer solves T2 o1t0f0s0 and some of these T3 variants (`<S>/g3a-after-T2.txt`, `-T3.txt`).

## Measure (read, run, compare: no composer change)
1. T2 o1t0f0s0, via the probe test (`JUNCTION_PROBE_HUB=T2`, `JUNCTION_PROBE_TAG=o1t0f0s0`, `PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1 PLANAR_SHAPE=default PLANAR_DUMPSEGS=<S>/g3n-T2-segs.txt`): from the strands and the track map, list B's copies. For each copy, give the parent piece, the crossings it is over (which lines, in travel order), and its deck(s): the deck run and which crossed lanes each deck spans. Say whether the D and F crossings are one run of consecutive same-kind over crossings, which would make one deck per run (G3b2) span all four.
2. The same variant with MappedComposer (no `PLANAR_COMPOSER`), `JUNCTION_PROBE_ASCII=1` into `<S>/g3n-T2-mapped.txt`: read off how B crosses D1, D2, F1 and F2. Is it one bridge over all four, or two bridges with a gap, and if two, where does the gap sit and how long is it? Also give the build code site in MappedComposer that makes that choice, as file:line, with a one-line description.
3. Deck lengths: from the D357 rules (deck max 24, the deck–deck gap ≥ 4 in neighbour rows), the minimum deck length to span the four lanes in the track composer's geometry for this variant, and the same with the run split between D and F. Use numbers from the dump, not estimates.
4. Classify: (a) G3b2's "one deck per run of same-kind consecutive over crossings" merges crossings that MappedComposer bridges separately (the run rule is too strict); (b) the copy's single parallelogram forbids the gap; (c) other, described in two lines.

## Do not
- Change any composer rule, the map, the slot layer or the solve. Touch ports 4077/4079 or start a game client. Use git stash.

## Report
- References read; commit hash, if any; measures 1-4 with file paths; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
