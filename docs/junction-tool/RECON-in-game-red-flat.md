# RECON IG1: why in game every hover has red tiles, and why the terraform flattens

Launch HEAD e1bfcda67d (worktree .claude/nodes, branch wt-ig1). Experiment: patch /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/ig1.patch (one hidden test "[.ig1]" in src/tests/junction_game.cpp, run as `IG1_OUT=file IG1_STEEP=3 [IG1_TRAIN=hp,te,mass,drag,speed,tl] build-rel/openttd_test "[ig1]"`). Raw outputs: scratchpad ig1-*.txt.

## Method (what was and was not done)
- The `[game]` environment can run the real commands once it has a company (CompanyID::Begin(), avail_railtypes, unlimited money and limits), ResetBridges, ResetRailTypes/InitRailTypes, RecomputePrices, year 2000, build_on_slopes true (Pete's openttd.cfg: build_on_slopes true, max_bridge_height 12, train_slope_steepness 3, realistic acceleration).
- Each of the 48 fixtures of tools/junction/terrain is loaded into a 256x256 Map (corner heights from the tile corners; Buildable = clear grass, Water = sea or river, Spannable = plain rail, Blocked = a rail depot). Check: MakeJunctionTerrain on that map reproduces the fixture in the 2470 cells it can compare: 0 differences in heights, slopes and classes (the class mapping is faithful).
- Per fixture that plans: PlanJunctionHoverNear(seed, spec, terrain, {0,0}) -> MakeJunctionTerraformSteps -> ApplyJunctionOrientation/OrientJunctionOffset as ShowProjection -> EstimateJunctionTerraformed on the untouched map (the GUI's red tiles) -> the steps run with Command<TerraformLand>::Do(Execute) -> the same estimate again.
- 31 of 48 fixtures plan with the test spec (T topology, seed 41x14); 17 give no plan (not red tiles). Only the T topology exists in the GUI (the screenshot's "L_R x L_R" is its notation).
- NOT done: a sequential Execute of the rail/bridge steps after the terraform (it hit an assert, IsValidDiagDirection in map_func.h, in my harness; not investigated: INFERRED harness artefact). The tests after the terraform are the estimate's own QueryCost tests.

## 1. Red tiles
Before the terraform (the GUI today): 30 of 31 plans have red tiles, 338 bad tiles in all (mean 11; one plan is clean). By failing step:
- 257 rail steps, all DIAGONAL (half) pieces (Upper/Lower/Left/Right) on tiles the plan terraforms, STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION: the current slope refuses the piece (GetRailFoundation Invalid), the planned slope (flat in every one of the 257) accepts. Current slopes: inclined (3, 6, 9, 12) and single-corner (1, 2, 4, 8), a few 23/27/30. No straight piece failed, and no rail piece failed on a tile the plan leaves alone.
- 42 bridges (2 head tiles each): 32 BRIDGEHEADS_NOT_SAME_HEIGHT and 6 LAND_SLOPED_IN_WRONG_DIRECTION (heads on terraformed tiles: the heads are measured on the old ground), 4 BRIDGE_TOO_LOW_FOR_TERRAIN (heads untouched).
- Station/signals: none (the T has no station). Tiles the snapshot calls Buildable but the game refuses: none (0 mismatches above).
After applying the terraform steps with the game's command (0 of the steps failed) and re-running the tests: 20 bad tiles remain in 10 plans, 0 rail pieces, all bridges: 10 x STR_ERROR_BRIDGE_TOO_LOW_FOR_TERRAIN. Example (all ten look alike): a 5-middle bridge, heads at z 5, the span's middle tiles 5,5,5,(5-6),(5-6): game rule (tunnelbridge_cmd.cpp:495) is GetTileMaxZ(middle) <= z_start of the head, and the plan leaves two middle tiles one level above the head. I found no span-clearance check against the head level in junction_facts/nodes/levels (grep z_max, clearance): INFERRED that the planner does not enforce it; a cause independent of the order of test and terraform.
So: the order (test on the current map, terraform later) explains 318 of 338 red tiles (94 %); a planner rule gap on bridge spans explains the remaining 20 (and in game would fail the build, not only the estimate).
Matches the engineer's screenshot (3-4 red tiles, diagonal pieces and a bridge head on gentle grass, west half near the smaller bridge): the same two groups.
Suggested fix to test (not done): run the rail/bridge tests on the map after the plan's terraform (apply the steps to a copy of the map or evaluate GetRailFoundation on the planned slope), and add the span rule max_z(middle) <= head z to the plan.

## 2. Flattening
JunctionRampLevels at slope_steepness 3 (default, Pete's cfg), tl and mass from the loco (original vehicles, loaded wagons; speed = the train's top speed; TE = weight x 76 x 9800 / 256):
- MakeTestJunctionSpec: slope_steepness is 0 in the test spec, so ramp_levels = tl = 4 (every fixture test runs with that); with steepness 3 the design train (561 t, 14194 hp, 196) gives 5.
- Vanilla trains, 4 / 6 / 8 coal wagons (tl 3 / 4 / 5): 0 for every steam loco, the 1990s diesels (Ploddyphut, Powernaut, SH/Hendry 25, UU 37, Floss 47) and the Turner Turbo/SH 125 give 0-1; only CS 4000 (2) and Centennial (3) reach 2-3. Passenger wagons (lighter): Floss 47 1, Turner 2, CS 4000 3-4, Centennial 4-5, the rest 0. At steepness 7 almost everything is 0-1.
- So for typical vanilla trains ramp_levels is 0 or 1: D219 then allows no climb (0) or one level per tl tiles (1). A strong train (the engineer's, ramps visible) gives 3-5.
Plans (31-40 fixtures plan per spec; tiles = layout track tiles incl. bridge heads; "ramp" = planned ground inclined and the level at its low end):
| train (ramp_levels) | plans | track tiles flat before -> after terraform | ramp tiles | mean abs(level - original ground) | mean level range per plan |
| test spec (4) / design 561 t (5) | 31 | 72.5 % -> 89.7 % | 4.4 % | 0.37 | 2.68 |
| Powernaut + 4 coal (0) | 39 | 75.9 % -> 97.3 % | 0.1 % | 0.42 | 0.79 |
| Floss 47 + 4 pass (1) | 38 | - -> 92.4 % | 3.0 % | 0.33 | 1.63 |
| CS 4000 + 4 pass (3) | 40 | - -> 92.6 % | 2.9 % | 0.33 | 1.62 |
| Centennial + 6 pass (5) | 38 | 76.0 % -> 92.5 % | 3.1 % | 0.34 | 1.63 |
Where originally sloped tiles go (design train, counts over the 31 plans): non-straight pieces (curves, diagonals, junction tiles) on originally sloped ground: 737, 725 of them (98 %) end flat; straight pieces: 902, 478 (53 %) end flat; with ramp_levels 0: 440 straight, 365 (83 %) flat; with 3: 455, 260 (57 %).
Causes: (a) a non-straight piece cannot ramp (the ramp tile is inclined along a straight piece), so on slopes it is levelled by terraform or sits on a foundation: this alone flattens about 98 % of sloped curve/diagonal tiles regardless of the train; (b) with ramp_levels 0 (typical vanilla train) the D219 rule forbids any climb inside the junction: flattening of straight pieces is the rule's consequence (83 %) and not a solver fault; (c) with ramp_levels 3-5 about half of the sloped straight tiles are still flattened (53-57 %): not diagnosed (INFERRED: the ramp window spacing and the terraform-versus-ramp cost; no example traced).
These fixtures are gentle where they plan (72 % of the track tiles start flat), so the real-hill share of flattening is probably higher: the fixtures that fail to plan are the steep ones.

## 3. Other causes
None found beyond 1 (no signal or station failures: signals are priced without a command; the T has no station). Caveats: one anchor per fixture, no existing rail/roads except the Spannable class, company money and limits unlimited, depot for Blocked, only the T topology, test spec bridges (span 1-10) rather than the game's max_bridge_length 64.
