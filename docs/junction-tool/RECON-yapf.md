# RECON: YAPF rail cost model (OpenTTD, feature/junction-tool tree)
Paths are under /home/pete/OpenTTD/src. "(inf)" = my inference, not read directly from code.
Abbrevs: costrail = pathfinder/yapf/yapf_costrail.hpp; ini = table/settings/pathfinding_settings.ini; TL = YAPF_TILE_LENGTH = 100.

## 1. Node / segment structure
- Constants: TL=100, corner (non-diagonal trackdir) tile = 71, infinite = 1000*TL (pathfinder/pathfinder_type.h:16,19,27).
- A node = (tile, trackdir) key (yapf_node.hpp:50). A node is really a SEGMENT: PfCalcCost (costrail:295) walks tiles from the node's first tile until a reason to end (costrail:359-576).
- Segment ends at: next tile is a choice (new_td_bits.Count()>1 -> ChoiceFollows, costrail:525); station tile (costrail:468, Station, possible target); depot; waypoint; dead end / follow failure (509-521); rail type change (546); a PBS signal ahead / back of a path one-way signal when masking (534-543, only for safe-tile searches); loop / >10000 cost (553-565).
  Signals (non-PBS) do NOT end a segment for ChooseRailTrack; they are accumulated inside the segment (SignalCost, costrail:199-268). Only `DoTrackMasking` pathfinders (safe-tile) end on signals (477-481).
- Children are created in AddMultipleNodes, one per trackdir, `is_choice` = Count()>1 (yapf_base.hpp:232-239) -> flag choice_seen (yapf_node_rail.hpp:155).
- Node cost = parent_cost + segment_entry_cost (curve+doubleslip into the first tile, from parent) + segment_cost + extra_cost (costrail:632). Origin node has no entry cost (costrail:352-356, 366).
- Segment cost = sum over tiles of OneTileCost + 100*tiles_skipped + slope + signal + reservation + station + intra-segment curve/doubleslip transitions (costrail:361-410).
- Cache: segment cost (cached per (tile,td) key, node_rail.hpp:20) is reused only if global cache allowed: CanUseGlobalCache requires parent->num_signals_passed >= look-ahead size (=10) (costrail:637-642, 371-388). I.e. within the first 10 signals everything is recomputed live from current signal states; beyond that cached (static layout costs; signal states there are not read as red-penalties anyway since look_ahead cost array is exhausted, costrail:217).
- Search: A* on estimate = cost + octile distance to CalcClosestStationTile (destrail:151-168, yapf_common.hpp:129). Terminates when best open node is a destination (yapf_base.hpp:152-171) or ClosedCount >= max_search_nodes (default 10000, ini line ~102-105) -> then best intermediate node is used (GetBestNode, yapf_base.hpp:197-200). Ties on estimate: binary heap order, unspecified (inf: broken by insertion/heap order, effectively arbitrary but deterministic; can send all trains one way).

