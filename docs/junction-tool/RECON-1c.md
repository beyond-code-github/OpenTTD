# Recon for milestone 1c: train physics, signals, rig traffic tools

Head `e37f0bd3cb`. Read-only research for `PLAN-1c.md` step 1. File:line refs are into `src/` unless a path is given. Line numbers are approximate to a few lines. Anything marked *inferred* was read from code, not measured in game.

Units: train `cur_speed` is "km/h-ish" (1 unit = 1 km/h-ish; mph x 1.6). Power is hp, weight tonnes, force N.

## A. Realistic acceleration

### 1. `GroundVehicle::GetAcceleration` (ground_vehicle.cpp:105-186)

**Caller chain, per tick.** `TrainLocoHandler` (train_cmd.cpp:~4030-4165) calls `consist->UpdateSpeed()` (train_cmd.cpp:~3081). In the realistic model that is `DoUpdateSpeed(GetAcceleration(), brake ? 0 : 2, GetCurrentMaxSpeed())` (train_cmd.cpp:~3087; `DoUpdateSpeed` at ground_vehicle.hpp:373-398). Once per tick per train.

**Inputs and where they come from.**
- `speed` = `cur_speed`.
- `mass` = `gcache.cached_weight` = sum of `GetWeight()` over all parts, min 1 (`CargoChanged`, ground_vehicle.cpp:78-97).
- `power` = `gcache.cached_power * 746` W. `cached_power` is the sum over parts of `GetPower() + GetPoweredPartPower()` (`PowerChanged`, ground_vehicle.cpp:20-72; train.h:205-232). Multihead halves power per part.
- `max_te` = `gcache.cached_max_te` = sum over **powered parts only** of (that part's own `GetWeight()` x TE byte), x 9800 / 256 (ground_vehicle.cpp:36,64). It uses the loco's own weight, not the train's.
- `cached_air_drag` = `air_drag + 3*air_drag*n_parts/20` (ground_vehicle.cpp:62). `air_drag` is the engine's `RailVehInfo.air_drag`. If 0, it is `max(2048/max_speed,1)`, or 192 if max_speed <= 10 (ground_vehicle.cpp:45-58).
- `area` = 14, or 28 inside a tunnel (train.h:272).
- Rolling friction coefficient = `15*(512+speed)/512` (train.h:309).
- Slope steepness setting = `vehicle.train_slope_steepness` (train.h:321). Slope force is zero on flat track.

**Plain maths, flat track, accelerating (`AS_ACCEL`).**
- Resistance `R = 10*M` (axle) `+ M*15*(512+v)/512` (rolling) `+ area*cached_air_drag*v^2/1000` (air). `M` is mass in tonnes, `v` is speed.
- Slope: `R += sum(part_weight * steepness * 100)` over parts going up, minus the same going down. It is zero on flat.
- Tractive force `F = min(max_te, P*18/(5*v))`, with `P = 746*hp`. If `v = 0`: `F = max(min(max_te, P), 8*M + R)` (kick-off).
- `accel = (F - R) / (4*M)`, integer division. If `F > R` the result is at least +1; if `F < R` it is at most -1. `F == R` gives 0.
- Per tick: `subspeed += accel` (a u8 accumulator), then `cur_speed += subspeed_overflow` (i.e. `accel/256` km/h-ish per tick on average). This is `DoUpdateSpeed` (ground_vehicle.hpp:373-398). The speed is clamped to `[min_speed, max_speed]`. `min_speed` is 2 while accelerating, so a moving train never drops below 2. Above max, speed decays by `cur/10 + 1` per tick, not below max.
- Maglev: no axle or rolling resistance, `F = P/25`.

**Max speed that clamps it (`Train::GetCurrentMaxSpeed`, train_cmd.cpp:396-449).** It is the minimum of:
- `tcache.cached_max_curve_speed` (realistic) or `cached_max_track_speed` (original);
- the station approach cap, realistic only (train_cmd.cpp:~402-425);
- the depot limit 61;
- the bridge speed while on the bridge middle;
- the order max speed;
- 32 when reversing without a cab;
- `gcache.cached_max_track_speed`, the minimum over parts of the engine max speed and the rail type max (ground_vehicle.cpp:25-40).

**Curve limit (`Train::GetCurveSpeedLimit`, train_cmd.cpp:321-394).** It is computed from direction differences between adjacent vehicles, so it is a property of the consist's shape at that moment:
- two 45-degree bends closer than a vehicle length: 88;
- any 90-degree bend: 61;
- otherwise `232 - (13 - clamp(avg_spacing,1,12))^2`;
- exactly one left and one right bend: no limit.

It is then raised by `rti->curve_speed` and a tilt bonus. A straight junction (no bends) has no limit. A generated junction with Upper/Lower/Left/Right pieces does bend: **the limit applies on diverging and merging pieces**. The model for the reference train should either assume straight track or evaluate the limit per piece.

### 2. Mass and cargo

`GetWeight()` = `CargoSpec::WeightOfNUnitsInTrain(stored_count)` + engine weight (train.h:236). Articulated parts add no base weight; powered wagons add `pow_wag_weight`. `WeightOfNUnitsInTrain` multiplies the units by `freight_trains` (default 1, range 1-255) for freight cargo only (cargotype.cpp:~261; setting game_settings.ini:312). The weight is a sum of loaded stored cargo, so empty = base weights only. `GetMaxWeight()` (train_cmd.cpp:4330) gives the full-load weight, which a pure model can use for the loaded case (it uses `DetermineCapacity`, which is the capacity of the current cargo type or refit). The rig's wagons are never refitted or loaded, so rig runs are **empty** (gs/main.nut:208-247).

### 3. Progress and distance

- Per tick: `scaled = GetAdvanceSpeed(cur_speed) = cur_speed*3/4` (vehicle_base.h:442), plus the saved `progress`. `DoUpdateSpeed` returns that sum `j` (ground_vehicle.hpp:~392-397).
- Step cost: `GetAdvanceDistance()` = **192** if the moving direction is "diagonal" (`IsDiagonalDirection`, the NE/SE/SW/NW travel direction, i.e. along an X or Y track) and **256** (`TILE_CORNER_DISTANCE*2`) otherwise, i.e. travel in N/E/S/W direction along an Upper/Lower/Left/Right piece (vehicle_base.h:29-30, 454-457).
- The loop (train_cmd.cpp:4129-4160): `if j < adv: keep j as progress; else { for(;;){ j -= adv; TrainController(...); adv = next step cost; if (j < adv || cur_speed==0) break; } }`. Leftover `j` is saved in `consist->progress` (train_cmd.cpp:~4165). So several steps per tick are possible at high speed.
- A straight tile is 16 steps, so 16*192 = 3072 progress. **Ticks per straight tile = 4096 / v** at constant speed v (0.75v progress per tick). At 100: about 41 ticks per tile. At 74 ticks per day this is about 1.8 tiles/day (*inferred*). The 256 cost applies to N/E/S/W steps, so a step there is 4/3 as costly as a straight step. Whether the plan's "diagonal piece is 2/3 of a straight tile" (D36) holds depends on how many steps such a piece takes. **That was not checked here; it needs verifying against the track-piece step counts (open question 8).**

### 4. Braking and red signals

- **No modelled braking distance for signals.** A train at line speed does not decelerate for a red signal. Three effects combine:
  1. In the last tile before a red signal, `TrainApproachingLineEnd` (train_cmd.cpp:3857-3906, called from `TrainCheckIfLineEnds`, train_cmd.cpp:~4015) clamps `cur_speed` to `_breakdown_speeds[x]` (train_cmd.cpp:3852: 225, 210, ... 30, 15, 15 by sub-tile position x = 0..15). This is a cap, instant, not a deceleration. At 100 units it bites only from about x = 9 on.
  2. On entering the red tile the controller sets `cur_speed = 0; subspeed = 0; progress = 255` (train_cmd.cpp:3459-3475). It is an instant stop.
  3. The train is therefore effectively hard-stopped within about one tile. Stopping distance is about 1 tile and independent of speed (*inferred*).
- `AS_BRAKE` only applies when the vehicle is Stopped, Reversing or Stuck (train.h:291). Then `accel = min(-F - R, -10000)/M`, a strong deceleration, and min speed 0.
- Stations (realistic model only): speed cap `max(st_max_speed, 25*distance_to_go)` as the train nears its stopping spot (train_cmd.cpp:402-425). Not relevant to a through junction.
- The realistic braking settings (`train_acceleration_model`, `train_slope_steepness`) do not affect red-signal stops. Original model: braking factor -4x acceleration for AS_BRAKE (train_cmd.cpp:3084), but red signals work the same hard-stop way.
- Consequence for the plan: "braking for joiners" never happens as a deceleration ramp. A mainline train either keeps going (green) or stops at the signal tile edge. **Headway should be block occupancy plus restart acceleration, not braking distance** (open item 6 is answerable that way unless the engineer wants conservative margins).

### 5. Original model, one paragraph

Acceleration is a constant per engine: `acceleration = clamp(power/weight*4, 1, 255)` (`UpdateAcceleration`, train_cmd.cpp:452-461), applied as `acceleration*2` per tick while moving and `acceleration*-4` when braking (train_cmd.cpp:3084). The target speed is the engine/track max, no curve limit (train_cmd.cpp:328, 399). Speed is instead reduced by fixed fractions when climbing (`AffectSpeedByZChange`, `_accel_slowdown`, train_cmd.cpp:3146-3170) and on 45/90-degree turns (train_cmd.cpp:3577-3580). There is no air drag, rolling resistance, TE cap or mass sensitivity beyond the power/weight ratio. A pure model for this is trivial (linear ramp), but the two models diverge in the speed-time curve, so prio lengths would differ.

### 6. Inputs a pure model needs

- Engine: `power` (hp, `RailVehicleInfo.power`), `tractive_effort` byte, engine `weight`, `air_drag` byte (0 means derived from max speed), engine max speed (`GetDisplayMaxSpeed`), `pow_wag_power`/`pow_wag_weight`, multihead flag.
- Wagons: `weight`, cargo capacity and cargo weight per unit (loaded case), `freight_trains`.
- Rail type: max speed (`GetMaxTrackSpeed`), acceleration type (normal, monorail, maglev), `curve_speed`.
- Settings: `train_acceleration_model`, `train_slope_steepness` (flat track: irrelevant), `freight_trains`. `max_train_length` (default 7, game_settings.ini:214) is only a build cap in tiles, not physics, but it bounds the picker's consist length.
- Number of parts (`n_parts`, enters the air-drag cache).
- GRF properties can override power, weight and TE through `GetVehicleProperty`, so read effective values from the game (via GS or the `Train` caches), not just the base `RailVehicleInfo`.
- Constants: 746 W/hp, 9800, the rolling and air formulas above, 0.75 progress per speed unit, 192/256 step costs.

## B. Signals and blocks

### 7. Block signal vs path signal, occupied next block

**Where it stops (both kinds).** At the red signal tile boundary. The check runs when the front is about to enter the tile that holds the red signal (`red_signals` from `GetTileTrackStatus`, train_cmd.cpp:~3420, 3459-3475), and the speed cap table (A4) engages in the last tile. The front stops at the edge of the signal tile, short of it (*inferred from code; not measured*). The rig `sample` tool can measure the exact stop tile.

**Block signal.** Red while the segment ahead is occupied (`UpdateSignalsOnSegment`). When the occupying train's last vehicle leaves a tile, `TrainMovedChangeSignals(gp.old_tile, ...)` recomputes the segment (train_cmd.cpp:3638-3662, "Signals can only change when the first or the last vehicle moves"). So the signal turns green on the tick the tail leaves the block's last tile.

**Restart.** The stopped train re-attempts every tick (the `progress = 255` trick, train_cmd.cpp:3466-3467). It moves on the first tick after green, then accelerates from 0: kick-off `F = max(min(max_te, P), 8M + R)` (ground_vehicle.cpp:~160-165). So restart is about 1 tick plus a full acceleration from standstill (*inferred*).

**Path signal.** The train tries to reserve a path at the signal tile (`ChooseTrainTrack`, train_cmd.cpp:2778 and its call at 3439). The signal shows green only if the reservation succeeds; otherwise it stays red (`changed_signal` reset). Reservations are released per tile as the tail passes (`ClearPathReservation`, train_cmd.cpp:~3560). So a following train can restart as soon as the tail has cleared the tiles it needs, not the whole block. Path signals allow tighter following inside a long block than block signals.

**"Stuck" is different.** A train that already passed a signal and then fails to extend its reservation is flagged `Stuck` and re-tries every `path_backoff_interval` ticks (default 20, pathfinding_settings.ini; train_cmd.cpp:4087-4110), reversing after `wait_for_pbs_path` (default 30 days). That case adds up to 20 ticks of dead time and is the only place the interval matters.

### 8. YAPF reservation and headway

Does a train reserve ahead through several blocks? No. `CYapfReserveTrack` reserves up to the first **safe waiting position** (yapf_rail.cpp:36-215, `FindSafePositionProc` at ~58-68). `IsSafeWaitingPosition` (pbs.cpp:395-428) is true on a tile with a **block** signal on the trackdir (pbs.cpp:400), or one tile before a **path** signal (pbs.cpp:416-419), or at depots, line ends, one-way path signals facing us. So the reservation spans at most one signal-to-signal section; it never runs through several signals. Reservation is attempted only when entering a path signal or when `pf.reserve_paths` is on (default false, pathfinding_settings.ini:~87; the rig does not set it, ottd_core/server.py:15-45). A reserved path through a path-signalled merge does cut headway at the merge: the first train holds every tile to the next signal. Block-signalled sections have no reservation, only occupancy.

## C. The rig's traffic tools (`tools/ottd/`)

### 9. What exists

Wagons are plain, unrefitted, empty (mass = base). The engine is the fastest buildable by default (`--engine ID` overrides). Defaults: realistic acceleration model, breakdowns off (server.py `[difficulty] vehicle_breakdowns = 0`), `reserve_paths` default off.

| Command | What it does or measures |
|---|---|
| `start/stop/status/rcon/gs` | lifecycle, console, raw GameScript JSON |
| `company` | selects or starts the GS company (IdleAI) |
| `build-line`, `depot`, `build-layout` | tracks, depots, a layout JSON (tracks, bridges, signals) |
| `train DEPOT_X DEPOT_Y [--engine] [--wagons]` | builds one train (gs/main.nut:208-247) |
| `orders V X1 Y1 X2 Y2` | two go-to-depot orders, alternating (gs/main.nut:249-260) |
| `go V` | starts the train |
| `vehicles`, `watch V --seconds N` | snapshot of id, tile, speed, state; sampled min/max/avg speed at about 0.5 s wall time |
| `sample V... --ticks N` | per-game-tick speed per vehicle: n, min, max, avg, `stops` (transitions to 0), `drops` (fall of 10 or more within 8 ticks, with the tile where it began) (gs/main.nut:456-505) |
| `junction-check FILE [--trains N --ticks T --find-clear / X Y]` | builds a layout and its `depots`, buys N trains (default 6, 3 wagons) in the first depot, starts one about 300 ticks apart with **no orders**, samples T ticks, prints a per-train line, then `OK` or `DEADLOCK?` (every train at speed 0 on one tile in two snapshots 1000 ticks apart) (ottd:257-323) |
| `junction-here X Y [--rot --mirror --ml --branch --tl --near --instant]`, `junction-scan --count N` | generates a junction in game with rcon `junction_export`, then builds it; scan does random placements and counts build failures (ottd:180-245). Not in README. |

Flow: `junction-check` loops via the circuit layout (`layouts/circuit-LR-TL4.json`, one-way loops beyond each leg), so trains circulate without orders. `sample` is the only fine-grained measure.

### 10. What is missing for a traffic test

Gaps against the four needs:
- **N trains per movement:** `junction-check` buys all trains in one depot and has no orders, so which movement each train takes is decided by pathfinding (no destination), not by design. There is no way to assign a train to a specific route (ML through, ML to branch, branch to ML, branch through) or to run a mix.
- **ML trains braking for joiners:** `sample` reports drops at speed but only for named vehicles, and does not say whether the cause was a signal or the merge. No per-signal or per-tile accounting.
- **Queue lengths at merges:** nothing counts trains waiting (speed 0) per tile or per queue/bay.
- **Trains backing up past a split:** no measure of whether a queue reached the split tile.

**Smallest additions (proposed, not built):**
1. GS `build_train` gains `engine_name`/loaded option (refit and load a freight wagon, or full load via cargo) so the loaded case can be tested. Plus a way to report the train's actual power, weight, TE for model comparison (`GSEngine`/`GSVehicle` getters, or `rcon` vehicle info).
2. Per-movement spawn: layout JSON `depots` and per-depot `orders` (list of waypoint tiles), or CLI `--route "x,y x,y ..."`; orders via `GSOrder.AppendOrder` with go-via waypoints/depots (wagons need a station or waypoint; depots already work).
3. A spawner command: `traffic RUN --per-route N --interval TICKS`, which buys and starts N trains per route at a fixed interval, so loads are repeatable.
4. A GS `occupancy` sampler: every K ticks, for a tile list (the queue and merge tiles taken from the layout), count vehicles with speed 0 and record the max run of stopped trains. Output: per tile max stopped count, ticks with queue past tile T.
5. A named-tile event log in `sample`: `entered tile / speed` for chosen tiles (e.g. the merge signal tile and the ML tile after it), so the ML train's speed at the merge can be checked against line speed, and the stop tile of a red-signalled train measured.
6. A "backed-up" check: a pass/fail that a given split tile never has a stopped train on it for more than X ticks (a special case of 4 and 5).
7. Mass and rig settings: flags to `start` for `freight_trains`, `train_acceleration_model`, `pf.reserve_paths` so runs state their settings explicitly.

## Open questions for the engineer

Taken from PLAN-1c open items 2-6, refined by this recon.

1. **Loaded or empty (item 2).** The model can do either. The rig can run only empty today. Loaded gives the longer, conservative prio. Is conservative the rule, and is `freight_trains` taken as the game default (1) or as the live setting value?
2. **Mainline speed (item 3).** The model's speed is `min(engine max, rail type max)` regardless (ground_vehicle.cpp:25-40), so "the lower of the two" is what the game does. Do you want the prio sized to that, or to the rail type max (a fast train on a slow line is then over-provisioned)? Related: should bridge speed (`GetBridgeSpec(...)->speed`) cap it, since the picker also sets the bridge type (D47)?
3. **Original model (item 4).** Support both, or refuse and tell the player? The two give different prios. Realistic is the default (game_settings.ini:162), so refusing original is cheap, but it silently breaks for players on original.
4. **Picker list (item 5).** Engines available now, or all for the rail type? The code can't say which the player prefers. (Rig note: it only builds engines buildable now.)
5. **Braking and sighting (item 6).** Answered in code: no braking distance, hard stop within about a tile, cap table in the last tile. So the options are (a) block occupancy plus restart acceleration, no braking margin; (b) add a safety margin in tiles. Which? And does the prio length need to include the stop-tile offset (the train stops short of the signal tile, A4/B7), or is that absorbed in R25 spacing?
6. **Curve limit.** Merge and diverge pieces bend. Should the model apply the curve speed limit (88/61 and so on) inside the junction, or assume straight track at line speed? This changes the mainline speed at the merge, and so the prio length.
7. **Block vs path signal at merges.** Path signals release per tile, block signals per block (B7). Does the generator mix them, and should the headway model use different restart rules per type?
8. **Diagonal cost.** D36 says a diagonal piece is 2/3 of a straight tile in time. The code gives a step cost ratio of 256:192 per step; the step count per piece was not checked here. Should the model derive the piece time from the real step counts, or keep D36's figure?
9. **Rig settings.** Should rig traffic runs use the default settings (reserve_paths off, breakdowns off, realistic, `freight_trains` = 1) or the engineer's own game settings? This decides whether rig results stand in for the player's game.
10. **Traffic bracket values (item 1).** Trains per hour per movement for Low/Medium/High: needs the headway model plus rig runs; nothing in the code to derive them.
