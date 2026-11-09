# O4b1: the six movements of a T routed together by negotiation (DESIGN-O4 revision 3, search step 1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4b1`, branch `wt-o4b1`, head in your launch prompt. You are the only writer there; never touch sibling worktrees or the main worktree. Never use `git stash`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. Timing runs alone, never beside a build or tests.
- Design: `docs/junction-tool/DESIGN-O4.md` revision 3 ("Movements", "Sharing", "The search" step 1), decisions D160-D162. The engineer's principle: nothing canned; the layout must emerge from rules, costs and the search.

## Why
Revision 3 routes every movement of the junction, the main-line through movements included, with the one line router under negotiated congestion (PathFinder, McMurchie & Ebeling): movements may share track where the rules allow (that is where splits and merges come from), and every other overlap is priced and raised each round until none is left. O4n gave the router `resource_cost` for this. This step builds the negotiation for a T on flat ground, without prios, rule feedback or signals (O4b2 adds those), and measures it.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`; DESIGN-O4.md revision 3; D160-D162.
- `src/junction_line_router.h` (whole), `src/junction_line.h` (`JunctionLineSpec`, `JunctionLineRoute`, `RasterizeJunctionLine`), `src/junction_route.h` (`JunctionPiece`, `JunctionRaster`, `AddRasterToLayout`), `src/junction_layout.h` (`JunctionLayout`, `JunctionBridge`).
- `src/junction_rules.h` (ports, `FindJunctionPorts`, `MakeJunctionCheckContext`, `CheckConnectivity`, `CheckGeometry`, `CheckCurveSpeed`, `CheckBridges`) and in `src/junction_rules.cpp` how `FindJunctionPorts` assigns roles and which movements the checker expects for a T (grep `expected`).
- `src/junction_generator.cpp` ~330-350 (the router settings the old generator derives from the spec: `curve_spacing`, `s_bend_window`, `turn_cost`, bridge prices and span) and `MakeTestJunctionSpec`.
- `src/junction_compact.h` (`MeasureJunctionCompactness`).

## The model (settled; implement exactly)
- **Ports.** Six `JunctionPort`s on the box edge, same roles and the same relative arrangement as today's generator produces: run `FindJunctionPorts(*GenerateJunction(MakeTestJunctionSpec()).layout)` once (a throwaway test or debug print, not committed), note for each role which edge, which row or column relative to the others, and its trackdir, and write `MakeOrganicTPorts` to reproduce that arrangement on a box of width W and height H with the two main-line tracks on rows 0 and `ml_gap + 1` running the full width and the two branch tracks on the bottom row (y = H - 1) at x = p and p + branch_gap + 1 (or whichever order the old layout uses; record it). If the old layout's arrangement can't be described this way (for example the branch is on the top edge), stop and report what it is.
- **Movements.** The movements the checker expects for a T (from `MakeJunctionCheckContext`'s expected list for these ports; it should be six: E and W through, and four turning). Each is one single-track route (`JunctionLineSpec{1, 1, 1}`) from its `from` port piece (start, inward trackdir) to its `to` port piece (goal, outward trackdir), bounds = the box.
- **Router settings** from the spec as the old generator derives them (curve_spacing, s_bend_window, turn_cost, rail_piece from `spec.prices`, bridges from `spec.prices.bridge` and `spec.bridge_span_ok`, `max_bridge_middle` as the old generator uses or 8 if it has none). Flat ground: terrain may be null (then no tunnels); `max_expansions` default.
- **Resources.** A resource is (tile, track bit, level, z) with level from `JunctionResourceLevel` (Surface, BridgeSpan, TunnelBore). Every routed piece occupies its resource. Two pieces of different movements **conflict** when they are on the same tile and level and z and either (a) the same track bit but not compatible, or (b) different track bits that overlap (`TracksOverlap` of the two bits) or cross; two BridgeSpan pieces on the same tile always conflict (bridges never cross bridges); two TunnelBore pieces on the same tile and z always conflict. Pieces are **compatible** (share for free) when they are the same track bit, same trackdir, same level and z, and the two movements have the same `from` port or the same `to` port.
- **Costs.** Unit U = `spec.prices.rail_piece`. Present factor pf starts at U and doubles each round. History h[resource] starts at 0 and grows by U each round in which the resource is in conflict. For movement m, `resource_cost(piece, z, level)` = h[resource] + pf x (number of other movements' pieces that conflict with this piece). Costs are >= 0.
- **Rounds.** Round 1 routes the movements in the expected-list order, each against the movements already routed in this round (so round 1 is sequential with prices). Each later round reroutes every movement in the same order against all the others' current routes. Converged when no two pieces conflict. Stop after `max_rounds` (parameter, default 16) without convergence. If any route fails (no route), the negotiation fails with that error.
- **Output.** On convergence: a `JunctionLayout` made by `AddRasterToLayout` of every movement's raster (shared pieces appear once; identical bridges once; check how `AddRasterToLayout` handles a duplicate and dedupe before adding if it doesn't), with the layout's size and ports consistent with what `FindJunctionPorts` and the checker expect (see how today's generator sets `size`).

## API (new files `src/junction_organic.h`, `src/junction_organic.cpp`, registered in `src/CMakeLists.txt` beside the other junction files; tests `src/tests/junction_organic.cpp` registered in `src/tests/CMakeLists.txt`)
```cpp
/** Ports of a T on a box of width x height (DESIGN-O4 revision 3): main-line tracks on rows 0 and ml_gap + 1, branch tracks on the bottom row from x = p. */
std::vector<JunctionPort> MakeOrganicTPorts(const JunctionSpec &spec, int width, int height, int p);