## 2. Cost terms (defaults from ini; units = cost points, TL=100 = one straight tile)
Base (costrail:136-158):
- diagonal trackdir tile: 100. non-diagonal tile (the "corner" tile of a 45deg curve): 71.
- level crossing (road tile, diagonal): +rail_crossing_penalty = 3*TL = 300.
- bridge/tunnel/station skipped tiles: +100 each (tiles_skipped, costrail:401). Bridge head tile itself 100. Bridge ramp slope: +slope penalty on entering unless flat ramp (yapf_costbase.hpp:27-45); tunnel none.
Transitions (costrail:104-118):
- 45deg turn (td2 != NextTrackdir(td1)): rail_curve45_penalty = 1*TL = 100. 90deg (only when 90deg turns allowed AND TrackdirCrossesTrackdirs): rail_curve90_penalty = 6*TL = 600.
- doubleslip (plain rail tile both sides with >1 track bit facing each other): rail_doubleslip_penalty = 1*TL = 100 (costrail:120-128).
- slope: uphill diagonal entry: rail_slope_penalty = 2*TL = 200 (costrail:98-102).
- depot reverse 50*TL = 5000 (costrail:418).
- speed limits (bridge speed, railtype max): inside look-ahead radius only (num_signals_passed < 10): extra = TL*(vmax_veh - limit)*(4+tiles_skipped)/vmax_veh when limit < train max (costrail:484-497). So a slow bridge adds up to ~400+100*len points. Applied in extra_cost (note: uses `follower` of the move into this node).
Stations (costrail:468-475, 606-628):
- Every station tile passed: rail_station_penalty = 10*TL = 1000 per tile (segment cost, platform_length = tiles_skipped+1).
- When it IS the destination: extra -= 1000*platform_length (cancels it) and + PlatformLengthPenalty (costrail:270-286): missing = ceil(train_len/TILE_SIZE) - platform_len. missing<0 (platform longer): rail_longer_platform_penalty 8*TL = 800 + per_tile(0)*|missing|. missing>0 (shorter): rail_shorter_platform_penalty 40*TL = 4000 + per_tile(0)*missing. Exact fit: 0. So destination platform cost = distance + curves only (+800 if longer than train, +4000 if shorter).
  - Consequence: routing THROUGH another station (e.g. via a different platform's tiles) costs 1000/tile; this strongly discourages paths through platforms.
Signals (costrail:199-268), only counted for signals facing the travel direction:
- look-ahead array (ctor, costrail:63-70): LA[i] = p0 + i*(p1 + i*p2) with p0=500, p1=-100, p2=5, count rail_look_ahead_max_signals=10:
  LA = [500, 405, 320, 245, 180, 125, 80, 45, 20, 5] for i=0..9 (i = signals already passed in this path; computed by me from the formula). Signals beyond 10: 0 (costrail:217).
- Red signal (non-PBS type) in path direction: + LA[num_signals_passed] (if >0) (240-243). Green: nothing (negative LA would be added, but defaults are all >=0).
- First signal on the path (num_signals_passed==0) and red: Block/Entry: + rail_firstred_penalty = 10*TL = 1000; Combo/Exit: + rail_firstred_exit_penalty = 100*TL = 10000 (246-253).
- Red PBS (path) signal: no LA penalty, no firstred penalty (240, 247-253 default branch) (PBS handled by reservation costs below).
- Two-way first red signal: if setting rail_firstred_twoway_eol=true (default; ini ~111-114) and ChooseRailTrack sets it (yapf_rail.cpp:460), a RED non-PBS first signal that has signals in both directions, after a choice has been seen in the path, = DEAD END: branch pruned entirely (costrail:229-235). So a red two-way signal as first signal after a split makes that lane unusable (not just penalised); if all branches are pruned path_found counts as found (yapf_rail.cpp:501).
- last-red at destination (costrail:606-617): if the last signal passed before the target was red: Exit-type: rail_lastred_exit_penalty = 100*TL = 10000; other non-PBS: rail_lastred_penalty = 10*TL = 1000. A later GREEN signal resets last_signal_was_red (costrail:220); a red PBS signal gives none.
- one-way signal against us: segment dead end (206-208). Back of PBS signal passed (has_signal_against & PBS): + rail_pbs_signal_back_penalty = 15*TL = 1500 if num_signals_passed < 10 (262-264).
PBS reservations (costrail:184-197): only when n.last_signal_type is PBS (initial node assumes PBS, node_rail.hpp:~143-146) and num_signals_passed < LA_size/2 = 5:
- station platform with any reserved tile in the skipped run: rail_pbs_station_penalty = 8*TL = 800 * (skipped+1).
- tile whose reserved trackbits overlap the track we'd take: rail_pbs_cross_penalty = 3*TL = 300 per tile (x71/100 for diagonal-less -> 213 on corner tiles) * (skipped+1).
- ChooseRailTrack uses the plain follower WITHOUT MaskReservedTracks (yapf_rail.cpp:403-500 vs follower with masking only in CYapfFollowAnySafeTile, 344); so reserved tracks are penalised, not forbidden.
Other terms: no random term, no rail-type cost term except forcing segment end (546). max_search_nodes=10000. Cache disabled for complex waypoint destinations (destrail:~125).

## 3. Destination handling (station order)
- PfDetectDestination for OT_GOTO_STATION: any rail station tile of that station index whose track equals the trackdir track (destrail:~143-149). So ANY platform tile of the station is a target; there is no "nearest platform" selection, just lowest total cost via A*. CalcClosestStationTile only seeds the heuristic (destrail:~121-128, 161).
- The target is detected at the far end of the platform segment (follower skips platform tiles via tiles_skipped, costrail:401, 470) and PfDetectDestination(cur.tile,cur.td) is called on segment end if end reason is Station (581-588).
- Platform choice therefore depends only on: distance, curves, doubleslips, slopes, the red/LA/firstred/lastred signal terms, the platform-length penalty (0 / +800 / +4000), PBS reservation costs (800*(len) if the platform has a reserved tile and path is under PBS signal state), and heap tie order. There is NO explicit occupied-platform penalty for normal stations (only waypoints: occupied -> +lastred_penalty 1000, costrail:424-466, and via the PBS reservation term / red signal). (inf) Nearer/cheaper platforms are used more simply because they win the A* comparison; equal-cost platforms are separated only by tie order, and by occupancy-through-signals/reservations.
- After the pathfinder picks, with PBS the path is reserved only if the target is free: TryReservePath bails if !IsWaitingPositionFree(dest) (yapf_rail.cpp:202); train_cmd.cpp ChooseTrainTrack then marks stuck/waits; it does not re-pick another platform in that call (train_cmd.cpp:~2800-2900, inf from reading flow). Platform choice only changes next time a pathfind runs, i.e. when the red/reserved state is visible in costs.

## 4. When the choice is made
- ChooseTrainTrack (train_cmd.cpp:2778) is called from the train controller when entering a tile with more than one possible track (train_cmd.cpp:3439) and, with reserve_paths / path signals, from TryPathReserve (3004 area) when a train at a PBS signal / waiting point extends its reservation. If the entered tile already has our reservation matching the entry, it returns that track without pathfinding (2790-2793). Single-track tile returns immediately (2796+).
- So: decided at EACH tile with a real choice (junction), using signal states at that instant; A* explores the whole route to the station but only the first step is returned (yapf_rail.cpp:477-497). Under PBS the origin is the end of the current reservation (FollowTrainReservation, yapf_rail.cpp:459) so the choice is made at the end of the existing reservation, and then the whole path to the target (up to next safe waiting point/target) is reserved (TryReservePath 189-232). Without PBS (block signals), decision at every split ahead.
- Pathfinding errors: if the best path is not "found" best_intermediate_node (closest to destination by estimate) is used.

## 5. Worked examples (defaults)
A. Two equal-length parallel lanes, lane B has 2 extra 45deg curves.
  Each 45deg transition = +100. Unequal geometry also changes the tile base: a diagonal->corner->diagonal bend is 2 transitions (+200) and one tile costs 71 instead of 100 (-29) *if* the lane doesn't gain a tile; if lanes are "equal length" in tiles, the net per curve pair = +200 - (corner tiles: 71 vs 100 each) ; treat as (200 - 29*k) with k corner tiles. If both lanes use the same number of tiles and B has 2 curve transitions more with 1 corner tile: diff = 200 - 29 = +171 (or +200 when no corner tile cost difference, e.g. curves inside same-count tiles).
  Therefore lane A is strictly cheaper by ~171-200 points (~2 tiles). With empty network (no red signals) EVERY train takes A: A* picks min cost, equal-looking lanes are not balanced. For traffic balance, curves must match exactly (same count of 45deg/90deg transitions, slopes, doubleslips). Even equal totals give a deterministic tie -> same lane every time (inf).
B. Same lanes, A (cheaper by D=171..200) has first signal red (block occupied), block/entry signal.
  Penalty on A = LA[0] 500 + firstred 1000 = 1500; if A's path has no later green signal before the station, +lastred 1000 -> 2500. Train goes to B when cost_A + 1500 (or 2500) > cost_B, i.e. D < 1500 (or 2500): true for any realistic bend difference. If the red is the SECOND signal on the lane (first green): LA[1]=405 (+1000 lastred if last) -> 405..1405 vs D~200: still diverts. If the red signal is the 5th: LA[4]=180 (+1000 if last): diverts only if D<180 (or <1180 with lastred). Red combo/exit first signal: 500+10000.
  If first red signal is a 2-way signal and a choice was seen before it: lane A is pruned entirely (dead end) -> always B regardless of D.
  PBS: a red path signal adds nothing by itself; an occupied/reserved lane adds 300/tile (213 corner) for tiles up to the 5th passed signal, 800*len on a reserved platform. Reserved lane A over L tiles overlapping the track: +300*L. Diverts when 300*L > D.

## 6. Minimal model for our evaluator (units 100 = 1 tile)
Route cost = sum over tiles: 100 (diag) or 71 (corner tile)
 + 100 per 45deg transition; 600 per 90deg; 100 per doubleslip pair; 200 per uphill diagonal tile (and bridge ramp entry unless flat); 300 per level crossing; +100 per skipped bridge/tunnel/platform tile.
 + signals i=0.. in path order: if red and non-PBS: LA[i]=500+i*(-100+5i) (i<10); first signal red: +1000 (block/entry), +10000 (combo/exit); first red two-way signal after a split: prune lane.
 + at target: if last passed signal red: +1000 (non-exit), +10000 (exit); (green after red clears it).
 + station: 1000 per platform tile passed that is NOT the target platform (through-station); target platform: 0 if train fits exactly, +800 if longer, +4000 if shorter.
 + PBS only if signal state PBS (initial assumed) and < 5 signals passed: 300*(tiles overlapping reserved track), 800*(platform tiles) if platform has a reservation; +1500 passing the back of a PBS signal.
 + bridge speed: if limit < train max (within first 10 signals): 100*(vmax-limit)*(4+len)/vmax.
Decision: lane/platform with minimum total; ties are NOT randomised (deterministic heap order), so balance requires identical cost, and a red/reserved state diverts only when penalty > cost gap. Re-evaluated at every split tile with live signal states (first 10 signals), so "occupied block ahead" diverts trains only while the block is red when the train reaches the split.
