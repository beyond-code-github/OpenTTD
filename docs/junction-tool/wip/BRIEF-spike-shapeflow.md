# Spike: shape as a flow (bends as turning carried by faces, no yes/no)

Throwaway spike: nothing committed. Time box: 75 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/shapeflow/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## Idea (DESIGN-tracks §8.5; engineer: bends are not optional pieces, they are track like any other)
Headings are multiples of 45° (0 N .. 7 NW, clockwise, y down). Walk any face of the planar map with the face on the left: the turns along it (inside stretches, at vertices, along the box) add up to one full turn: -8 in 45° units, clockwise positive (the outer face +8). A stretch's net turn b_e (sum of its signed heading changes) appears in the two faces it bounds with opposite signs, so with vertex and box turns fixed, "every face sums to -8" is a network-flow system: an LP over it returns whole numbers by itself (no branching). The spike asks: does today's geometry satisfy it (a check of the face map against the layout), and what net turns does a minimum-turn flow choose versus today's?

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`. Read `src/junction/planar/CLAUDE.md`, `planar_regions.{h,cpp}` (PlanarMap walks, `Faces()`, `Events()`; forward dart = face left of the line's travel), and in `planar_components.cpp` the composer's ladder face code (`ladder_faces`, the stretch-to-segment mapping from `Events()`), `SolveMappedVariant`, `Model::Solve` (HiGHS).

## Change (env `PLANAR_SHAPEFLOW=1`, spike only, in `SolveMappedVariant` after the rough solve succeeds; print to a file named by env `PLANAR_SHAPEFLOW_OUT`, one per variant)
1. **Full face walks.** Expose from PlanarMap (spike-only accessor) every face's walk in order: darts (line, stretch, forward?) and ring edges. For each dart, the stretch's segments in travel order (as the composer maps them); a reverse dart walks them backwards with headings d+4.
2. **Turns from the rough solution.** turn(a -> b) = ((b - a + 12) mod 8) - 4 mapped to [-3, 4]; a turn of 4 (a cusp: two legs leaving one vertex parallel) takes the sign of the face, i.e. -4 on inner faces, +4 on the outer. Per face: within-dart turns (reverse darts: negated, as walking backwards negates each turn), vertex turns (last heading of one dart to the first of the next), ring arcs (from a dart ending on the box, along the box sides with their corners, to the next dart: compute from the solution's coordinates and the box). Check every face sums to -8 (outer +8); list failures with the face walk.
3. **Flow LP.** Variables: b_e per stretch (free integer range [-8, 8], as b_e = p_e - n_e with p, n >= 0), vertex turns and ring arcs fixed to the rough values. Rows: per inner face, sum of its darts' signed b_e + fixed turns = -8 (the outer face row is implied; drop it). Objective: minimise sum (p_e + n_e). Solve as an LP (continuous) with HiGHS; record whether the solution is integral, its objective, and the rough's sum |b_e|.
4. **Second LP:** also free the turns at crossing vertices within [1, 3] in magnitude (crossing angle 45° to 135°), keeping their sign; same report.
5. **Jogs:** count stretches whose rough net turn is 0 but which have heading changes (sideways steps: invisible to a net-turn flow), and total heading changes per hub.

## Runs
T2 o0t2f1s1, T3 o1t2f1s1, LL² o1t3f0s0 (`PLANAR_ONLY=<tag>` on the mapped test of that hub; see `/home/pete/OpenTTD/tools/junction/pv` for the test names), then the full T2 set.

## Report (findings under 35 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/SPIKE-shapeflow.md`)
Per run: faces, faces failing the -8 check (list), stretches, LP 1 and LP 2 (integral?, objective vs rough sum |b|, stretches whose b differs from the rough with both values), jogs (count, share of all heading changes), ms. Then `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/spike-shapeflow.patch`, `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
