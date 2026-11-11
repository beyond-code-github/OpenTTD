# CK1: the checker leaves search acceptance; an oracle sweep keeps it honest (D193)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug build `build`, Release `build-rel`; builds are stale, an incremental build is expected). First: `git checkout -B wt-ck1 <HEAD>` (HEAD given at launch). You are the only writer there.
- Why: D193 (docs/junction-tool/DECISIONS.md) "correct by construction; the checker is a test oracle only". Measured (docs/junction-tool/DIAG-node-shortfalls.md Q3): over 259 search states on flat 100-196 km/h, house, hub tile and 200 random block terrains, CheckJunctionRules rejected 0 of 66 candidates; it costs ~8.4 ms per call (Release) against ~1.4 ms per rendering solve.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (skim the C++ part).
- docs/junction-tool/TESTS.md (tags, budgets, `tools/junction/jt`).
- src/junction_nodes.h :170-195 (JunctionNodeSearch, SearchJunctionNodes) and src/junction_nodes.cpp SearchJunctionNodes (~:1351-1440).
- src/junction_game.cpp :400-470 (GenerateJunctionNear's per-offset search; the start offset and JunctionBlockedPortApproach).
- src/tests/junction_game.cpp :370-430 (MakeNearTestTerrain, SetNearTestRect, near tests) and :500-560 (SetNearRampRow, ridge and 196 km/h tests).

## Tests (outermost first)
1. New TEST_CASE in src/tests/junction_game.cpp, after the 196 km/h near test: `"the node search's layouts pass the rule checker on random terrain"`, tags `"[junction][nodeoracle]"` (in the wide gate; not a per-step fast tag). Body:
   - `spec = MakeTestJunctionSpec()`; `topo = JunctionNodeTopologyT()` (src/junction_generator.h); `REQUIRE(topo.layout.has_value())`; `frame = MakeJunctionFrame(spec)`; `start_offset = {frame.hub_x - topo.hub.x, -topo.hub.y}` exactly as GenerateJunctionNear computes it.
   - 200 block terrains: for seed 1..200, `std::mt19937 rng(seed)`; `terrain = MakeNearTestTerrain()`; 1-3 rectangles (count uniform 1..3), each width and height uniform 1..3, x0 uniform in [start_offset.x, start_offset.x + topo.layout->size.x - 1], y0 uniform in [0, topo.layout->size.y - 1]; `SetNearTestRect(terrain, x0, x0 + w - 1, y0, y0 + h - 1, JunctionTileClass::Blocked)`.
   - 50 ridge terrains: for seed 1001..1050, `terrain = MakeNearTestTerrain(); terrain.build_on_slopes = true;` ridge row r uniform in [2, topo.layout->size.y - 4]; the same four SetNearRampRow calls as the ridge test, at r .. r+3.
   - For each terrain: skip (count it) when `JunctionBlockedPortApproach(terrain, start_offset.x, start_offset.x + topo.layout->size.x - 1)` has a value. Otherwise `search = SearchJunctionNodes(*topo.layout, spec, terrain, start_offset, 16)`; when `search.layout` has a value: `violations = CheckJunctionRules(*search.layout, spec)`; `INFO` the seed and every violation (rule, tile, message) and `CHECK(violations.empty())`.
   - At the end print one line `std::cout << "ORACLE terrains=" << n << " skipped=" << skipped << " accepted=" << accepted << " micros=" << total_search_micros << "\n";` and `CHECK(accepted > 0)`.
   This test passes before the code change (the checker is still inside acceptance, so every accepted layout is clean by construction of the search). It is the regression net, not the red test. Record its output line before the change.
2. Red test, in src/tests/junction_nodes.cpp next to the existing SearchJunctionNodes tests (grep "SearchJunctionNodes" there), tag `[nodes]`: `"the node search does not run the rule checker"`. It asserts that a flat 196 km/h search's `log` contains no "rules:" line and that the new member `JunctionNodeSearch::checked` (see Changes) is 0 after a successful search on flat land. Before the change it fails on `checked` being 1 (it does not compile until the member exists; add the member first with the counter increment at the existing CheckJunctionRules call so the red failure is the assertion `checked == 0` with value 1).

## Changes
- src/junction_nodes.h, struct JunctionNodeSearch: add `int checked = 0; ///< Times the rule checker ran inside the search (D193: none; it is a test oracle).`
- src/junction_nodes.cpp, SearchJunctionNodes: first (red step) add `search.checked++;` before the CheckJunctionRules call. Then (green step) delete the CheckJunctionRules call, its `violations` block and the `search.checked++;` line. Heights and fits stay. Update the JunctionNodeSearch::layout doc comment from "Rule-clean, fitting and with connected heights." to "Fitting and with connected heights; rule-clean by construction (D193; the [nodeoracle] test checks it)."
- Remove the `#include` of the rules header from junction_nodes.cpp only if nothing else in the file uses it (grep `CheckJunctionRules\|RuleViolation`).
- docs/junction-tool/TESTS.md: add a row for `[nodeoracle]` with the Release time you measure via `tools/junction/jt fast build-rel nodeoracle 30` (Debug: "gate only, not measured").

## Do not
- Do not change any other test's expectations or tags. Do not remove CheckJunctionRules from any test. Do not touch GenerateJunctionNear or the GUI.
- Do not add terrain kinds other than the two listed.

## Stop conditions
- The oracle test fails before your change (a rejected layout with the checker inside acceptance cannot happen; if it does, report the seed and message).
- The oracle test fails after your change: report every failing seed with its violations and the search log (first 20 lines) and STOP without committing; do not try to fix the solve.
- `[nodeoracle]` takes over 30 s Release.

## Gates
- `tools/junction/jt fast build nodes 5` (Debug) and `tools/junction/jt fast build-rel nodes 1`.
- `tools/junction/jt fast build-rel tmapnear 5`.
- `tools/junction/jt gate build-rel` (budget 120 s).
- Commit: `Change: Junction node search accepts without the rule checker (D193)` with the body naming the oracle line before and after, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commits (hash, subject); red evidence (the failure message seen); types and members added/removed; suite results with times; the ORACLE line before and after; stops; `git status --porcelain` and `git log -1` in .claude/nodes. When you have reported, stop.
