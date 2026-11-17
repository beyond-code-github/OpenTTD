# Brief 3a: clash sides from the topology (regions, ring, leg order)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, head `{HEAD}`, build dir `build-rel` (Release). Build only the `openttd_test` target, never `openttd` (a rig and the engineer's client run that binary). Never use git stash, never start a game client, never touch ports 4077/4079.
- Plan: `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` (whole). Evidence: `wip/SPIKE-regions.md`, `wip/SPIKE-regions2.md` (253 clash sides, 0 contradictions). The spike code: `wip/spike-regions2.patch`, `wip/spike-regions2-test.cpp` (read them: the face map, the ring rule and the comparison are already written there; port what you need, do not apply the patch).
- Today the lazy loop (`SolveLazy1`, planar_components.cpp) separates one clash per round (`bad[0]`), trying up to 4 directions with a trial solve each (`SepOptions`). This step gives each clash its side from the topology, so a round separates every decided clash at once with no trial solves.

## 2. Read first (once, offset/limit)
- `CODINGSTYLE.md` (C++ part); `src/junction/planar/CLAUDE.md`.
- `planar_topology.h` (lines, nodes, crossings, events, ring, routes), `planar_channels.h` (`node_legs`).
- `planar_components.cpp`: `GeoSet`/`Geo`, `Clashes`, `SepOptions`, `SolveLazy1` (~1676-1970).

## 3. The side oracle (new files `src/junction/planar/planar_regions.h` and `.cpp`, in its CMakeLists)
```cpp
/** Which side of each other two lines keep, read from the topology alone (DESIGN-tracks §3). */
class PlanarLineSides {
public:
	PlanarLineSides(const PlanarTopology &topology, const PlanarChannels &channels);
	/** Where line p lies relative to line q's travel: +1 right, -1 left, 0 undecided (or p == q). */
	int8_t Side(uint32_t p, uint32_t q) const;
	/** Empty, or why the face map failed validation (V - E + F != 2, a face not a simple walk). */
	const std::string &Why() const;
};
```
Sources, in this order:
1. **Faces:** the spike's planar map (vertices: port ends, nodes, crossings; edges: line stretches between events plus the ring boundary; rotation as in the spike). Two lines bounding a common face: the side the face gives.
2. **Ring:** one route path through each line; if the two routes share no line and cross nowhere, the spike's arc rule (the clockwise ring arc from q's route entry to its exit lies on q's left).
3. **Leg order:** if the routes share a line, find the first node where the two routes meet (a merge both enter, or by symmetry a split both leave); the node's in (or out) order left to right gives the side of the two legs; p and q take the side of the legs their routes run on there.
Two lines that cross each other (a topology crossing between them) have no single side: `Side` is 0 for them (their clashes keep today's path). Precompute a table for all pairs. `Side(q, p) == -Side(p, q)` always.

## 4. Tests (new file `src/tests/junction/planar/planar_regions.cpp`, tag `[junction][planarregions]`, red first with `Side` returning 0 and `Why` empty)
1. "planar regions: the face map is valid": T2, T3 (`PlanarTText(3)`), LL² (`PlanarLL2Text()`): `Why()` empty.
2. "planar regions: sides are antisymmetric and decided for every pair of neighbouring lines": for all p ≠ q, `Side(p,q) == -Side(q,p)`; on T2, T3 and LL² every pair of lines that do not cross each other is decided (non-zero); report the undecided count in INFO.
3. "planar regions: known sides": T2: C1 is right of F2 (`Side(C1, F2) == +1`); LL²: D1a is right of E1 (`+1`); plus two face pairs from SPIKE-regions2.md's agree lists (e.g. LL² A1 left of A2a), with the side as the spike logged it.

## 5. The composer
- `MappedComposer` builds a `PlanarLineSides` once per variant (after the channels it uses, `ch`); if `Why()` is not empty, `M.Fail(Why())`. Hand it to the solve through a member the lazy loop can read (the existing path from `SolveMappedVariant` to `SolveLazy`; add one parameter `const PlanarLineSides *sides` to `SolveLazy` and `SolveLazy1`, null for the hand composer).
- Strand → line: a strand's base line is its name with any trailing copy mark (`'` or `"` and any digits after it) removed, looked up with `PlanarLineByName`.
- In `SolveLazy1`, a clash round: for every clash in `bad` whose two strands map to different lines with `Side != 0`: take `SepOptions` for that clash (sorted best first) and use the first option whose direction `f` satisfies `Dot(f, Right(h_q)) * Side(p_line, q_line) < 0` (the rows keep q at least 2 beyond p along f, so p lies on q's −f side; h_q is q's segment heading from `g.geo`). Add those rows for all such clashes together, then one `M.Solve`. If that solve fails: `why = "infeasible with topology sides"` and return false. If the round has no decided clash, run today's path for `bad[0]` unchanged. `SepOptions` must expose the direction it used for each option (return it alongside the rows); no other change to it.
- Under-deck handling, the area step and the up-front rules stay unchanged.

## 6. Measure (before at the launch head, after)
- `tools/junction/pv save build-rel T2` and `... T3` before (in /home/pete/OpenTTD/tools/junction/pv), `pv check` after: report the diff summary and best areas. Areas may change (a different, still legal choice); report every changed variant. Wall time of each `pv run`.
- LL² with `PLANAR_TIMES=1`, `timeout 60`: o49t0f1s0, o49t0f0s0, o1t0f0s0, o0t0f0s0: area, size, ms, rounds, before and after.

## 7. Do not
- Do not remove the up-front clash rules, the gap steps or anything in `Fork`/`Join`/`Fray`/`Bundle`/`Flex`. No env knobs.

## 8. Stop conditions
- The face map fails validation on any of the three hubs, or test 2 finds undecided non-crossing pairs, or a contradiction between sources (a pair where faces and ring disagree): stop, report.
- T2 or T3 best area worse than 906.25 / 1941.25, or more than 2 T variants newly failing: stop before committing, report the `pv` diff.

## 9. Gates
- `tools/junction/jt fast build-rel '[planarregions]'`, `'[planargaps]'`, `'[planarmodel]'`, `'[planar]'`.
- `src/junction/planar/CLAUDE.md`: the side oracle and the lazy loop's use of it, present tense, no decision ids.
- One commit: `Change: Junction clash sides from the topology (regions, ring, leg order)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 10. Report (short)
References read; commit; red evidence; types/functions added; `pv` diffs and times; LL² table; gates with count and time; stops; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.

## Revision (2026-11-17, after the agent's stop)
- Antisymmetry dropped: Side(p, q) is relative to q's travel; antiparallel neighbours can each be left of the other.
- Ring and leg-order sources dropped: a line carrying several routes has no single side against another line (EF1 vs CD1: F1|C1 by ring says right, E1|D1 by leg order says left). Faces only: Side(p, q) = the side of q where their common face lies; 0 when they share no face, cross, or share faces giving opposite sides.
- Non-face clashes keep today's trial path in 3a; 3b pre-generates the face-neighbour separations so they are kept apart through the lines between.
