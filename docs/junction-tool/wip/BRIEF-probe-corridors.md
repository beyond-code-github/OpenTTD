# Probe: corridor walls as up-front non-overlap rows (validity, then effect)

Throwaway probe: nothing is committed. Time box: 60 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Keep context small: outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/corridors/`, code read with offset/limit, command output to files. If anything below does not fit the code as described, stop and report; do not improvise a different rule.

## State and idea
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`, Release build dir `build-rel`.
- `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` §3 and §6 (short). Checked: every face of T2, T3, LL² (walked with travel directions) is two walls from one start vertex to one end vertex, or one wall against the box, except the empty crossing triangle. Idea: non-overlap per corridor: the right wall stays right of the left wall, written up front as linear rows, so the lazy clash loop has little or nothing left. The earlier blanket attempt (`PLANAR_FACESEPS`, every segment pair of two face lines) over-constrained because pieces far apart along a corridor are not alongside each other. This probe tests a matching rule that pairs only pieces at the same phase of the corridor.
- The face dump code: `/home/pete/OpenTTD/docs/junction-tool/wip/facedump.patch` (prints each face walk: `name#stretchF` = walked along travel, `B` = against, `R` = box edge; the walk keeps the face on its left, so an `F` line is the face's RIGHT wall and a `B` line its LEFT wall, in reverse travel order).

## Read first
- `src/junction/planar/planar_regions.cpp` (PlanarMap, ~20-180), `planar_regions.h`.
- `src/junction/planar/planar_model.h`: `Seg`, `Strand`, `CrossRec` (~154-185), `Right`, `Dot`.
- `src/junction/planar/planar_components.cpp`: `MappedComposer` build entry and where `PlanarLineSides` is built per variant; `SolveLazy1`, `SepOptions` row shape.
- `src/tests/junction/planar/planar_plan.cpp` `[.mappeddump]` (how a mapped variant is built and solved from a test).

## The walls of a face, in composer segments
- A line's stretch k runs between its k-th and (k+1)-th event (crossings in `topology.events[line]`, the line's start and end). In the composer, line X is strand X (copies `X"..`/`X'..` are ignored here). Its stretch's segments: from the segment holding its k-th crossing (the `CrossRec` whose `over` or `under` is on strand X; match crossing records to topology crossings in event order along the strand) to the segment holding the (k+1)-th, both inclusive; the line start/end for the first/last stretch.
- Right wall: the face's `F` entries in walk order; left wall: its `B` entries reversed. Each wall is then a list of segments in travel order. Faces with an `R` edge, the outer face and the crossing triangle are skipped (box walls are already held by the port rows).

## Rule R1 (phase matching)
For each face: collapse each wall's segment list into runs of one heading (consecutive segments of equal heading form a run; zero-length segments count). For each heading h present in both walls, with `nL` runs of h on the left and `nR` on the right:
- `nL == nR`: pair the k-th left run with the k-th right run.
- `nL == 1` or `nR == 1`: pair the single run with every run of h on the other side.
- otherwise: skip, and count it.
Each pair (left run a, right run b) gives rows: for every segment end point p of b and q of a: `Dot(Right(h), p) - Dot(Right(h), q) >= 2`.

## Phase A: validity on solved layouts (no solver change)
- Solve at {HEAD} the best variants: T2 o0t2f1s1, T3 o0t1f1s1, LL² o49t0f1s0 (and o1t3f0s0). Evaluate every R1 row on each solution; report per hub: faces used, pairs, rows, skipped (h, face), and every violated row (face, walls' lines, h, slack).
- R1 is valid only if no row is violated on any of these solutions.

## Phase B: only if phase A has zero violations
- Knob `PLANAR_CORRIDOR`: add all R1 rows before the first solve (tag `corridor` via `TagScope`), lazy loop unchanged.
- Measure with and without the knob: `/home/pete/OpenTTD/tools/junction/pv run build-rel T2 ...` and `T3` (full variant sets; compare with `pv diff`), LL² o49t0f1s0, o1t3f0s0, o0t0f0s0, o1t0f0s0 with `PLANAR_TIMES=1`, `timeout 60`: area, ms, lazy rounds, clashes in round 0.
- For any variant newly infeasible: `PLANAR_IIS=1` core (first 30 lines).

## Output
- Findings under 40 lines: `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-corridors.md` (verified vs inferred).
- `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-corridors.patch`; `git checkout -- .`; confirm `git status --porcelain` empty; rebuild `openttd_test`.
- Report in under 15 lines. When you have reported, stop.
