# RECON: what a junction layout costs YAPF (compute, not route penalties)

Source: /home/pete/OpenTTD/src/pathfinder/yapf/ (paths below relative to it unless noted). Read-only recon; no game run, so anything marked GUESS is unmeasured.

## 1. What ends a segment

Reasons are the `EndSegmentReason` enum, `yapf_type.hpp:17-35`. Segment walk is `PfCalcCost`, `yapf_costrail.hpp:~335-565`. Reasons that set a bit and `break` the tile loop:

| Reason | Where set | Notes |
|---|---|---|
| ChoiceFollows | costrail:527 | next tile offers >1 trackdir (`new_td_bits.Count() > 1`). The normal junction boundary. |
| DeadEnd | costrail:515 (follow fails, not rail-type) | track ends / `Follow` refuses |
| DeadEnd | costrail:208 (`SignalCost`) | one-way signal facing against us (`has_signal_against && !has_signal_along && IsOnewaySignal`) |
| DeadEnd | costrail:232 | first red 2-way signal treated as EOL (only when `choice_seen`, no signals passed yet, setting `rail_firstred_twoway_eol`) |
| RailType | costrail:513, 548 | rail type changes |
| Station | costrail:475 | `follower->is_station`; possible target; one segment ends per station platform |
| Waypoint | costrail:466 | possible target |
| Depot | costrail:422 | possible target |
| SafeTile | costrail:480, 519, 537, 540 | ONLY when `TrackFollower::DoTrackMasking()` (the "find nearest safe tile" pathfinder, `CFollowTrackFree*`, yapf_rail.cpp:~340-355). Not set in the ordinary route search. Triggers on PBS signal ahead / non-PBS signal along / reserved-tile end. |
| InfiniteLoop | costrail:554 | next tile/trackdir equals segment start |
| SegmentTooLong | costrail:558-563 | segment cost > `MAX_SEGMENT_COST` = 10000 (costrail:58), i.e. roughly 100+ straight tiles; only on `Railway` tiles |
| PathTooLong | costrail:502 | only with `max_cost` set (nearest-depot search) |

Internal-only, not cached (`yapf_type.hpp:17-35`): PathTooLong, FirstTwoWayRed, LookAheadEnd, TargetReached.

Important negatives (evidence: no `Set` call for them in the loop):
- Ordinary signals (block, entry, exit, combo, PBS, one-way when facing along) do NOT end a segment. `SignalCost` (costrail:~205-270) only adds cost, bumps `n.num_signals_passed`, and records `segment->last_signal_tile/td`. A segment can contain many signals.
- Tunnels, bridges and level crossings do not end a segment (follower skips the tiles; costrail:~408 `tiles_skipped`).
- Plain stations are Station-ended, so a through-station splits segments; a junction without stations has none of these.

Which create a node in the open list: every segment end creates children only through `PfFollowNode` (yapf_rail.cpp:254-259 for route search; :341-347 for safe-tile search) -> `AddMultipleNodes` (yapf_base.hpp:231-239) which makes one node per outgoing trackdir of the next tile. So each node = one segment start (tile, trackdir). A node enters the open list only if `PfCalcCost` returns true (base `AddNewNode`, yapf_base.hpp:~253+). `PfCalcCost` returns false (node dropped, never open/closed) for PathTooLong (costrail:579) and for any reason in `ESRF_ABORT_PF_MASK` = {DeadEnd, PathTooLong, InfiniteLoop, FirstTwoWayRed} unless a target was seen (costrail:~600, mask at yapf_type.hpp:62-67). So dead-end and one-way-blocked branches cost one segment walk but add no node. RailType/SegmentTooLong/Station/etc. ends do continue as nodes.

## 2. Cache

