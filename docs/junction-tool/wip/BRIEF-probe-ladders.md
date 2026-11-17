# Probe: ladders from a rough solve replace the clash loop (fixed shapes first)

Throwaway probe: nothing committed. Time box: 60 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ladders/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`, Release `build-rel`.
- Read `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` §6-§7, `wip/PROBE-corridors.md`, `wip/CHECK-rungorder.md`. Apply `wip/probe-corridors.patch` and reuse its face/wall/segment code, with the rungorder fix (rotate each wall's stretches until end vertex joins start vertex; see CHECK-rungorder.md).
- Question: given a rough solved layout, do rung rows on every corridor keep a re-solve legal with no (or very few) lazy clash rounds, at the same area? Shapes stay fixed (no free bends yet).

## Rung rows (from a rough solution S)
For each corridor (faces without box edges, not the crossing triangle), walls L and R as segment lists in travel order (base strands only):
- For every end point p of every segment of L: in S, find the R segment j whose span along its own heading contains p's projection, nearest to p (perpendicular distance); if none contains it, skip p and count it. Add the row: p stays on the left of segment j's line by at least the spacing: `Dot(Right(h_j), R_j.a) - Dot(Right(h_j), p) >= 2 * |Right(h_j)|_scale`, using exactly the scale the lazy loop's `SepOptions` uses for a direction of that heading (read it; do not invent one).
- Symmetrically every end point q of R against the facing L segment: `Dot(Right(h_i), q) - Dot(Right(h_i), L_i.a) >= same`.
- Rows whose point is a shared vertex of both walls (s, t) are dropped. Tag `rung` via `TagScope`.
- Verify on S itself first: every rung row must hold on S (it is built from S). Report any that do not (that would be a construction bug).

## Runs (variants: T2 o0t2f1s1, T3 o0t1f1s1, LL² o49t0f1s0, LL² o1t3f0s0)
1. Rough: solve at {HEAD} as today (`PLANAR_TIMES=1`): area, ms, lazy rounds, clashes per round.
2. Ladder: rebuild the same variant, add all rung rows from step 1 before the first solve, run the normal solve (lazy loop still on, as a safety net): area, ms, lazy rounds, clashes found in round 0 (want 0) and which pairs (lines, copy or base).
3. If step 2's solution moves any rung point outside its facing segment's span: rebuild the rows from step 2's solution and solve once more; report the same numbers.
- Any infeasible run: `PLANAR_IIS=1` core (first 30 lines).

## Report (findings under 35 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-ladders.md`)
Per variant: rung rows, skipped points, rough vs ladder (area, ms, rounds, round-0 clashes and their pairs), second pass if run. Then `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-ladders.patch`, `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
