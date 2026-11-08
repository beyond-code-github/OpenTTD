# Brief 1c step M1: pure train performance model (realistic acceleration, flat track)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **You run in your own git worktree** (a copy of the repo on its own branch). Configure a Release build there:
  - `cmake -S . -B build-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 > <scratch>/m1/cfg.log 2>&1`;
  - then build only `openttd_test`.
  Commit on your worktree branch. The orchestrator merges it.
- **Base:** `feature/junction-tool` at `{HEAD}`.
- **Plan:** `docs/junction-tool/PLAN-1c.md` (design item 1), `docs/junction-tool/RECON-1c.md` §A (the formulas, with file:line refs), and decision D98 (a)–(c).
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/m1/`

## 2. Read first

- `docs/junction-tool/RECON-1c.md` §A1–A6.
- `src/ground_vehicle.cpp`: `GetAcceleration` (l.105–186).
- `src/ground_vehicle.hpp`: `DoUpdateSpeed` (l.373–398).
- `src/vehicle_base.h`: `GetAdvanceSpeed`, `GetAdvanceDistance`, and the `TILE_*` constants.
- `src/train_cmd.cpp`: the advance loop (about l.4129–4165), and how many movement steps a train takes through a straight (X/Y) tile and through a corner (Upper/Lower/Left/Right) piece. Find the per-track sub-tile step data the train controller uses.
- `CODINGSTYLE.md`: naming and comments.

## 3. Changes

### 3.1 New files `src/junction_train_model.h/.cpp`; add them to `src/CMakeLists.txt` the way the other `junction_*` files are listed

```cpp
/** Effective train properties, as the game caches them (GroundVehicleCache), for flat track. */
struct JunctionTrainParams {
	uint32_t power_hp = 0; ///< gcache.cached_power (sum over parts, hp).
	uint32_t max_te = 0; ///< gcache.cached_max_te (N).
	uint32_t mass_t = 1; ///< gcache.cached_weight (t), at least 1.
	uint32_t air_drag = 0; ///< gcache.cached_air_drag.
	uint16_t max_speed = 0; ///< Speed cap in cur_speed units (the lowest of engine, rail type and curve limits).
};

/** The state the game advances each tick. */
struct JunctionTrainState {
	uint16_t speed = 0; ///< cur_speed.
	uint8_t subspeed = 0;
	uint32_t progress = 0; ///< Saved progress, below the next step's cost.
	uint64_t distance = 0; ///< Progress units travelled in total.
	uint32_t ticks = 0;
};

/** Advance one tick on flat straight track: GetAcceleration (AS_ACCEL), DoUpdateSpeed, then progress, exactly as the game's integer code. */
void StepJunctionTrain(const JunctionTrainParams &params, JunctionTrainState &state);

/** Ticks for a train starting at speed 0 to travel the given progress distance on straight track. */
uint32_t JunctionTicksToCover(const JunctionTrainParams &params, uint64_t distance);

/** Progress units for one straight (X/Y) tile and for one corner (half) piece, from the game's step data. */
uint32_t JunctionProgressPerStraightTile();
uint32_t JunctionProgressPerCornerPiece();
```

- **Copy the integer arithmetic exactly**, including:
  - the kick-off rule at v = 0;
  - the ≥ +1 / ≤ −1 rounding of `accel`;
  - the `subspeed` overflow;
  - min speed 2 while accelerating;
  - the decay above max speed;
  - `GetAdvanceSpeed`.
- Comment each formula with the game file:line it mirrors.
- **Distance:** add the advance-speed progress each tick. On straight track every step costs 192, so the total progress travelled is the sum; ignore leftover-step subtleties beyond what's needed for `distance`.
- Don't model slopes, stations, maglev or the original model.

### 3.2 D36 check
`JunctionProgressPerStraightTile()` is 16 × 192 = 3072 if the game moves 16 steps per straight tile. `JunctionProgressPerCornerPiece()` is (steps per corner piece) × 256.
- Find the corner step count in the code, and cite it.
- Report the ratio corner/straight. D36 assumed ⅔.

## 4. Tests (`src/tests/junction_train_model.cpp`, registered in `src/tests/CMakeLists.txt`); red first (a link error is acceptable red for new functions; say so)

1. **Kick-off:** for params {power 1000 hp, max_te 200000, mass 200 t, air_drag 50, max 160}, the first `StepJunctionTrain` from rest gives speed > 0 within 3 ticks.
2. **Hand-worked tick:**
   - at speed 100, same params, compute `F`, `R` and `accel` by hand in a comment, using the RECON-1c formulas;
   - assert one step from {speed 100, subspeed 0} gives the hand result (speed and subspeed).
3. **Terminal speed:** run 20000 ticks. The speed settles at `max_speed` (160) if F > R there. Otherwise it settles where F ≈ R. Assert it never exceeds `max_speed`.
4. **Monotonic in mass:** `JunctionTicksToCover(…, 10 tiles)` with mass 400 is > with mass 200, which is > with mass 100.
5. **Straight tile:** `JunctionProgressPerStraightTile() == 3072`.
6. **Corner piece:** `JunctionProgressPerCornerPiece()` equals the cited step count × 256.

## 5. Do not

- read any game state: the model is pure
- touch any other junction file
- implement prio sizing (a later step)

## 6. Stop conditions

- The game's step counts per piece can't be found. Report what you found.
- Anything not covered.

## 7. Gates

1. Release build of `openttd_test` in the worktree, then `build-rel/openttd_test "[junction]"` plus the new tests, all passing.
2. Commit on the worktree branch: `Add: Junction train performance model`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- the worktree branch name and commit
- red evidence
- the step counts and D36 ratio, with file:line
- suite totals
- stops
- `git status --porcelain` and `git log -1`, both in the worktree

When you have reported, stop.
