# BRIEF S2c1: separators per crossing, maximal runs, copies from runs

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-structure, head HEAD_HASH. Release build build-rel (current). Logs: /tmp/claude-1000/-home-pete-OpenTTD/83ff9380-9534-4138-8dc1-79a833146e49/scratchpad/s2c1/. Tests print with WARN, never std::cout.
- Implements docs/junction-tool/wip/SPEC-structure.md §2 item 3 (structures: maximal runs on the separator, no node between), §4 P3 and P5, §7 "S2c design notes" (1): grouping drives copies, not the reverse. Rulings: D369, D371(2) (one structure per maximal run, split only where a kind's limit or D228a forces it), D371a, D372, D364, D331/D332.

## Read first (once each, with offset/limit)
1. docs/junction-tool/wip/SPEC-structure.md §2, §4, §7.
2. src/junction/planar/planar_regions.h (whole header, short) and planar_regions.cpp: `CopySide` (~417), `JoinLanes`, `RoleAt`, `PlanarMovers`, `BuildPlanarTrackMap` (~546 to its end: rows, prio-join copies, mover copies from S2b, channel copies, `copy_over`).
3. src/junction/planar/planar_paths.cpp lines 200-220 (how `over` comes from bits) — read only.
4. src/junction/planar/planar_structure.{h,cpp} (the oracle) and src/tests/junction/planar/planar_structure.cpp (`WrapToday`, tally).

## Changes
1. **Separator per crossing** (planar_regions.{h,cpp}): `std::vector<uint32_t> PlanarCrossingSeparators(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const std::vector<uint32_t> &over, const std::set<uint32_t> &separator_movers, std::string &why)`: per topology crossing, the line that carries its structure: for a crossing in a channel crossing's `line_crossings`, its line on the `over` channel; for a crossing in a node group (`group_crossings`), the mover if in `separator_movers`, else the keeper (S2b's rule; prio joins included, the same rule). A crossing in neither, or in both: `why` set, naming it.
2. **Runs** (planar_regions.{h,cpp}): `struct PlanarStructureRun { uint32_t line; std::vector<uint32_t> crossings; bool copy; };` (doc-comment: a structure as a run of topology crossings on its separator line, in travel order; copy when the line's share is full) and `std::vector<PlanarStructureRun> PlanarGroupStructures(const PlanarTopology &topology, const PlanarChannels &channels, const PlanarLineSides &sides, const std::vector<uint32_t> &separator, const JunctionSpec &spec)`: for each line, walk `sides.Events()[line]` (the variant's event order) and cut maximal runs of consecutive crossings whose separator is that line; then split each run greedily from its start into the fewest pieces such that each piece has `crossings.size() <= spec.max_bridge_middle` and, when copy, `crossings.size() + 2 <= 2 * spec.sd + spec.tl` (D228a). `copy = channels.line_share[line] >= 1.0`.
3. **Track map from runs:** `BuildPlanarTrackMap(topology, channels, sites, const std::vector<PlanarStructureRun> &runs)` replaces the `over` and `separator_movers` parameters. Rows exactly as today. Copies: exactly one `copy_over` per run with `copy`, over that run's crossings, nothing else makes copies. Side: if all the run's crossings are channel crossings of one channel, that channel's `CopySide`; if all are crossings of one site's group, the side the S2b code uses for that site kind (prio join: its sigma; plain join: `sites[s].side`; fray: `CopySide`); otherwise compute both and use it if equal, else set `why` to "copy sides disagree on <line>" (a finding, see tally).
4. **Callers:** every caller builds `over` as before, then `PlanarCrossingSeparators`, `PlanarLineSides(topology, channels)`, `PlanarGroupStructures`, then the map. Add a one-call test-side helper for this in each test file that needs it (not in the library).
5. **Oracle, P3 maximality:** two structures with the same separator track whose crossings are adjacent entries of that track's `map.events` (no event between) are a breach ("P3 not maximal") unless merging them would breach P5 (bridge limit, or D228a when it has a copy).
6. **Wrapper:** `WrapToday` makes one `PlanarStructure` per run (map crossings of the run's topology crossings, copy = the run's copy track).

## Tests (red before green; record each failure line)
1. Oracle maximality: from the S1/S2a hand base case, split one multi-crossing structure into two adjacent ones → exactly one "P3 not maximal" message (red before change 5).
2. Before changes 1-4, run the new oracle (change 5 first) over S2b's maps in both uniform forms and record the "P3 not maximal" counts per hub: that is the red for the refactor (if all 0, say so; the refactor still lands, since grouping must drive copies).
3. After: in both uniform forms (`{}`, all movers) on T2, T3, LL2: P1-P7 all 0, "P3 not maximal" 0, and print per hub: runs, runs merged across a channel and a site group, runs split by P5, and copy-side disagreements (each with one example by line names).
4. Identity: for every variant where no run is merged across channel and site and none is split, the map's track names and per-track events equal S2b's (compare by kind and line name as S2b did). Golden files from the S2b binary before your change, in the scratch dir.

## Do not
- Do not change `BuildPlanarPaths`, channels, sites, rows or the composers.
- No enumeration, cost, ranking or `BuildPlanarStructure` (S2c2).

## Stop conditions
- Any copy-side disagreement: finish the commit only if the count is 0; otherwise stop before committing and report every case.
- A topology crossing with no separator rule (change 1's why).
- An existing test fails (report; do not edit expectations).

## Gates
- Release build, zero warnings in touched files; `tools/junction/jt fast build-rel '[planarstructure]' 5`, `tools/junction/jt fast build-rel '[planar]' 5`, `tools/junction/jt gate build-rel` within 30 s.
- Commit: `Change: Junction planar structures as maximal runs; copies from runs (S2c1)` with trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; functions and types added or changed (names); the test 2 and test 3 tallies; identity result; gates; stops; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
