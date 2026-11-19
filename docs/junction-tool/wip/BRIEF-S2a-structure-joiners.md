# BRIEF S2a: separator per joiner, copies by share, oracle corrections

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-structure, head HEAD_HASH. Release build build-rel (current). Logs: /tmp/claude-1000/-home-pete-OpenTTD/83ff9380-9534-4138-8dc1-79a833146e49/scratchpad/s2a/.
- Implements docs/junction-tool/wip/SPEC-structure.md §4 (P3 corrected, P5, P6) and §7 S2 plan items (a) and (c), plus the joiner half of (b): D364 (one separator bit per joiner), D331/D332 (only full-share lines double), D228a (flow bound, B <= 2·sd + tl). The spec is the source of truth (D368).

## Read first (once each, with offset/limit)
1. docs/junction-tool/wip/SPEC-structure.md (whole).
2. src/junction/planar/planar_structure.h and .cpp (yours from S1: the oracle).
3. src/tests/junction/planar/planar_structure.cpp (S1's tests, `WrapToday`, the tally).
4. src/junction/planar/planar_regions.h lines 95-115 and planar_regions.cpp lines 375-400 (`JoinLanes`) and 546-660 (`BuildPlanarTrackMap`: rows, then join copies with `joiner_over`, then channel copies).
5. src/junction/junction_spec.h lines 35-50 (`tl`, `sd`).

## Facts you need (verified by the orchestrator)
- In `BuildPlanarTrackMap`, a prio join's crossings are the pairs (lane i, joiner j) with i > j (lanes and joiners from `JoinLanes`, nearest first). Today `joiner_over == false` makes lane i the separator over joiners j < i and copies lane i with NO share test; `joiner_over == true` makes joiner j the separator over lanes i > j and copies it only if `channels.line_share[joiner] >= 1.0`.
- D364: the choice is per joiner, not per hub.

## Changes
1. **Track map, per-joiner form.** Replace `bool joiner_over` in `BuildPlanarTrackMap` (header and source, doc comment updated) with `const std::set<uint32_t> &separator_joiners`: the topology lines of the joiners that are separators. For each join, each pair (lane i, joiner j), i > j: if joiner j is in the set, joiner j is the separator of that crossing; otherwise lane i is. Copies: a joiner in the set gets a copy over its crossings with lanes i > j only if its share is full (as today); lane i gets a copy over its crossings with joiners j < i that are NOT in the set, only if `channels.line_share[lane i] >= 1.0` (new: the share test). Rows unchanged.
2. **A helper to list joiners:** `std::vector<uint32_t> PlanarPrioJoiners(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites)` in planar_regions.{h,cpp}: every joiner line of every prio join, in site order then nearest first (from `JoinLanes`).
3. **Existing callers** (7 in src/tests, grep `BuildPlanarTrackMap(`): `false` becomes `{}`; `true` becomes a set of `PlanarPrioJoiners(...)` of that variant; a variable `joiner_over` is mapped the same way. No other change to those tests; if any of their assertions then fails, STOP and report it (it would mean a test relied on a lane copy with a partial share).
4. **Oracle** (planar_structure.cpp):
   - P3: a crossing may be in two structures exactly when each of its two tracks is the separator of one of them; otherwise exactly one.
   - P5: also, for a structure with a copy, `crossers.size() + 2 <= 2 * spec.sd + spec.tl` (D228a: head-to-head length at one middle tile per crosser); message names D228a.
   - P6: also, each Copy track is the copy of exactly one structure.
5. **Characterisation wrapper:** `WrapToday` takes the separator set and assigns join crossings their separator by the rule in change 1 (the track that change 1 makes separator). Enumerate per variant: `{}` and all joiners (today's two forms), and, for the first built variant of each hub only, every subset of its joiners if there are at most 6 joiners, else say so. Keep the existing tally lines; add, per hub, a classification of the crossings still WITHOUT a separator: for each, the kind of the node group it lies in (`PlanarSiteKind` of the site whose group holds it, via `channels.group_crossings`) and, for each of its two lines, its role at that site (in, straight leg, other leg, lane, joiner, or none), printed as counts per (kind, role, role) with one example by line names.

## Tests (red before green; record each failure line)
1. P3 exception: S1's bridge-over-tunnel test variant now gives NO P3 message (today it does: red first).
2. P5 D228a: a mutation of S1's base case giving a structure a copy and more crossers than 2·sd + tl − 2 gives exactly one P5 message naming D228a.
3. P6 one copy per structure: two structures naming the same copy give exactly one P6 message.
4. Copies by share: for every variant of T2, T3 and LL2, with separator set `{}`, every Copy track's parent line has `line_share >= 1.0` (red today: S1's tally shows P6 breaches in this form on T2 and T3).
5. The characterisation's P6 count is 0 in both forms on all hubs, and its P3 count now counts only the crossings outside prio joins (assert nothing about P3; print).

## Do not
- Do not assign separators to non-join site crossings (that is S2b, designed from your classification).
- Do not change channels, sites, `BuildPlanarPaths` or the composers.
- Do not implement grouping, cost, ranking or `BuildPlanarStructure`.

## Stop conditions
- An existing test fails after change 3.
- A join has a crossing pair that is not (lane i, joiner j) with i > j.
- `spec.sd` is not the main line's signal spacing (read its doc comment); then leave D228a out and report.

## Gates
- Release build, zero warnings in touched files. `tools/junction/jt fast build-rel '[planarstructure]' 5`, `tools/junction/jt fast build-rel '[planartracks]' 5` (check the tag name used in planar_tracks.cpp first), then `tools/junction/jt gate build-rel` within 30 s.
- Commit: `Change: Junction planar track map separator per joiner, copies by share (S2a)` with trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence per test; functions and members added or changed (names); the tally lines and the classification of crossings still without a separator (copy them); gate results; stops; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
