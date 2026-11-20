# BRIEF E1f-2: authored crossing directions (parser and line sides, D389, G3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-e1, head HEAD_HASH (check `git log -1`). Release build `build-rel` (incremental).
- **Load: build `-j 4`, `JT_JOBS=4`; never two builds or test runs at once.**
- Design: docs/junction-tool/wip/CARD-E1d-psg197-variant.md sections "Measured" (G3) and "G3 design". Budget 75 minutes; any single test run over 60 s is a stop.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part).
- src/junction/planar/planar_topology.h (whole) and planar_topology.cpp: `PlanarBuilder`, the authored branch of `Derive`, the `cross` parsing in `ParsePlanarTopology`.
- src/junction/planar/planar_regions.h lines 35-90 (`PlanarLineSides`, `CrossingFrom`: +1 when the line arrives from the LEFT of the other line's travel) and planar_regions.cpp lines 60-135 (the rotation at each crossing, lines 112-126).
- src/tests/junction/planar/planar_topology.cpp (its `AuthoredText`, the authored tests) and src/tests/junction/planar/planar_structure.cpp lines 700-800 (its copy of `AuthoredText`, the loader tests).
- docs/junction-tool/wip/probes/e1/psg197-cross-marked.txt (53 lines).

## Notation
In a `cross L: ...` statement each partner carries a direction mark, no space: `M>` means M arrives from L's left (crossing to L's right), `M<` from L's right. With authored crossings every partner must carry a mark, and the two marks of a pair are opposite.

## Changes
**planar_topology.h**: `PlanarTopology` gains, after `crossings_authored`: `std::vector<bool> b_from_left; ///< Authored crossings only (D389): per crossing, line b arrives from line a's left (its mark in a's `cross` statement is '>').` The `cross` bullet of the `ParsePlanarTopology` comment adds: "each partner carries a direction mark, `M>` (M arrives from the line's left) or `M<` (from its right); a pair's two marks are opposite".
**planar_topology.cpp**, authored branch of `Derive`, before resolving names: strip one trailing `>` or `<` from every partner token and keep it; a partner without one: error `fmt::format("cross {}: partner {} has no direction mark (> or <)", L, M)`. After the "listed by both" check: for each pair, the mark of b in a's statement and of a in b's statement must differ, else `fmt::format("crossing {} x {}: both lines give the same direction", a name, b name)`. Fill `t.b_from_left` (size = crossings) after sorting the crossings.
**planar_regions.cpp**, `PlanarLineSides`, the loop at lines 112-126: when `t.crossings_authored`, `b_in_left = t.b_from_left[c]` and the "ends do not interleave" check is skipped; otherwise unchanged.
**tools/junction/topologies/psg197.txt**: replace its 53 `cross` lines with the 53 lines of psg197-cross-marked.txt, in the same place; nothing else changes (`git diff` shows only those lines).
**CLAUDE.md** (src/junction/planar): the "Authored crossings (D389)" bullet in "## Topology" adds the marks and the opposite rule; "## Line sides (`planar_regions`)" gains: `- With authored crossings the side each line arrives from at a crossing is the authored mark (D389, `b_from_left`), not the ring interleave.`

## Tests, outermost first; record each red before the code
1. **"planar regions: PSG 197's line sides are decided by its authored directions"** (planar_regions test file, tags `[junction][planar]`): parse psg197.txt from `JUNCTION_TOPOLOGY_DIR`; `PlanarLineSides sides(t, BuildPlanarChannels(t));` REQUIRE `sides.Why()` empty. Red before: the parse fails on the marks ("unknown line ..." with a mark) once psg197.txt has them; record it. If after the change `Why()` is not empty, that is a stop: report the message (it is data or design, not yours to fix).
2. Both `AuthoredText` helpers (planar_topology.cpp and planar_structure.cpp tests) append each partner's mark: `>` when `PlanarLineSides(t, BuildPlanarChannels(t)).CrossingFrom(c, partner) == +1` on the derived topology `t`, else `<`. In planar_topology.cpp's test "authored crossings equal to the derived ones parse to the same topology", add: for every crossing c, `authored.b_from_left[c] == (sides.CrossingFrom(c, derived.crossings[c].b) == +1)`, and the authored topology's `PlanarLineSides` gives the same `CrossingFrom` for every crossing and both lines and the same `Faces().size()`. Repeat that test for `PlanarLL2Text()` beside T1 and T2.
3. Refusals (planar_topology.cpp, "authored crossings are refused with a reason"): new SECTIONs "a partner without a mark" ("no direction mark") and "a pair with the same mark on both sides" ("same direction"). Existing SECTIONs that add or edit names keep marks valid (a name added to both lists gets `>` in one and `<` in the other).
Red for 2 and 3 before the parser change: "unknown line" with the mark in the name, or the missing member; a compile error is not an acceptable red (add the member first).

## After the tests are green
Run the loader characterisation by name: `/home/pete/OpenTTD/.claude/ng1/build-rel/openttd_test "planar structure: PSG 197's pinned variant\, gaps and promise"` and keep its full output (the new gap list).

## Do not
- Do not change the derived path's behaviour, the loader, Structure, or any other layer. Do not edit the marks in the data.

## Stop conditions
- Test 1's `Why()` not empty after the change; any existing test changing result; a red for another reason.

## Gates and commit
- `cmake --build ... -j 4 --target openttd_test`, `JT_JOBS=4 tools/junction/jt fast build-rel '[planar]' 30`, `JT_JOBS=4 tools/junction/jt gate build-rel` (report count and time; at 4 jobs ~31 s against 30 s is expected and not a stop).
- One commit: `Add: Junction planar: authored crossing directions (cross marks, D389)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not push or merge.

## Report (short)
References read; commit; red evidence per test; members added; suite results; the loader characterisation output in full; stops; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
