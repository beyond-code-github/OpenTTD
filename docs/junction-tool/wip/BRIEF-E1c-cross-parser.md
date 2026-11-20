# BRIEF E1c: the topology parser accepts authored crossings (`cross`, D389)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-e1, head HEAD_HASH (check with `git log -1`). Builds: `build-rel` (Release) there is current to wt-cells; rebuild it incrementally, never configure a new build dir.
- Plan: docs/junction-tool/PLAN-sss-v2.md E1; ruling D389 (1) (docs/junction-tool/DECISIONS.md); card docs/junction-tool/wip/CARD-E1-crossings.md.
- Budget: 60 minutes in total; any single test run over 60 s is a stop.

## Read first (once each, with offset/limit)
- CODINGSTYLE.md (C++ part).
- docs/junction-tool/DECISIONS.md rows D389 and D390 (`grep -n '^| D389\|^| D390'`).
- src/junction/planar/planar_topology.h (whole, 110 lines) and src/junction/planar/planar_topology.cpp lines 60-436 (`PlanarBuilder`, `Derive`, `ParsePlanarTopology`).
- src/tests/junction/planar/planar_topology.cpp (whole: its `Parse` helper and test style).
- src/junction/planar/CLAUDE.md section "## Topology (`planar_topology`)".
- docs/junction-tool/TESTS.md lines 1-8 (how to run `jt`).

## The data file
- `git mv docs/junction-tool/wip/topologies/psg197.txt tools/junction/topologies/psg197.txt` (the canonical copy from now on; it already holds the `cross` statements from E1b).
- Replace every reference to `wip/topologies/psg197.txt` in docs/junction-tool/**/*.md and src/junction/**/CLAUDE.md with `tools/junction/topologies/psg197.txt` (`grep -rl` then `sed -i`; list the files touched in the report).
- src/tests/CMakeLists.txt: below the `JUNCTION_TERRAIN_DIR` definition add the comment `# The authored topology files (tools/junction/topologies).` and `target_compile_definitions(openttd_test PRIVATE JUNCTION_TOPOLOGY_DIR="${CMAKE_SOURCE_DIR}/tools/junction/topologies")`.

## Tests (src/tests/junction/planar/planar_topology.cpp, tags `[junction][planar]`), outermost first
Write all of them first, build, run, and record each red message before changing the parser. Expected red for tests 1-3: the parse fails with "unknown statement 'cross'"; test 4 is a characterisation and is green from the start (say so) (an assertion on the error string or on `crossings_authored`, not a compile error; add the `crossings_authored` member to the header first so the tests compile).
1. **"planar: PSG 197 parses with its authored crossings"**: read `JUNCTION_TOPOLOGY_DIR "/psg197.txt"` with `std::ifstream` into a string; parse; REQUIRE the error is empty; `crossings_authored` is true; `lines.size() == 101`; `crossings.size() == 121`. Then, from the same text, collect each `cross L: M1, M2, ...` statement (split at ':' and ','; trim) and REQUIRE, for each, that the names of the other lines of `events[L]` in order equal M1, M2, ... (use `PlanarLineByName`).
2. **"planar: authored crossings equal to the derived ones parse to the same topology"**: parse `PlanarTText(1)` (derived). Build its authored text: the same text plus, for every line with a non-empty `events` entry, one statement `cross <line name>: <other line names in event order>`. Parse that. REQUIRE no error, `crossings_authored` true on it and false on the derived one, the same `crossings` (a, b) and the same `events`. Repeat the whole check for `PlanarTText(2)` in the same test.
3. **"planar: authored crossings are refused with a reason"**, on the authored `PlanarTText(1)` text from test 2 (build it with a small static helper `AuthoredText(const std::string &text)` in the test file, used by tests 2 and 3), each SECTION changing only the `cross` statements:
   a. a name that is no line (`cross <line>: Nope` replacing that line's statement's first name): error contains "cross names unknown line Nope";
   b. a second statement for a line that already has one: "has two cross statements";
   c. a line listing itself: "crosses itself";
   d. a line listing one partner twice: "twice";
   e. one partner removed from one line's list only: "is listed by";
   f. one pair removed from both lines' lists: error contains "times but their ends interleave";
   g. one pair added to both lists where the derived topology has no crossing: loop over line pairs (a < b) with no derived crossing, neither a port line, add `b`'s name at the end of `a`'s list and `a`'s at the end of `b`'s (creating a statement if the line has none), parse, and take the first whose error contains "do not interleave"; REQUIRE one exists.
4. **"planar: a topology without cross statements is not authored"**: `PlanarLL2Text()` (from planar_plan.h) parses with `crossings_authored` false and the same crossings count as before your change (read it from a run before the change and write the number in the test).

