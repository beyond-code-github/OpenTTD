# Branch inventory (2026-11-19, engineer: "how do we dig ourselves out of this mess?")

Measured with `git cherry feature/junction-tool <branch>` (patch id) and subject match.

## Planar solver line (the architecture, STATUS "Architecture")
- `wt-planar` (66 commits not on feature/junction-tool, base 3b6094e430 2026-11-15; pushed to the fork at c829db7742) and `wt-tracks` = wt-planar + TM1 aef8174b23 (not pushed). Worktree `.claude/ng1`. feature/junction-tool has none of the planar solver: the in-game tool built from it runs the legacy pipeline; the rig binary is built from ng1.

## Probe line (throwaway, chained, never landed)
- Bendy prio: probe-pb..probe-pb7, probe-pr, probe-rm (D347a/b form B).
- This session: probe-pd, sh, cm, sl, ud, fl, sy, fl2, et, ab, lo, lj, js (each on the previous).
- Older: probe-s2, s2b, s2c, s4a..s4e, spike-slots, dbg-s4a, dbg-s4d, dbg-s5.
- Landing candidates: o (offsets at least), r + g (free fray rise, fray gap at least), side-trial clash fallback, far-lane step (j), bendy prio join (D347b choice, via the track map), E jog as a searched shape candidate. Not: f, c, a, probe caps (diagnostic).

## Old pipeline (legacy, no new work)
- Landed (subject found on feature/junction-tool): wt-g33b4, g33b5, hm1, ig3, lr1, mj1, of1, r3, r5, tu4 and the other single-commit wt-* branches.
- Stopped / not for merge: wt-r7, wt-r7b (awaiting ruling, stopped), wt-tu5 (parked), wt-m1, wt-ng1, wt-np3, wt-o4b2, wt-o4c2, wt-choice-probe, wt-h1c, wt-probe-ll2, wt-oracle-227.
- Unclear: wt-2b2b1b "Add: Junction recording states band event rows and their order" (2026-11-14), not found on feature/junction-tool.

## Worktrees
- ng1 (wt-tracks, active); crash, play, prof (detached); nodes (wt-choice-probe), nodes2 (wt-before), o4b2, o4c2 (old WIP).

## Clean-up done (engineer, 2026-11-19: merge now; archive-tag then delete)
- feature/junction-tool 41575a7909 merges wt-tracks (source identical to aef8174b23). Rules audit pass 1 committed 3c5a4152af.
- 305 branches tagged `archive/<name>` and deleted (probe, dbg, spike, stopped legacy WIP incl. wt-2b2b1b and wt2-f1, and every wt-* whose commits are on feature/junction-tool by ancestry or subject). Local branches left: feature/junction-tool, master, wt-tracks (ng1).
- Worktrees removed: crash, nodes, nodes2, o4b2, o4c2, play, prof. Untracked files of crash and nodes saved to /home/pete/OpenTTD-archive/worktrees-2026-11-19/*.tgz (the others had none).
- From now on: landings branch from feature/junction-tool; probes branch from it too, never from another probe.
