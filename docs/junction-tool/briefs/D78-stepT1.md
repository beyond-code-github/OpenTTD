# Brief D78 step T1: terrain snapshot and router support

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Worktree:** `{WORKTREE}`, branch `{BRANCH}`, head `{HEAD}`. Build dir `{BUILD}`.
  - Configure it with `cmake -S . -B {BUILD} -G Ninja -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER=clang-18 -DCMAKE_BUILD_TYPE=Debug > {LOG}/cfg.log 2>&1` if it is absent.
  - Another agent works in `/home/pete/OpenTTD`; don't touch it.
- **Plan:** `/home/pete/OpenTTD/docs/junction-tool/PLAN-D78.md`, design points 1–4, step T1. **Facts:** `/home/pete/OpenTTD/docs/junction-tool/RECON-terrain.md`. Read both; they may not be in your worktree yet.

## 2. Read first

- `src/junction_router.h`, and `src/junction_router.cpp` (all; it is about 330 lines)
- `src/tests/junction_router.cpp`: the helpers `MakeRequest` and `WallLayout`, and one test

## 3. Changes

### 3.1 New `src/junction_terrain.h` (header only; add it to `src/CMakeLists.txt`)

```cpp
/** What occupies a tile, as far as the generator cares. */
enum class JunctionTileClass : uint8_t {
	Buildable, ///< Clear land, rough, rocks, trees, fields: buildable at a cost.
	Water, ///< Sea, canal, river: only under a bridge span.
	Blocked, ///< Houses, other companies' property, unclearable objects, own rail (1st cut).
};

/** One tile of the snapshot. */
struct JunctionTerrainTile {
	int8_t z_min = 0; ///< Lowest corner height.
	int8_t z_max = 0; ///< Highest corner height.
	bool steep = false;
	JunctionTileClass cls = JunctionTileClass::Buildable;
	Money clear_cost = 0;
};

/** A terrain snapshot in the generator's canonical frame. Tiles outside the grid are Blocked. */
struct JunctionTerrain {
	TileIndexDiffC origin; ///< Canonical coordinate of grid cell (0,0).
	int width = 0;
	int height = 0;
	std::vector<JunctionTerrainTile> tiles; ///< Row-major, width * height.
	Money foundation_cost = 0; ///< Price of a leveled foundation (BuildFoundation).
	bool build_on_slopes = true;
	int max_bridge_height = 12;

	const JunctionTerrainTile *At(int x, int y) const; ///< nullptr outside the grid.
	/** Whether rail can sit at level plane on the tile: flat at plane, or a non-steep slope whose top is plane and build_on_slopes. */
	bool UsableAt(int x, int y, int plane) const;
	/** Clear plus foundation cost of building at plane; only meaningful when UsableAt. */
	Money CostAt(int x, int y, int plane) const;
	/** Whether a bridge span at deck level plane+1 may pass over the tile. */
	bool SpanOk(int x, int y, int plane) const;
};
```

Implement the four member functions inline in the header, exactly as follows:

| Function | Rule |
|---|---|
| `At` | bounds check on (x − origin.x, y − origin.y) |
| `UsableAt` | the tile exists, `cls == Buildable`, `!steep`, and either (`z_min == plane && z_max == plane`) or (`build_on_slopes && z_max == plane && z_min < plane`) |
| `CostAt` | `clear_cost`, plus `foundation_cost` when `z_min < plane` |
| `SpanOk` | the tile exists, `cls != Blocked`, `z_max <= plane`, and `plane < z_min + max_bridge_height` |

### 3.2 Router (`junction_router.h/.cpp`)
Add these fields to `JunctionRouteRequest`:

```cpp
const JunctionTerrain *terrain = nullptr; ///< When set, tiles must suit the plane (D78).
int plane = 0; ///< The level the junction is built on.
```

When `terrain` is set, the router applies these rules:
- **Every F/L/R piece's tile** must pass `terrain->UsableAt(x, y, plane)`. That includes the start-sharing piece (Exception 1) and both pieces of Exception 2.
- **Cost:** add `terrain->CostAt(...)` to each such piece's cost. A tile already holding layout track adds nothing more: its clearing and foundation were paid by the earlier route.
- **Bridges:** both heads must pass `UsableAt` (and add `CostAt`). Every middle tile must pass `SpanOk`.
- Without `terrain`, behaviour is unchanged.

The heuristic stays the same: it remains admissible because terrain cost is never negative.

### 3.3 Tests (`src/tests/junction_router.cpp`, tag `[junction][router][terrain]`)

Use a helper that makes a flat Buildable grid at z=0, with `foundation_cost` 250, covering the request bounds. Record red evidence for each test against the unchanged router (the field exists but is ignored).

1. **Hill detour:** a 3-wide block of tiles at z 2..2 (flat at 2) across the straight line, with plane 0. Expect no route through those tiles: either a detour, or a bridge if the span rule passes. For plane 0 with a hill at z_max 2 the span is not OK, so it must be a detour.
2. **Leveled foundation:** one tile with z_min 0 and z_max 1 (non-steep) on the straight line, with plane 1 and every other tile flat at 1. The route goes straight through it, and its cost includes 250. Check this by comparing with an all-flat grid: the cost difference is 250. Expose nothing new; compute the cost from the route via the same prices: rail pieces × rail_piece + the known extras. If the cost can't be observed without new API, assert only that the route passes through that tile, and say so.
3. **House blocks:** a column of Blocked tiles. The route avoids it, or bridges over? No: spans over Blocked tiles are not OK, so it must detour or fail. With bounds that leave no detour, expect `std::nullopt`.
4. **Water under a span:** a column of Water tiles at z 0 with plane 0. A straight route bridges over it (`B` in the moves). With `bridge_span_ok` false, expect `std::nullopt`.
5. **No terrain:** behaviour is identical to before; the existing tests still pass.

## 4. Do not

- touch the generator, the checker, the signal pass or the game side
- add map access

## 5. Stop conditions

- A test expectation conflicts with the router's move rules. Report the moves.
- Anything not covered.

## 6. Gates

1. `cmake --build {BUILD} --target openttd openttd_test > {LOG}/t1_build.log 2>&1`. There must be no errors or warnings in junction files.
2. `{BUILD}/openttd_test > {LOG}/t1_test.log 2>&1`. All pass.

Commit message: `Add: Junction router respects a terrain snapshot`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report

- commit
- red evidence
- types and functions added
- suite totals
- the moves for tests 1, 2 and 4
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
