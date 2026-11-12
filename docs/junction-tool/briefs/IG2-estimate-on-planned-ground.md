# IG2: the in-game estimate judges terraformed tiles on the planned ground; the status names the game's error (RECON-in-game-red-flat.md)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-ig2 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes2: never touch it. Never start a game client or touch the engineer's game or rig servers.
- IG1 (docs/junction-tool/RECON-in-game-red-flat.md: read it whole; its harness is the hidden `[.ig1]` test in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/ig1.patch, which loads the 48 real fixtures into a [game] map and runs the GUI's path): 30 of 31 in-game plans have red tiles; 257 rail steps are diagonal half pieces on tiles the plan terraforms, refused by `TestRail` on the CURRENT slope (LAND_SLOPED_IN_WRONG_DIRECTION) while the planned slope is flat; 42 bridges have heads on terraformed ground (BRIDGEHEADS_NOT_SAME_HEIGHT / SLOPED_WRONG_DIRECTION). After applying the terraform with the game's command, 0 rail pieces stay red; the 10 bridges left fail BRIDGE_TOO_LOW_FOR_TERRAIN (a planner gap: IG3, not this step).
- EstimateJunctionTerraformed (src/junction_layout.cpp ~:462) runs EstimateJunctionImpl, which tests every rail/bridge step with the game's commands on the current map. The GUI (src/junction_gui.cpp ~:1210) calls it with the plan's height_cost; the steps are in hand there (ShowPlaced's `steps`). GetPlacementError (~:318) returns a generic STR_JUNCTION_ERROR_TILES_BLOCKED whenever there are bad tiles.

## Read first (once each)
CODINGSTYLE.md (C++ part); the RECON; src/junction_layout.{h,cpp} (JunctionPricing, EstimateJunctionImpl, EstimateJunctionTerraformed); src/junction_game.{h,cpp} GameJunctionPricing (TestRail, PriceBridge, and how JunctionTunnelPrice mirrors the tunnel command's cost); src/tunnelbridge_cmd.cpp CmdBuildBridge's cost part; src/junction_gui.cpp ~:96-150 (how the ghost computes planned corners from steps), ~:1200-1215, GetPlacementError.

## Changes
1. `JunctionPricing` gains two members, implemented in GameJunctionPricing and in every test pricing class:
   - `virtual Money RailPrice(Track track, bool foundation) const = 0; ///< The price of building one track piece on a tile with the planned ground, with a foundation or not, as CmdBuildSingleRail charges it (no terrain checks).`
   - `virtual Money BridgePrice(TileIndex start, TileIndex end) const = 0; ///< The price of the bridge as CmdBuildBridge charges it, without its terrain checks (as JunctionTunnelPrice mirrors the tunnel command).`
2. `EstimateJunctionTerraformed(layout, origin, pricing, height_cost, steps)` gains `const std::vector<JunctionTerraformStep> &steps`. A tile is "planned" when the steps move any of its corners. For a rail step on a planned tile: no command test; it is valid (the planner proved the piece on that ground: T3) and priced `RailPrice(track, GetRailFoundation(planned slope, all bits on the tile) != Foundation::None)`, the planned slope from the tile's current corners plus the steps' moves (the same corner arithmetic as the ghost, ~:131-145; if that needs a shared helper, it is one static function in junction_layout.cpp named `JunctionPlannedSlope`, and the ghost keeps its own code). For a bridge with either head or any span tile planned: no command test; priced with BridgePrice. Every other step is tested with the commands as today. The GUI passes the steps.
3. GetPlacementError: when `estimate.first_error` is set and not "too close to the edge", return it (the game's message) instead of the generic string; the generic string stays when there is none.
4. The `[.ig1]` harness becomes a committed `[game]` test "the in-game estimate of a terraformed plan has no red tile the terraform fixes" (tag `[junction][junctiongame]`; hidden `[.identity]` too if it takes over 5 s Release): on the 48 fixtures in a game map, PlanJunctionHoverNear + steps + EstimateJunctionTerraformed before the terraform: no bad rail tile; bad bridges only where, after applying the steps with the game's command, the bridge still fails (report their count and errors: expected the 10 BRIDGE_TOO_LOW ones, which IG3 fixes); and the estimate's total equals the sum the commands charge after the terraform (rail and bridges) within the foundation/clearing differences you document. Plus `[game]` unit tests: RailPrice == TestRail's cost on a flat and on a foundation tile; BridgePrice == PriceBridge's cost on flat ground.
5. TESTS.md rows.

## Red
At HEAD the committed fixture test fails with 257 rail bad tiles (record the count).

## Stop conditions
- A rail step on a planned tile whose piece GetRailFoundation refuses on the planned slope (that is a planner bug: report fixture, tile, bits, slope); the estimate total differs from the post-terraform command total by more than the documented differences; anything not covered.

## Gates
- `tools/junction/jt fast build '[junctiongame]' 5` (or the tag's budget), `'[hoverplan]' 5`; `tools/junction/jt gate build-rel` (count, time); the `openttd` target builds Debug and Release.
- One commit: `Fix: Junction estimate judges terraformed tiles on the planned ground and names the game's error`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the red/green counts, the remaining bad bridges, and what the engineer should see in game. When you have reported, stop.