- Global segment cost cache: `CSegmentCostCacheT` (yapf_costcache.hpp:66-95), a hash (14 bits) + deque of `CYapfRailSegment`. Key = `(tile << 4) | trackdir` of the segment start (yapf_node_rail.hpp:20-35). So yes, keyed per trackdir, per segment start only; the end tile is stored, not keyed.
- Stored per segment: cost, end tile/trackdir, `end_segment_reason & ESRF_CACHED_MASK`, last signal tile/td (yapf_node_rail.hpp:66-78; costrail:~600-608). On a cache hit the tile walk is skipped entirely (costrail:~375-395) and only the last signal's current state is re-read.
- Invalidation: a single global counter `s_rail_change_counter` (costcache:45-52); `stGetGlobalCache` flushes the WHOLE cache when the counter differs (costcache:125-133). Counter bumped by `YapfNotifyTrackLayoutChange` (yapf_rail.cpp:694-696), called from rail_cmd.cpp (e.g. 600, 743, 1204, 1526), station_cmd.cpp, tunnelbridge_cmd.cpp, road_cmd.cpp (crossings), waypoint_cmd.cpp, plus signal edits. It is also called with `INVALID_TILE` after every successful path reservation whose destination node is cache-eligible (yapf_rail.cpp:~229-231). Signal colour changes do not flush (state is re-read).
- Crucial limit: the global cache is used only when `n.parent->num_signals_passed >= sig_look_ahead_costs.size()` (costrail:637-641, `CanUseGlobalCache`). Size = `rail_look_ahead_max_signals`, default 10 (pathfinding_settings.ini ~line 210). Otherwise the node gets a throw-away entry in `local_cache` (costcache:148-150) and the segment is re-walked every pathfinder call. Also `DisableCache(true)` for nearest-depot with a distance limit and for complex-waypoint orders (yapf_rail.cpp:~277, destrail:~138).
- Consequence: the part of the search within the first 10 signals of the train is never cached; only segments reached after >= 10 signals are, and every reservation flushes them again (GUESS: in a busy network the global cache hit rate is low; the code makes it a long-range optimisation only).

## 3. When the pathfinder runs for a train

- `ChooseTrainTrack` (train_cmd.cpp:2778) is the entry. Callers: tile entry of the moving front, train_cmd.cpp:3439 (`IsMovingFront()`); `TryPathReserve` path, :3004; :2342.
- Early exits that skip YAPF: a suitable reserved track already exists on the tile (:2788-2790); only one track available and no PBS signal needing a reservation (:2793-2804).
- Otherwise, with reservation on (`pf.reserve_paths` or forced, :2780): `ExtendTrainReservation` first; if the reservation already ends at a safe waiting position, quick exit without YAPF (:~2822-2830). Otherwise `DoTrainPathfind` -> `YapfTrainChooseTrack` (yapf_rail.cpp:597) -> `ChooseRailTrack` (:455-503): one `FindPath` from the end of the existing reservation (`FollowTrainReservation`), then walks back and `TryReservePath`.
- So it is NOT only at choice tiles: it runs at each tile entry where the train has no reservation ahead (typically whenever the reserved path ended at a non-safe point, e.g. after passing a block signal chain), and at single-track tiles when a PBS signal needs a reservation. It runs from the end of the current reservation, i.e. possibly far ahead of the train. With no path reservation, it runs at tiles with >1 track (choice tiles) only.
- Other callers: nearest depot (yapf_rail.cpp:670, runs forward and reverse), nearest safe tile (:683), check reverse (:606; train_cmd.cpp:3036).
- Search loop: `FindPath` (yapf_base.hpp:152-175): pop best open node; stop if it is a destination (:157-160); `PfFollowNode`; stop when closed count >= `max_search_nodes` (default 10000, ini ~line 105; checked at :167). Each iteration = one closed node = one segment expanded.
- Heuristic: route search uses octile distance to destination tile (`PfCalcEstimate`, destrail:~193) so it is A*; depot and safe-tile searches use `estimate = cost` (destrail:~67, ~102), i.e. plain Dijkstra, expanding everything cheaper than the nearest target.

