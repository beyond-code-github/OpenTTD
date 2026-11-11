# N3c: S-bend detours and the structure search (DESIGN-N3 §4; D182)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Model (settled by the orchestrator)
- A **state** is a layout (the flat T, possibly with detours inserted) plus its working offset (working = layout + offset). Its template is BuildJunctionNodeTemplate(state layout). All rules (curve gaps over the new curves, prio coverage, crossings, box edges) come from the template; nothing is added by hand.
- A **detour** replaces the interior of one free link p -> q (row r, dir d) by: a straight run on row r, an S-bend to row r + side, a straight run on row r + side, an S-bend back to row r, a straight run on row r ending at q. In the next solve the S-bends are rigid classes and the three runs are free links, so the positions solve moves and stretches them (DESIGN-nodes §4b: an S-bend is a pattern of two opposite 45-degree curves).
- This step realises a shift as a detour within one free link (shift out and back before the next point). Staying shifted past a node (D182) is a later repair kind, added only when a measured case needs it. Write this sentence into DESIGN-N3 §4 as the step's scope note.

## State
Worktree `/home/pete/OpenTTD/.claude/n3c`, branch `wt-n3c`, head given at launch. Create `build` (Debug) and `build-rel` (Release) (clang-18, ccache); build `openttd_test` only. Tests through `tools/junction/jt`: `'[nodes]'` (budget 1 s Release, 5 s Debug); `jt gate build-rel` once before the commit. Output in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n3c/`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `docs/junction-tool/DESIGN-N3.md`; `src/junction_nodes.h`, `src/junction_nodes.cpp`; `src/junction_route.h` (JunctionRoute, RasterizeJunctionRoute, AddRasterToLayout, NormaliseJunctionLayout, JunctionNextTrackdir); `src/tests/junction_nodes.cpp` (helpers, N3a test 4's hand S-bend "FFFLFRFFFF", N3b test 3 "a house beside the hub fails on A's run"); `src/tests/junction_map.cpp` 556-590 and 998-1008 (the house and hub-tile cases).

## Additions to `src/junction_nodes.h` / `.cpp`
```cpp
/**
 * Replace the interior of a free link of a state layout by a detour around one column (DESIGN-N3 §4).
 * @param layout The state layout (not normalised by this function).
 * @param tpl Its template.
 * @param link A free link of tpl.
 * @param column The layout x the detour's shifted run must cover.
 * @param side +1 or -1: the row the shifted run is on, relative to the link's row.
 * @param why On failure, receives the reason.
 * @return The new layout, or std::nullopt.
 */
std::optional<JunctionLayout> InsertJunctionDetour(const JunctionLayout &layout, const JunctionNodeTemplate &tpl, uint32_t link, int column, int side, std::string *why);

