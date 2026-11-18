# Spike: LL³ (three lanes each way) through the solver: does it build, what breaks first

Throwaway spike: commit nothing in ng1. Time box: 75 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes (a rig and the engineer's client are running). Build only `openttd_test`. Load: another agent builds and tests in ng1 at the same time, and the machine has a known CPU instability: build with `-j 8`, solve ONE variant at a time (`PLANAR_ONLY`), never a full sweep. Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll3/`.

## Goal (engineer: "We need to be able to build absolutely any hub at any scale")
Topology `/home/pete/OpenTTD/docs/junction-tool/wip/topologies/LL3a.txt`: LL² widened to three lanes, lane-paired (each exit lane i takes source lane i from both sides, one under a prio; no choice routes). Find out whether it parses, builds and solves, and what breaks first.

## Setup
`git -C /home/pete/OpenTTD/.claude/ng1 worktree add <scratch>/wt d2ee981bcc` (detached), a Release build dir inside it configured with the cache options of `/home/pete/OpenTTD/.claude/ng1/build-rel/CMakeCache.txt`; build `openttd_test`. Add a throwaway hidden test (tag `[.ll3]`) next to "planar: dump a mapped component layout" (src/tests/junction/planar/planar_plan.cpp) that reads the topology text from env `JUNCTION_TOPOLOGY_FILE`, parses it, and either (a) with `PLANAR_DRY=1` runs `SolvePlanarMapped` to list variants (the existing DRY print), or (b) solves one variant with `SolvePlanarMappedVariant(t, MakeTestJunctionSpec(), tag, nullptr, layout, &why)`, then plans and checks it as the dump test does (PlanPlanarJunction, CheckJunctionRules), printing area, size, ms, violations and why. Remove the worktree at the end.

## Steps (stop at the first hard failure and report it with the exact message)
1. Parse: errors? Face map (`PlanarLineSides(t, BuildPlanarChannels(t)).Why()`) valid?
2. Variant bits (open crossings, tie sites, triangles, candidate steps) and DRY: how many variants build, and the construction failures by reason (count per reason, 3 examples each).
3. Solve the first 5 built variants one at a time (and if one solves, 5 more chosen spread over the over bits): per variant ok/why, area, size, ms, which shape won (default or program), checker violations by rule.
4. For each failure class: name the code (file:line) that fails or refuses (e.g. the 2-lane-only / axis-only refusals at planar_components.cpp ~1250/1256, the "too many variants" guard, curve/gap rules) and whether it is a hard-coded limit (on the DESIGN-tracks §12 hit list) or a real infeasibility.

## Report
Findings under 40 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/SPIKE-LL3.md`, the throwaway test as `wip/spike-ll3.patch` (git diff of the throwaway worktree). Report under 12 lines. When you have reported, stop.