## 4. What drives compute, and what does not

Cost model (derived): per pathfinder call, work ~ (nodes closed) x (follow + child node creation) + (tiles walked in uncached segments) + sorting in the open list. Nodes closed ~ number of segments whose cost+estimate is below the best destination's.

Drivers, most to least important:
1. Number of reachable segments before the destination is popped = branching of choice tiles x depth (each ChoiceFollows doubles the frontier). Layouts with many alternative parallel routes of similar cost make YAPF expand all of them before settling (A* with ties). Most important.
2. Number of choice tiles on and near the path (it is the same quantity as 1 seen locally: a choice tile with k exits creates k nodes, each requiring a `PfCalcCost` segment walk, even if k-1 are later discarded).
3. Segment length in tiles: only the uncached walk is linear in tiles (inner loop, costrail:~367-563), and a hot loop per node. Secondary: roughly linear, but short segments mean more nodes for the same tile count.
4. Signals: each adds a little per-tile work, but crucially `num_signals_passed` controls cache eligibility (needs >= 10) and look-ahead cost/speed penalty window (costrail:~512). A signal-dense hub keeps the search in the uncached regime. They never end a segment or create nodes (except one-way-against = pruning DeadEnd).
5. Stations/depots/waypoints inside the hub: split segments and add possible-target checks (`PfDetectDestination` at each such end).

Do not matter (for node count): signal type or colour (cost only), curves and slopes (cost only), tunnels/bridges (no split), track length beyond tile walk, signals along a segment.

Dead ends: a segment ending in DeadEnd costs one walk but adds no node; one-way blocking likewise. So dead-end spurs and one-way-blocked wrong-way exits are cheap (prune), unlike two-way alternative routes which are fully expanded. Reservation-bearing PBS layouts: reserved-tile cross penalties change ordering only.

## 5. Proposed countable measure

Define the junction as a directed graph of segments (nodes = tile,trackdir starts; edges at choice tiles), exactly as YAPF sees it: segment = maximal run ending at a choice tile / station / waypoint / depot / dead end / one-way-against / rail-type change. Drop branches that end in DeadEnd (including one-way blocked) from node counts but count them as one walk.

Measure A (cheap, closest to the real thing): for each through path (entry e -> exit x) with path cost C(e,x) from the tool's own cost estimate, N(e,x) = number of segments reachable from e (honouring one-way signals) whose cost-to-reach is < C(e,x), plus 1 per pruned DeadEnd child. Report the sum or mean over all entry/exit pairs, and the max. Compute by Dijkstra from each entry over the segment graph, bounded at the hub boundary (treat each exit leg as a single terminal segment). This mimics YAPF expanding every node cheaper than the target.
- Fidelity: faithful to the closed-node count in the Dijkstra case (nearest depot / safe tile; exact if costs match) and an upper bound in the A* case (octile estimate prunes branches heading away from the destination; GUESS: by 2-3x in a hub, since the heuristic is crude and ties are common). Does not model caching (GUESS: ignore, since the first 10 signals are uncached) or reservations.

Measure B (simplest fallback): segments per through path = mean number of ChoiceFollows boundaries crossed + 1, and total directed segment count of the hub. Fidelity: captures only the on-path term; blind to wasted exploration (GUESS: correlation moderate, because wasted exploration scales with branching, which correlates with choice count).

Measure C (tie-break secondary): total uncached tiles walked = sum over expanded segments of their length (cost units from A). Fidelity: second-order (GUESS: under 30% of per-call time beside node handling; unmeasured).

Recommended tie-break key: (A.sum, then B, then C). Validate by running a hub in-game with `debug=yapf=5` (the `Facility::Yapf` notice in yapf_base.hpp:~175 prints rounds, open, closed per call, "closed" being the node count A predicts) and comparing A to closed counts for the same entry/destination.
