# BRIEF S2b: a separator for every crossing inside a node group (movers)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-structure, head HEAD_HASH. Release build build-rel (current). Logs: /tmp/claude-1000/-home-pete-OpenTTD/83ff9380-9534-4138-8dc1-79a833146e49/scratchpad/s2b/. Test output prints with WARN, never std::cout.
- Implements docs/junction-tool/wip/SPEC-structure.md §7 S2 plan item (b); rulings D372 (one separator bit per mover; movers are joiners at any join and a fray's other leg; keepers are lanes and straight legs; copies by share), D364, D331/D332.

## Read first (once each, with offset/limit)
1. docs/junction-tool/DECISIONS.md: grep -n "^| D372 \|^| D364 " and read those two rows.
2. src/junction/planar/planar_regions.h and .cpp: `PlanarPrioJoiners`, `JoinLanes`, and `BuildPlanarTrackMap`'s join block (the per-joiner form and copies from S2a, commit 5122295632).
3. src/tests/junction/planar/planar_structure.cpp: `WrapToday`, `Classify`, `RoleAt`, the tally (S2a).
4. src/junction/planar/planar_channels.h lines 40-75 (`PlanarSite`, `group_crossings`, `line_share`).

## Facts (from S2a's tally, verified by the orchestrator)
After S2a, the only crossings without a separator on T2, T3 and LL2 are, by (site kind, role, role): Fray (other leg, straight leg) and Join (joiner, lane) where the join is not a prio join pair. No other kind occurs.

## Changes
1. **Movers.** Replace `PlanarPrioJoiners` with `std::vector<uint32_t> PlanarMovers(const PlanarTopology &, const PlanarChannels &, const std::vector<PlanarResolvedSite> &)`: every joiner of every join (prio or not) and every other leg of every fray, in site order. Move the role logic the track map needs from the test's `RoleAt` into planar_regions.cpp as a file-local function (not in the header); the test keeps calling its own copy or the moved one through what the track map exposes, your choice of the two, stated in the report.
2. **Track map.** Rename the parameter `separator_joiners` to `separator_movers` (doc comment updated). For every crossing in a node group (`channels.group_crossings`) between a mover m and a keeper k of that site: m is the separator if m is in the set, otherwise k. Copies: a separator with `line_share >= 1.0` gets one copy over its run of such crossings at that site (a mover: over all its crossings with keepers; a keeper: over its crossings with movers not in the set), on the side the existing copy code uses for that site kind (for frays use the channel copy side rule `CopySide` if it applies; if no side rule fits, STOP and report). Prio joins keep their S2a behaviour exactly (rows too); this change adds frays and plain joins.
3. **Callers:** `PlanarPrioJoiners` users switch to `PlanarMovers`; S2a's "all joiners" form becomes "all movers".
4. **Wrapper:** `WrapToday` assigns these crossings their separator by the rule in change 2 and groups by copy as in S2a.

## Tests (red before green; record each failure line)
1. For every variant of T2, T3, LL2 and the two uniform forms (`{}` and all movers): the tally's "without separator" count is 0 and P3 is 0 (red today: 64/192/2688 without separator).
2. P6 stays 0 in both forms on all hubs; every Copy track's parent has a full share.
3. Identity for prio joins: with the set restricted to prio joiners, every prio join's tracks, copies and rows equal S2a's (compare track names and events per prio join; red cannot be shown since it is an identity: say so).
4. Print the tally for the first variant of each hub over all subsets of its movers if at most 8 movers, else say so.

## Do not
- Do not change channels, sites, `BuildPlanarPaths`, the composers, or the oracle.
- No grouping beyond the copy grouping, no cost, ranking or `BuildPlanarStructure` (S2c).

## Stop conditions
- A group crossing that is not mover x keeper.
- No existing side rule fits a fray copy.
- An existing test fails.

## Gates
- Release build, zero warnings in touched files; `tools/junction/jt fast build-rel '[planarstructure]' 5`, `tools/junction/jt fast build-rel '[planar]' 5`, then `tools/junction/jt gate build-rel` within 30 s (a lone SIGILL in an unrelated test that passes on rerun: rerun once and report it).
- Commit: `Change: Junction planar track map separator per mover (S2b)` with trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; functions added, renamed or removed; the tally lines; gate results; stops; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
