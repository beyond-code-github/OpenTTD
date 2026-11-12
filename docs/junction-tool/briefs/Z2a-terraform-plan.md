# Z2a: a terraform plan on a terrain snapshot: corner targets, the game's propagation, cost, applied terrain (D190, D196, D218, D219; DESIGN-G1 §15)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-z2a <HEAD>` (given at launch). Never use git stash.
- D218 (read it): heights are costs, not failures; terraform is allowed by default. No junction code models terraform yet. This step adds the pure-data model the height solve (Z2b) will produce and the tests will check: a set of target corner heights, propagated as the game propagates a terraform, priced, and applied to a JunctionTerrain copy so T3's facts can verify a solved placement on the terraformed ground.
- JunctionTerrain (src/junction_terrain.h) stores per tile z_min, z_max, steep, incline, slope (canonical frame, D152), class, clear_cost; the snapshot is in the canonical frame (corners rotated/mirrored with it).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; DECISIONS.md D190, D196, D218; DESIGN-G1.md §15.
- src/junction_terrain.h whole; src/junction_game.cpp FillJunctionTerrainCell and MakeJunctionTerrain (how slope, z_min, z_max, incline and class are filled; the canonical corner order).
- src/terraform_cmd.cpp (CmdTerraformLand: which corners it raises, how it propagates to neighbouring corners, which tiles stop it, how it prices: PR_TERRAFORM per corner changed, clearing) and src/slope_func.h (corner bits, ComputeSlope or equivalent from corner heights).

## Tests (new file src/tests/junction_terraform.cpp, tag `[terraform]`; record each red against stubs)
1. **Outer "terraform: a ramp row cut flat makes a straight run buildable"**: a terrain with a ridge row across a straight X run (as MakeRidgeTerrain or the [placeoracle] ramp rows); a plan targeting the run's tiles flat at the run's level; PropagateJunctionTerraform succeeds; on ApplyJunctionTerraform's terrain JunctionFacts::StraightRun over the run is true (it was false before). Cost = the number of corner-level moves x terraform_cost + the cleared tiles' clear_cost, counted as the game would.
2. "terraform: raising one corner by one on flat land changes only that corner" (no propagation; the four tiles sharing it get the right slopes).
3. "terraform: raising a corner by two propagates one ring" (neighbouring corners raised to stay within one; slopes and z_min/z_max of every touched tile match what the game would make; steep only where the game makes steep).
4. "terraform: a plan touching a house, water, rail/road (Spannable) or the snapshot edge is refused" with the reason naming the tile.
5. "terraform: corner heights round-trip": for every non-steep and steep slope, corner heights from (z_min, slope) and back give the same tile.

## Changes
1. **src/junction_terrain.h:** `Money terraform_cost = 0; ///< Price of moving one corner one level (PR_TERRAFORM).` in JunctionTerrain; filled where foundation_cost is filled in src/junction_game.cpp.
2. **src/junction_terraform.{h,cpp}** (src/CMakeLists.txt beside junction_terrain):
```cpp
/** A terraform plan (D218): target heights of tile corners in the snapshot's canonical frame; corner (x, y) is the north corner of tile (x, y), i.e. shared by tiles (x-1..x, y-1..y). */
struct JunctionTerraform {
	std::map<std::pair<int, int>, int> corners; ///< Corner -> target height (only corners that change).
	Money cost = 0; ///< Set by PropagateJunctionTerraform.
};
/** Complete the plan as CmdTerraformLand would: every corner orthogonally next to a changed corner is moved so neighbouring corners differ by at most one, repeatedly; fails with the reason (tile, class) when a corner of a tile that cannot be terraformed (not Buildable) or outside the snapshot would move. Sets cost (corner-level moves x terraform_cost + clear_cost of each Buildable tile whose corners change). */
bool PropagateJunctionTerraform(const JunctionTerrain &terrain, JunctionTerraform &plan, std::string *why);
/** The terrain with the plan applied: z_min, z_max, slope, steep and incline recomputed for every tile with a changed corner. */
JunctionTerrain ApplyJunctionTerraform(const JunctionTerrain &terrain, const JunctionTerraform &plan);
/** A tile's four corner heights (canonical corner order W, S, E, N as slope_type.h names them), and the tile back from them. */
std::array<int, 4> JunctionTileCorners(const JunctionTerrainTile &tile);
void SetJunctionTileCorners(JunctionTerrainTile &tile, const std::array<int, 4> &corners);
```
   The corner order and which canonical corner is "north" follow the snapshot's canonical frame exactly as FillJunctionTerrainCell builds slopes; if the canonical frame makes "corner (x, y)" ambiguous, STOP and report.
3. TESTS.md `[terraform]` row (fast, budget 1 s).

## Do not
- No change to placement, facts, the generator's build steps, or any game command. Nothing in-game in this step.

## Stop conditions
- The game's propagation rule or price differs from the brief's summary in a way the tests would encode wrongly (report what the game does, with file:line); the canonical corner mapping is ambiguous; anything not covered.

## Gates
- `tools/junction/jt fast build '[terraform]' 5`; `tools/junction/jt gate build-rel` (report the count); the `openttd` target builds.
- One commit: `Add: Junction terraform plan on a terrain snapshot: propagation, cost, applied terrain`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus what the game's propagation and pricing do (file:line) and how the tests mirror it. When you have reported, stop.