## The changes
**planar_topology.h**
- `PlanarTopology` gains, after `events`: `bool crossings_authored = false; ///< D389: crossings and event order come from `cross` statements; every order is pinned.`
- The comments of `crossings` and `events` change from "Derived:" to "Derived, or authored (D389):".
- The `ParsePlanarTopology` comment gains, after the `prio` bullet: `` - `cross <line>: <line>, ...` every crossing of the line in travel order (D389); with any `cross` statement every crossing is authored, each pair listed by both its lines, and the set must keep parity: two paths of different routes sharing no line cross an odd number of times exactly when their ends interleave on the ring. ``

**planar_topology.cpp**
- `PlanarBuilder` gains one member: `std::vector<std::pair<std::string, std::vector<std::string>>> cross; ///< The `cross` statements as authored: line name, partners in travel order.`
- `ParsePlanarTopology`: `word == "cross"`: split `rest` at the first ':' (no ':' is the error "cross statement without ':'"), trim the name, `SplitList` the rest, push to `b.cross`. Names are resolved in `Derive` (lines may be named later in the text).
- `Derive`: unchanged up to and including the `must` set. Then, when `!this->cross.empty()`, an authored branch that returns at its end (the candidate, greedy and chord-order code below runs only without `cross` statements). The authored branch, in this order, each check setting `this->error` and returning on the first breach:
  1. Resolve names with `this->line_index`: unknown → `fmt::format("cross names unknown line {}", name)` (for the statement's own line and its partners).
  2. Per line at most one statement: `"line {} has two cross statements"`.
  3. No line in its own list: `"line {} crosses itself"`; no partner twice in one list: `"line {} crosses {} twice"`.
  4. Every pair listed by both lines: for each L listing M, M has a statement listing L, else `"crossing {} x {} is listed by {} but not by {}"` (L, M, L, M).
  5. Crossings: one `PlanarCrossing{min, max}` per pair; sort by (a, b) as the derived branch does.
  6. Parity: for every two paths i < j with different routes and no shared line, `k` = the number of crossings with one line on path i and the other on path j; `need = must.contains({i, j})`; when `(k % 2 == 1) != need`: `fmt::format("paths {}..{} of {} and {}..{} of {} cross {} times but their ends {} on the ring", first line name, last line name, route name, ..., k, need ? "interleave" : "do not interleave")`. Note the derived `interleave` lambda already returns false for same-route or line-sharing pairs; parity needs the separate condition "different routes and no shared line" to choose which pairs to check.
  7. Events: `t.events.assign(t.lines.size(), {})`; for each statement, `events[L]` = the crossing index of (L, M_k) for each partner in authored order. Set `t.crossings_authored = true`.

**src/junction/planar/CLAUDE.md**, section Topology: keep the existing bullets (they describe derivation) and add after the "Event order" bullet one bullet: `- Authored crossings (D389): a `cross L: M1, M2, ...` statement lists every crossing of L in travel order; with any such statement derivation is off, each pair is listed by both lines, every order is pinned (`crossings_authored`), same-route and port-line crossings are allowed, and the parser refuses a set that breaks parity (two paths of different routes sharing no line cross an odd number of times iff their ends interleave). PSG 197 is authored this way (tools/junction/topologies/psg197.txt, 121 crossings).` Also add "`cross <line>: <line>, ...`" to the first bullet's list of statements.

## Do not
- Do not change the derived branch's behaviour, the ring, `PlanarRoutePaths`, or any other planar file.
- Do not make Structure, channels or anything downstream read `crossings_authored` (a later step does).
- Do not add a second test file, a fixture directory beyond tools/junction/topologies, or a helper in src/junction.
- Do not edit psg197.txt's content.

## Stop conditions
- PSG 197 fails a check (report the exact error; do not edit the data).
- Any existing test changes result.
- A test is red for a reason other than the one named.

## Gates and commits
- `cmake --build /home/pete/OpenTTD/.claude/ng1/build-rel --target openttd_test > <scratch>/build.log 2>&1`; grep it for `error|warning`.
- `/home/pete/OpenTTD/.claude/ng1/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel planar 30`, then `.../tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel` (budget 30 s; report the time).
- Commit 1: `Codechange: Junction test build: JUNCTION_TOPOLOGY_DIR for authored topology files` (CMakeLists and the git mv with reference updates).
- Commit 2: `Add: Junction planar topology: authored crossings (cross statements, D389)` (header, cpp, tests, CLAUDE.md).
- Both end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not push, do not merge.

## Report (short, this format)
- References read
- Commits: hash and subject
- Red evidence: each test and the failure message seen before the parser change
- Types and members added or removed: names only
- Suite results: `[planar]` count and time, gate count and time
- Stops, and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
