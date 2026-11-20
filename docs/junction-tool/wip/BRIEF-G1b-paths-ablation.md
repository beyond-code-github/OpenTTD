# BRIEF G1b: expanded route paths over tracks; G1 evenness and family ablation (Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (G1, f0b8b71a41). Debug `build`, Release `build-rel`.
- Follows G1 (wip/BRIEF-G1-joint.md; spec wip/SPEC-slots-lengths.md §8). G1 found: (1) `PlanarTrackMap::expanded.routes[r].paths` are the topology's paths copied unchanged (planar_regions.cpp, `result.expanded = t` near line 885): they list topology lines, not tracks, so line parts and copies are missing (LL2: path EF2, F2, SO2 skips F2's later part). The expanded topology's lines ARE the tracks, so its paths must walk the tracks. (2) G1's parity rows miss that an axis piece's length must be even. (3) T2 and T3 are infeasible with an IIS mixing beside, span clear and crossing rows; the cause is unknown.

## Read first (once, offset/limit)
- /home/pete/OpenTTD/CODINGSTYLE.md (C++ part).
- src/junction/planar/planar_topology.cpp lines 245-285 (the parser's path walk) and planar_topology.h.
- src/junction/planar/planar_regions.cpp lines 855-890.
- src/tests/junction/planar/planar_slots_probe.cpp (your G1 file).

## Commit 1: `Fix: Junction planar: the track map's route paths walk the tracks`

Test first, in src/tests/junction/planar/planar_shape_layer.cpp, tag `[junction][planar][planarshape]`: "planar track map: expanded route paths run over the tracks". For T2 and LL2 (`BuildPlanarStructure(topology, spec, 16)`, first variant each), for every route r of `map.expanded` and every path: the first track's `from` is a port, the last's `to` is a port, each next track is in the `out` list of the previous track's `to` node, and every track listed carries r (`lines[t].routes`); and every track that carries r lies on some path of r. Record the red failure (expected on LL2 at least).

Changes:
- planar_topology.h/.cpp: move the parser's walk (from "Paths: per route, every walk..." through the push of each finished walk and its error cases) into `bool PlanarRoutePaths(const PlanarTopology &t, uint32_t r, std::vector<std::vector<uint32_t>> &paths, std::string &error)`, with a doc comment (every walk over lines carrying route r from its one entry port line to an exit port line; false with `error` set on the parser's existing error cases, same texts). The parser calls it per route; its behaviour and messages are unchanged.
- planar_regions.cpp: after `result.expanded = t`, for every route, replace `result.expanded.routes[r].paths` by `PlanarRoutePaths(result.expanded, r, ...)`; on false set `result.why` to the error and return as the function does for other failures.
- src/junction/planar/CLAUDE.md: "Track map": the expanded topology's route paths walk its tracks (parts and copies included), by `PlanarRoutePaths`; "Topology": the walk is `PlanarRoutePaths`.
- Runs: `jt fast` Debug on `[planarshape]` and `[planar]`; Release gate. Report per hub (T2, T3, LL2 first variant) the total path count before and after (print once in the new test with `INFO`/a temporary print you remove before committing, or by a one-off count in your report).

## Commit 2: `Add: Junction planar probe: G1b, evenness and the lower-bound ablation`

In planar_slots_probe.cpp only:
1. Rename the test to `"planar probe: G1 joint slots and lengths on T2 T3 LL2"` (no comma: Catch splits names at commas).
2. Add the evenness row: each axis piece's total (its parts and step sub-parts) `= 2 * aux`, aux integer, tag `even <track> <piece>`.
3. `SolveG1` gains `uint32_t off` (default 0): a bit set leaves that family's rows out: 1 beside (item 6), 2 span clear (item 7, the clear rows), 4 deck gap and limit (item 7, the gap and span rows), 8 landing, 16 crossers (item 8), 32 node legs (item 9), 64 coverage (item 10), 128 bays (item 11), 256 curves (item 12). The oracles still run on any solution.
4. In the test, per variant: the full solve (off 0) as now; then, when it is not solved, one solve per single bit (9 solves) and one with all nine bits set, each printed as `G1A <hub> <tag> off=<bits> solved=<0|1> ms=<ms> clashes=<n> curve=<n> why=<first 300 chars>`.
5. Run in Release alone: `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 1800 ./openttd_test "planar probe: G1 joint slots and lengths on T2 T3 LL2" > g1b-run.txt 2>&1; grep '^G1' g1b-run.txt > g1b.txt`.
- Gate in Release after the commit.

## Do not
- Change any other rule or row of G1; add clash, separation or frame rows; change Shape, Structure or the parser's behaviour beyond the path walk move; fix a clash or an infeasibility.

## Stop conditions
- Commit 1 makes an existing test fail (H7 or any other): report the test and its message and stop at a clean state (WIP commit if needed, never stash).
- Any `STOP` throw in the probe other than on a variant (it aborts the run): report it and the variant, and stop.
- `consistency > 0` on a solved variant.

## Report (short)
- Commits; red evidence; path counts before/after per hub; gate results (count, time); `g1b.txt` verbatim if under 120 lines, else the `G1 ... SUMMARY` lines, every `G1 <hub> <tag>` line and every G1A line; stops; `git status --porcelain`, `git log -1`.
When you have reported, stop.