/** The result of a structure search. */
struct JunctionNodeSearch {
	std::optional<JunctionLayout> layout; ///< Rule-clean, fitting and with connected heights.
	int states = 0; ///< States solved.
	std::string log; ///< One line per state: "state <n> detours <k>: <ok | failure kind link column | template error | rules: <first violation> | heights: <why>>".
};
/** Best-first search over detours from the flat layout (DESIGN-N3 §4): at most max_states states solved. */
JunctionNodeSearch SearchJunctionNodes(const JunctionLayout &flat, const JunctionSpec &spec, const JunctionTerrain &terrain, TileIndexDiffC offset, int max_states);
```

### InsertJunctionDetour, exactly
1. Let p, q be the link's points, r their row, d the link's direction (the dir of a path containing it). The link's interior is the X pieces strictly between p and q on row r. Fail ("column outside the link") unless column is strictly inside.
2. Turn hands: rasterize the hand pattern `F L F R F` from p's piece (JunctionRoute{p.tile, p.trackdir, ...}); if its last piece is on row r + side, then T1 = L, T2 = R; else T1 = R, T2 = L (check the other gives r + side; else fail "no S-bend to that side").
3. The program from p is `F^n1 T1 F T2 F^n2 T2 F T1 F^n3` (n1, n2, n3 >= 0; the leading F leaves p). Choose n1 as large as possible such that no piece of the program on row r lies at `column` or beyond it (in travel order) before the second S-bend; then n2 as small as possible such that the shifted run covers `column`; then n3 so that the last piece of the program is q's tile with q's trackdir. Search n1 down from the interior length and n2 up from 0, rasterizing each candidate (it is a handful of tries). No solution -> fail ("link too short for a detour").
4. New layout = the old one with Track::X removed from every interior tile of the link (remove only that bit; a tile left with no bits is removed), then AddRasterToLayout of the program's raster without its first piece (p) and last piece (q). Fail ("collision at x,y") if any added piece lands on a tile that, after the removal, already holds a track.

### SearchJunctionNodes, exactly
- facts = JunctionFacts(terrain). Queue (FIFO within equal detour count; lower detour count first): start with {flat, offset, 0 detours}. Visited set of states by their sorted track list.
- For each popped state, while states < max_states: tpl = BuildJunctionNodeTemplate; error -> log, next. solve = SolveJunctionNodes(tpl, spec, facts, state offset). states++.
  - layout found: let `shift` be how far SolveJunctionNodes' normalisation moved it (derive from the anchor class's point, as the N3b tests' FlatToWorking does; if the anchor is UINT16_MAX, stop and ask); accept iff CheckJunctionRules(layout, spec) is empty and JunctionNodeHeightsOk(layout, spec, terrain, offset - shift, ...) and every track tile fits (JunctionNodeTrackFits at working coordinates). Accepted -> return it. Otherwise log the first rule violation or `why` and go on.
  - failure Straight or Break (link L, working column c): children, side -1 first then +1: InsertJunctionDetour(state layout, tpl, L, c - state offset.x, side). A child layout is normalised with NormaliseJunctionLayout; its offset is the parent's offset plus the normalisation shift (the shift NormaliseJunctionLayout applied, computed from the minimum track/bridge x and y before normalising, as FinishJunctionCandidate does). Detour count + 1. Skip visited. A failed insertion is logged.
  - other failures: logged, no children.
- Return with layout empty when the queue is empty or max_states is reached.

## Tests, red first (`src/tests/junction_nodes.cpp`, `[junction][nodes]`). Red: stubs return std::nullopt / empty search; record.
Outermost first:
1. "the node search builds around a house beside the hub": MakeMapTestTerrain, BlockMapTestTile(frame.hub_x - 1, 0) (N3b test 3's terrain); search from the flat T with its offset, max_states 16. REQUIRE layout (INFO log); CHECK states <= 16; WARN states and the log. (The acceptance inside the search already checks rules, fits and heights.)
2. "the node search builds around an unbuildable hub tile": as 1 with BlockMapTestTile(frame.hub_x, 0).
3. "the node search keeps the flat T on flat terrain": no blocks; CHECK states == 1 and the layout equals the flat layout.
4. "a detour around a column of the hand-built line" (inner): a hand layout of one straight X line (JunctionRoute from (0,2), Trackdir::X_SW, 14 F) plus whatever second line N3a test 4 used to get traced paths (copy that construction with a 14-tile line); tpl; the free link of the long run; InsertJunctionDetour around column 7 with side -1: REQUIRE a layout; CHECK no track on (7, 2), tracks on (7, 1), and ExtractJunctionChains of the result has a path with 4 curves; with side +1: CHECK tracks on (7, 3). Column 0 (outside the interior): nullopt with "column outside the link".
5. "a detour collides with a parallel line" (inner): a hand layout with a second straight line on row r + 1 over the column: side +1 gives nullopt with "collision".

## Do not
No change to SolveJunctionNodes, the template rules, junction_facts, junction_positions, the generator, the router or the checker. No staying shifted past a point. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Test 1 or 2 finds no layout within 16 states: report the full search log verbatim (a measurement: which rule or fact blocks the detour). Do not add repairs or rules.
- The S-bend hand pattern (step 2) does not produce a one-row shift with two opposite curves: report the raster.
- OVER BUDGET for `[nodes]` (if CheckJunctionRules inside the search makes it slow, report the time per state).
- Anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `jt gate build-rel`. One commit on `wt-n3c`: `Feature: Junction node search with S-bend detours (DESIGN-N3, D182)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; added members; for tests 1 and 2: states, the accepted state's detours (link, column, side) and the search time (WARN it with std::chrono); `[nodes]` times; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
