# Step: hard anchor in the mapped composer (lands probe wip/PROBE-anchor.md)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`. One heavy job at a time. Time box 120 minutes.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `95fe3d99c7`.
- Read first: `/home/pete/OpenTTD/CODINGSTYLE.md` (C++); `docs/junction-tool/wip/PROBE-anchor.md` and `wip/probe-anchor.patch` (the probe's code, behind `PLANAR_ANCHOR`); `src/junction/planar/CLAUDE.md` "Component solver"; `src/junction/CLAUDE.md` "Working rules for agents".

## Behaviour (no env switch: the mapped composer always does this)
1. Take the probe patch's model change as the behaviour, without the env: `Model` members `X0`, `Y0` (box low edges, `Ex`, constant 0 by default; the mapped composer makes them `2 * a`, `2 * b` with integer columns a, b so translation is by whole tiles), mapped box rows and both port-side sites (`PortIn`, out ports) on X0/Y0, and the pin window `4096 <= u <= 4097` on each axis for the first port's first line's point. The older `Composer` path stays as it is.
2. Output (the probe's defect: Y0 floats, so a shift by X0/Y0 is wrong): after the mapped solve, shift every exported coordinate by (sx, sy) computed from the solution alone: sx = the largest even number <= (least x over every strand point and prio-row point), sy = the largest even number <= (least y over the same points) - 1. Width and height from the shifted points: (greatest x - least x after shift) as today's formula uses X, i.e. width = (max shifted x)/2 + 1, height = (max shifted y)/2 + 1, matching today's `X/2 + 1` when today's X equals the greatest x. Shift in the mapped caller only (as the probe did).
3. The objective, pull and everything else unchanged.

## Tests (outermost first; red recorded before the code)
1. New `[junction][planarcomponents]` case: T2 mapped (`SolvePlanarMapped` on `PlanarTText(2)`, test spec): area 862.25, width 94, height 29, and every route piece's tile within [0, width) x [0, height). Red today: none expected for area; it is the guard for step 2: run it red against the probe's shift (apply the probe's output code first, see the height defect fail the bounds), then fix.
2. The existing T2/T3/LL² mapped tests stay green with today's bests (T2 862.25, T3 1933.25 `o1t3f1s0`, LL² 1347.25 `o17t0f1s0`).
3. The mapped plan tests that run signals and `CheckJunctionRules` on T2/T3 (`src/tests/junction/planar/planar_plan.cpp`) stay green.
4. Report (not assert): for T2 `o1t3f1s0`, are the route programs identical to today's (dump both, diff)? If not, how many pieces differ (equal-area tie choices are expected).

## Gates
- `/home/pete/OpenTTD/tools/junction/jt gate build-rel` (count, time; budget 30 s, it ran 29.5 s before).
- T2, T3, LL² mapped tests singly with wall times, twice each, against today's (T2 2.8 s, T3 4.1 s, LL² 92 s).
- `/home/pete/OpenTTD/tools/junction/pv check build-rel T2` and `T3` (exit 0: no variant changed area or reason; if a baseline file is missing, run `pv run` before your change and after, and diff).
- Update `src/junction/planar/CLAUDE.md` (a sentence beside the pull: the anchor, the whole-tile translation, the output shift). Commit on wt-planar: "Feature: Junction planar hard anchor removes the sliding symmetry", ending "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>".

## Report
References read; commit; red evidence; members added; gate and timing results; route identity (test 4); `git status --porcelain`, `git log -1`. When you have reported, stop.
