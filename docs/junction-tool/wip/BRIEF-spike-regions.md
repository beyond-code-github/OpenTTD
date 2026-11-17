# Spike: regions from the topology vs the lazy clash sides

Throwaway spike: nothing is committed. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only the `openttd_test` target in `build-rel`, never `openttd`. Keep context small: outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/regions/`, code read with offset/limit.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `de1301993c`, Release build dir `build-rel`.
- Background: `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` (whole, short). The claim to test: the topology is a planar map; its faces (regions between neighbouring lines) give every clash side the solver now finds by trial.

## Read first
- `src/junction/planar/planar_topology.h` (PlanarEnd, PlanarLine, PlanarPort, PlanarNode, PlanarCrossing, `events`, `ring`) and how `ring`, `crossings` and `events` are derived in `planar_topology.cpp` (grep `ring`, `crossings`, `events`; read only those functions).
- `src/junction/planar/planar_components.cpp` `SolveLazy1` (~1885-1970): where a clash pair is separated and which direction is taken (the `sep`/trial rows and `bad[...]` `SegKey`s); `GeoSet`/`Geo` (~1676-1712) for each segment's heading.
- The existing hidden tests at the end of `src/tests/junction/planar/planar_plan.cpp` (how `[.mappeddump]` builds a topology).

## 1. The planar map (a hidden test, `[.regions]`, in a new file `src/tests/junction/planar/planar_regions.cpp` registered in its CMakeLists)
For a topology (`PlanarTText(2)`, `PlanarLL2Text()`):
- **Vertices:** each port line's box end; each node; each crossing (`topology.crossings`).
- **Edges:** each line split at its crossings in travel order (`events[line]`): stretches start→c1→…→end. Plus boundary edges joining consecutive ring ends (`ring`, closing the cycle).
- **Rotation** (clockwise order of edge ends around each vertex; canonical frame x east, y south):
  - split node: the in edge, then the out edges left to right across travel (`node.out` order);
  - merge node: the out edge, then the in edges right to left (`node.in` is left to right);
  - port end: the boundary edge to the previous ring end, the line, the boundary edge to the next ring end, in the order that makes faces consistent;
  - crossing of lines a and b: a's incoming stretch, then b's end on a's left, a's outgoing stretch, b's end on a's right. Which of b's ends is on a's left: derive it from the ring (the arc clockwise from a route's entry to its exit lies to that route's left; use one route path through each line) or from how `planar_topology.cpp` derives the crossing.
- Trace the faces (standard face walk on the rotation system). **Validate:** V − E + F = 2; every face a simple closed walk; exactly one face outside the box. If a rotation choice is ambiguous, pick the one that passes validation and report the choice. If no choice passes, stop and report.
- Print each face: its boundary as a list of (line, stretch index, side: the face lies left or right of the line's travel).

## 2. The lazy clash sides (temporary print in `SolveLazy1`)
- For the final successful round, print every separation the lazy loop added: the two segments (strand name, index), each one's heading, and the chosen direction vector `f` (the side kept). Strand names map to topology lines: strip copy marks (`'`, `"` and the digits after them) to get the base line name.
- Run T2 o0t2f1s1 (`"planar components: T2 from its topology text (mapped) at the probe's area"`, `PLANAR_ONLY=o0t2f1s1`) and LL² o49t0f1s0 (`"planar components: LL² from its topology text (mapped) at the probe's area"`, `timeout 60`).

## 3. Compare
For each lazy separation between lines p and q (base names; skip pairs of the same line):
- **Same region:** p and q bound a common face. Read the side the face implies (p left or right of q, relative to q's travel on that stretch) and compare it with the side the solver chose (sign of `f` · Right(heading of q's segment); positive means p was kept to q's right). Count agree / contradict.
- **Not adjacent:** no common face. Count, and list up to 10.
- Separations between a line and itself or its own copy: count separately.
Report per hub: number of faces, V/E/F, lazy separations total, same-region agree, same-region contradict (list all with the face), not adjacent (with examples), self/copy.

## 4. Output
- Findings (under 60 lines, plain language): `/home/pete/OpenTTD/docs/junction-tool/wip/SPIKE-regions.md`. Mark what is verified by the run and what is inferred.
- The spike code as a patch: `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/spike-regions.patch` plus the new test file copied to `/home/pete/OpenTTD/docs/junction-tool/wip/spike-regions-test.cpp`.
- Then restore: `git checkout -- . && git clean -fd src/tests/junction/planar/` (only the new file), confirm `git status --porcelain` is empty, rebuild `openttd_test`.
- When you have reported, stop.
