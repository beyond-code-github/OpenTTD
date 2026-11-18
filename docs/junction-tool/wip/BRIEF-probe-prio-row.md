# Probe A3: the prio row as a gapped offset copy of the near lane (DESIGN-tracks 13.1), main line over

Throwaway probe: nothing committed in ng1. Time box: 75 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. One heavy job at a time (no parallel test runs). Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/probe-pr/`; read code with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule. Do not start background work that outlives your report.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `e4b50580e1`. Apply `/home/pete/OpenTTD/docs/junction-tool/wip/probe-prio-bridge2.patch` (probe A + A2, behind `PLANAR_PRIOBRIDGE=1`: G9c exempts copy switches (`IsCopySplit`), the near lanes' coverage is summed along the strand, the near lanes' fixed prio row is not pushed). If it does not apply cleanly, stop and report.
- Read first: `docs/junction-tool/DESIGN-tracks.md` section 13.1; `wip/PROBE-prio-bridge2.md` (incl. the orchestrator reading); DECISIONS rows D342, D345, D347.

## Change (behind the same env)
In `MappedComposer`'s n-lane `Join` (`src/junction/planar/planar_components.cpp` ~971-1020), prio branch, for each near lane S[j] (j >= 1), after all of S[j]'s segments in this function are added:
1. Let T = 2 * (-sigma) * Right(h) (two half-units to the side away from the joiner; the side today's row takes). Let segs = the segments of S[j] added in this call, in order (record `M.strands.at(S[j]).segs.size()` before the first `Double` for this lane and after its last segment).
2. Let gap = the segments that are a deck of S[j] (the `d0` each `Double` returns; not the copy's `d1`, which lies on the sigma side).
3. Walk segs; each maximal run of consecutive segments not in gap becomes one row strand `prio:<S[j]>#<r>` (r = 0, 1, ... in walk order): `M.NewStrand(name, M.NewPoint())`, its start tied to the run's first segment's `a` + T (`M.Tie(start, {a[0] + T[0], a[1] + T[1]})` as the existing prio-row code ties its start), then for each segment s of the run `M.AddSeg(name, s.d, 0)` and `M.Add(Eq(<new seg>.L, s.L))`. These strands are named `prio:...` like today's rows, so they clash with every other strand as today's rows do; check that the box bounds loop covers them (it does for strands created before it, read and confirm).
4. The far lane S[0]'s fixed row and Block227 unchanged.

## Runs (sequential; baselines in `scratchpad/probe-pb/{T2,T3}-base.txt`, A2 results in `scratchpad/probe-pb2/`)
- T2 and T3 mapped full sets with `PLANAR_PRIOBRIDGE=1` (`/home/pete/OpenTTD/tools/junction/pv` as probe A2 did, `-j 1`): per variant area/size or failure text; best.
- If any variant solves: the mapped-dump test probe A2 used (plan + signals + `CheckJunctionRules`) on T2 and T3 with the env: checker clean, or each violation text; prio placement (pattern, search, or "does not fit" with its tile).
- If none solves: `PLANAR_ONLY=o1t3f1s0 PLANAR_IIS=1 PLANAR_DEBUG=1` on T3; report the IIS rows; add no rule.

## Report (findings under 30 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-prio-row.md`, committed alone on feature/junction-tool in `/home/pete/OpenTTD`, message "Doc: Junction probe A3 prio row as a gapped offset copy", ending "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>")
Then in ng1: `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-prio-row.patch` (cumulative; commit with the findings), `git checkout -- .`, remove untracked files you added, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
