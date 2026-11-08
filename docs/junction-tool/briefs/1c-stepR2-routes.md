# Brief 1c step R2: traffic with fixed routes, and an "ML train stopped" measure

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Why:** R1 (`ottd traffic`) runs free-roaming trains on the test circuit, so loop congestion and junction behaviour mix together.
  - The prio-length comparison (prio 1×/2×/3× at 12 trains) suggests 2× clears ML waiting inside the junction. It can't say so cleanly.
  - This step pins each train to a route and measures **ML trains stopping inside the junction**. On a well-designed junction that number should be 0.
- **The circuit** (`[.circuit]` in `src/tests/junction_design.cpp`) has three one-way loops:
  - west: WOut→EIn;
  - east: EOut→WIn;
  - south: SOut→NIn.
- **Route classes, as "go via" waypoint orders:**
  - `ml`: via west, then east. Runs only the ML: E row, east loop, W row, west loop.
  - `bw`: via south, then west. Covers the N→W join and the E→S split.
  - `be`: via south, then east. Covers the N→E join and the W→S split.
- **Read first:**
  - `tools/ottd/README.md`;
  - `tools/ottd/ottd`: `setup_circuit`, `cmd_traffic`;
  - `tools/ottd/gs/main.nut`: `CmdBuildTrain`, `CmdOrders`, `CmdOccupancy`;
  - `src/tests/junction_design.cpp`: `[.circuit]` (from l.195) and `WriteLayoutJson`.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/r2/`

## 2. Changes

### 2.1 Circuit JSON gains waypoints and the junction box (`junction_design.cpp`)
1. **Waypoints.** After the loop signals are placed, for each loop raster (west, east and south, in the existing `loops` order), pick a waypoint piece:
   - the axis piece (X or Y track, not on a bridge) nearest the middle of the raster;
   - whose tile has exactly one track bit;
   - with no signal on that tile, and whose two neighbouring pieces along the raster are also unsignalled axis pieces.
   If none qualifies, FAIL with the loop name.
2. **Junction box:** the generated junction's own bounding box (from the junction layout before the loops are added), as `[x, y, w, h]`.
3. **Normalisation:** shift both by `min_x`/`min_y` like the depots.
4. **JSON:** `WriteLayoutJson` gains an optional parameter and writes `"waypoints":[{"name":"west","x":..,"y":..,"axis":"X"},...]` and `"junction":[x,y,w,h]` after the depots. Keep its existing output unchanged when the new data is absent.
5. Print them after the depot line too.
6. Regenerate all three circuits as the previous step did: `JUNCTION_GEN_PRIO=n JUNCTION_CIRCUIT=...` for `circuit-LR-TL4.json` (n=1), `-prio2.json` and `-prio3.json`.

### 2.2 GS (`gs/main.nut`)
1. **`build_waypoint {x, y}`:** `GSRail.BuildRailWaypoint(tile)`. Reply `{ok:true}`, or an error with `GSError.GetLastErrorString()`.
2. **`orders_via {vehicle, tiles:[[x,y],...]}`:** append one `OF_NON_STOP_INTERMEDIATE` order per tile, in order. Reply with the order count.
3. **`occupancy` gains an optional `box:[x0,y0,w,h]`** (map coordinates). When given, each per-vehicle entry in the reply also gets `in_box`, the number of samples where that vehicle was stopped (speed 0) with its location inside the box. Add it as `stops:[[id,count,in_box],...]`; the existing two fields keep their positions.

### 2.3 `ottd traffic ... --mix ml:N,bw:N,be:N`
- **Without `--mix`,** behaviour is unchanged.
- **With it:**
  - the train count is the sum (`--trains` is ignored);
  - after building the layout and depot, build the three waypoints at origin + waypoint;
  - build the trains as today, assigning classes round-robin in the order ml, bw, be (skipping exhausted classes), and give each its `orders_via` before `start`;
  - run `occupancy` with `box` = origin + junction box;
  - after the heatmap, print per class: trains, total stop events, and **stopped samples inside the junction** (the sum of `in_box`), plus that as a percentage of the class's samples (samples × trains in the class).
  - The last line is `ML STOPPED IN JUNCTION: <n>`.

## 3. Run (print the join line when the server starts; don't wait for anyone)

1. `ottd stop; ottd start; ottd company`.
2. For each of `circuit-LR-TL4.json`, `-prio2.json` and `-prio3.json`: `ottd traffic <file> --find-clear --mix ml:6,bw:3,be:3`.
3. Set `PYTHONUNBUFFERED=1` so progress shows in logs.

## 4. Tests

- `[.circuit]` passes for prio 1, 2 and 3. The JSON has 3 waypoints and a junction box.
- `junction-check` still passes once (exit 0) on `circuit-LR-TL4.json`.
- The rig runs complete.

## 5. Do not

- change the generator, router, signal pass or checker
- change the loops' routes or signals
- interpret beyond the numbers

## 6. Stop conditions

- An order or waypoint can't be built. Report the error.
- Trains don't follow their routes: an `ml` train is seen on a south-loop tile in a heatmap sample. Add a check if it's cheap: count `ml`-class samples on the south loop's tiles. Otherwise, report what you observed.
- Anything not covered.

## 7. Gates

1. Release build and suite (`build-rel/openttd_test`), all passing, since the C++ test changed.
2. The `junction-check` regression passes.
3. The three runs complete.

Commit message: `Add: Rig traffic with fixed routes and an ML-stopped measure`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- the per-class table and the `ML STOPPED IN JUNCTION` line for each of the three circuits
- the three junction heatmaps, cropped to the junction box if easy, otherwise full
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