/** The outcome of routing a junction's movements together. */
struct JunctionNegotiation {
	std::optional<JunctionLayout> layout; ///< Set when the negotiation converged.
	std::vector<JunctionLineRoute> routes; ///< Each movement's final route, in the expected order.
	int rounds = 0; ///< Rounds run.
	int conflicts = 0; ///< Conflicting piece pairs after the last round (0 when converged).
	std::string error; ///< Why it failed, when it did.
};

/** Route every movement of a T together by negotiated congestion (DESIGN-O4 revision 3, search step 1; no prios, signals or rule feedback). */
JunctionNegotiation NegotiateJunctionMovements(const JunctionSpec &spec, const std::vector<JunctionPort> &ports, int width, int height, const JunctionTerrain *terrain = nullptr, int max_rounds = 16);
```
Everything else is `static` in the .cpp.

## Tests (red first: with the functions stubbed to return an empty result, record each failure), tag `[junction][organic]`
1. Outer: "NegotiateJunctionMovements: a T on flat ground converges": `MakeTestJunctionSpec()`, W = 44, H = 16, p = 20: converged, `conflicts == 0`, layout set; `CheckConnectivity`, `CheckGeometry` and `CheckBridges` on it (via `MakeJunctionCheckContext` with `FindJunctionPorts` and the T's expected movements, traced as the checker does) report no violations. INFO the rounds, the occupied/slivers from `MeasureJunctionCompactness`, and the moves of each route.
2. "NegotiateJunctionMovements: movements from one port share their first pieces": in test 1's result, the two movements leaving the E entry port share at least their first piece (same tile and trackdir), and likewise the two leaving the W entry port and the two leaving the branch entry port; the two movements arriving at each exit port share at least their last piece.
3. "NegotiateJunctionMovements: crossings go over or under": in test 1's result, no surface conflict exists (recheck independently in the test: no tile holds two overlapping or crossing track bits at grade), and at least one `JunctionBridge` exists (a T on flat ground can't connect all six movements without one).
4. "MakeOrganicTPorts: the old layout's arrangement": for MakeTestJunctionSpec, the roles, edges, relative rows/columns and trackdirs match what `FindJunctionPorts` finds on today's generated layout (as recorded above).
5. `[.bench]` "NegotiateJunctionMovements: timing": test 1's call timed with `std::chrono::steady_clock`, 1 warm-up and 11 timed runs, printing (via `std::cout`; `std::printf` is poisoned) one line: `BENCH organic_t rounds=<n> converged=<0|1> occupied=<n> slivers=<n> median_us=<n> max_us=<n>`; also the same for p = 14 and p = 26.

## Do not
- Add prios, signals, rule feedback, the D80 doubled-bridge move, terrain cases or a scoring loop (O4b2 and later).
- Change the line router, the checker, the generator or any existing file other than the two CMakeLists.
- Hand-place any track, split, merge or bridge: everything in the layout comes from the routes.

## Stop conditions
- The port arrangement can't be described as above.
- Test 1 doesn't converge in 16 rounds: report the conflicts left (tiles, movements), the rounds' conflict counts, and the moves; do not tune the cost schedule beyond what is written here.
- A checker group in test 1 reports violations that the model above can't avoid (report rule and message).
- Any existing test fails (beyond a lone hardware flake: rerun alone and report).

## Gates
1. Build: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[organic]" 8`, then `"[junction]~[slow]"`.
3. Timing test alone, three times; report the BENCH lines.
4. Commit: `Feature: Junction movements routed together by negotiated congestion`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. One commit.

## Report
References read; commit; red evidence per test; the recorded port arrangement; names added; the per-round conflict counts for test 1; ASCII of test 1's layout (`RenderJunctionAscii` if it takes a layout; say if not); BENCH lines (three runs); suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
