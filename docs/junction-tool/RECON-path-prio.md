# RECON: path-signal prio (extra-track priority merge)

Date: 2026-11-07. Read-only recon. Sources: junctionary `Priority Merge.mediawiki` (PM), ABR text, `src/signal.cpp`, `pbs.cpp`, `train_cmd.cpp`, `rail_map.h`.
Marked **[inferred]** where the text does not say it. Images viewed (4): `images/Priority merges.png`, `coop/files/Pbs_presig_prio.png`, `coop/files/Standard_prio_pbs.PNG`, `coop/files/Standard_prio.PNG`. At this resolution signal types and facings are not legible, so the diagram below comes from text plus code, not from the pictures.

## 1. How the path-signal prio is built

- PM, "Priority Merges Using Extra Track": "Longer priority can be designated by connecting track to activate the main merging signal upstream. When a train on the prioritized track drives onto a tile connected to the unused track it tells the signal on the merging track to read red. This construction can be made with both path signals and regular block signals... The path signal construction needs only a single block signal on the merging track, while the block signal construction requires pre-signalling on the unused track to prevent trains from signalling themselves to stop."
- PM, same section: segment the prio so any train length touches a rung: "This can be prevented by splitting longer priority into segments."
- So the path version is: ML signals are path signals (nothing special added), the prio track carries NO signals, and the merging track has ONE plain block signal S. Facing of S: toward the merge (one-way, **[inferred]**). The wiki gives no facing or type detail beyond "block signal".
- Rungs: PM does not use the word. The pictures show perpendicular track pieces crossing the ML (90 degree crossings). Code confirms a crossing tile is X plus Y on one tile.

```
 ML  --PS--+-----PS--+-----PS--+-----PS----- merge tile ===> ML onward
   (path)  |   rung  |   rung  |   rung  /
           |         |         |        /
 prio  ----+---------+---------+-------+   (no signals, hangs off the
 track                                      merging track after S) [inferred]
 merging line ---- S (plain block signal, one-way toward the merge) ---/
```
- How the rungs link blocks (code, **mechanism inferred from reading**): `ExploreSegment` floods a signal block tile by tile and stops at any tile carrying a signal on the entered track (signal.cpp:318-337, `continue` after the signal handling). On a crossing tile (tracks = X|Y, not HORZ/VERT) the exit loop tests `tracks` (all bits), not `tracks_masked` (signal.cpp:352-353), so the flood enters on Y and leaves on X. The unsignalled prio track plus every rung therefore glues S's block to each ML block that has a rung.
- Result: S's block = merging track after S + prio track + all rung-linked ML blocks (the ML path signals stop the flood at the block edges). S goes red when any vehicle is on any of those ML tiles (`HasVehicleOnTile` / `EnsureNoTrainOnTrackBits`, signal.cpp:308-315).
- Why the path version needs no pre-signals: the ML path signals do not take their state from that glued block. signal.cpp:431 `if (IsPbsSignal(sig) && GetRailReservationTrackBits(tile).Test(track)) continue;` and 437-439 "Turn path signals red if the segment has a junction or more than one way in": a path signal is red by default and green only through a reservation. With block signals on the ML the glue would make every ML signal see "train in block" (self-stop), which is the failure the wiki names.

## 2. Mechanics in game

- One-way path signal at the merge seeing ML occupancy through rungs: no. A path signal does not read block occupancy (signal.cpp:431-439; its green comes from `TryPathReserve`). That is why the wiki puts a plain block signal S at the merge. **[inferred]** from the code.
- Does a reservation ever take a rung? No (**[inferred]**, three code facts). (a) Track X on a crossing tile does not connect to Y (`DiagdirReachesTrackdirs`, used in `IsSafeWaitingPosition`, pbs.cpp:395-436; train_cmd.cpp:2340 strips `TrackCrossesTracks` in look-ahead). (b) Reservation is per track: `TryReserveTrack` (rail_map.h:225-234) sets bit X; it fails only if the result `TracksOverlap` (a crossing track already reserved). X and Y reserved together would fail. (c) Nothing ever routes onto Y, so no rung is reserved and the prio is not blocked by reservations.
- Does a reserved but unoccupied ML block count as occupied for S? No. S is a block signal, so its flood looks only at vehicles, not at reservation bits (signal.cpp:308-315). **Risk [inferred]:** a train in an ML block that has no rung can hold a reservation reaching the merge block; S is green, the joiner passes S, and then its `TryPathReserve` can fail. Code: train_cmd.cpp:3640-3652, a train entering a Path block through a non-PBS signal is `MarkTrainAsStuck` if it cannot reserve; the comment there assumes this "should" never happen. So a gap in rung coverage turns into a stuck joiner on the merge tile (and not just a late merge). Needs a game test.
- Reservation also acts the other way: a joiner that passes S reserves the merge tile and ML track to the next safe position, so ML trains' path signals stay red until it clears (normal PBS merge behaviour, **[inferred]**).
- Link to R14 "Do not merge with PBS signals": consistent. The merge signal S is a block signal, not PBS.

## 3. Mixed blocks (path signals plus block/pre-signals)

- A block flood is typed by its entry signals: `SigFlag::Pbs` is set when a signal facing into the block is a path signal (signal.cpp:330) or is a two-way signal (signal.cpp:336). `UpdateSignalsInBuffer` then returns `SigSegState::Path` (signal.cpp:554-555).
- `SigSegState::Path` matters to trains, not to signal colours: trains reserve paths in such a block (train_cmd.cpp:2104, 2399). A block signal facing a Path block still works as a block signal: `TrainMovedChangeSignals` (train_cmd.cpp:3172-3183) flags "a PBS block with a non-PBS signal facing us", and the train then reserves a path on entering (train_cmd.cpp:3640-3652).
- Pre-signal logic in a mixed block is not touched by PBS: entry and combo go red iff a pre-signal exit exists in the block and none is green (signal.cpp:451-453). Only `IsPresignalExit` types count (rail_map.h:344-347: Exit or Combo); path signals are not exits, and a path signal facing out does not count as an exit.
- Consequence: a pre-signal prio chain beside path-signal ML works as pre-signals on the chain: each prio segment's exit state comes from the flood of its own ML block. A path signal in the same block does not hide the vehicle presence from the exit logic. **[inferred, not run]** Mixed typing is not rejected anywhere in signal.cpp.
- Caveat 1: the flood glue also applies to the pre-signal case; the pre-signal chain breaks the glue into one prio segment per ML block, which is the reason for it (R24).
- Caveat 2: pre-signal exit colours are flood-driven and ignore reservations, so mixing keeps the same "reserved but unoccupied" blind spot as section 2, but every exit watches a single ML block (finer granularity).

## 4. CPU cost

- No source says path signals cost more CPU. ABR12 only says waiting trains "eat barely any computing power" and "for lower cpu usage there are smaller maps, not less trains". ABR03 only says PBS "abuse" hurt flow, not CPU.
- Code: block signals cost one flood per change (`ExploreSegment`), bounded by buffers `SIG_TBU_SIZE`=64 signals, `SIG_TBD_SIZE`=256 open tiles, `SIG_GLOB_SIZE`=128 (signal.cpp:25-27). On overflow the block is `Full` and the update is abandoned (signal.cpp:406, 487-560). A glued prio block (many rungs, long prio) is bigger than a normal block; watch the 256 open-node cap and the 64-signal entry cap on long prios. **[inferred]**
- Path signals add `TryPathReserve` plus `CheckNextTrainTile` look-ahead per train (train_cmd.cpp:2293-2342, 3666, called every `path_backoff_interval` ticks); YAPF per search is capped by `pf.yapf.max_search_nodes`, default 10000 (table/settings/pathfinding_settings.ini:102-105). Cost scales with trains times path length, not with the number of signals.
- `pf.reserve_paths` (train_cmd.cpp:2104, 2381) makes every train do reservation in all blocks; a network of block signals avoids that unless the setting is on. No numbers available.

## 5. Recommendation

- Recommend (b): the pre-signal prio always, with path signals elsewhere on the junction, provided the in-game check below passes. Reason: the pre-signal prio is the form our generator already plans (D20/D25 ruling pending) and it keeps S-style occupancy per ML block, avoiding the unreserved-block risk of (a). The mixing needs no special case in `signal.cpp` as read.
- Fall back to (a) only if the check fails or a junction is path-signalled throughout. Reject (c): it removes the prio feature entirely for no code reason.
- Check in game:
  1. Mixed block: path ML signals plus pre-signal prio chain; ML trains of each length pass the merge; does the joiner wait at the entry signal while an ML train is anywhere over the prio?
  2. Path-version risk: ML train in a block with no rung but reserved through the merge tile: does the joiner enter and get stuck (train_cmd.cpp:3640)? Run with `reserve_paths` on and off.
  3. A train wholly between two rungs (shortest train, section "Flawed priority" in PM): confirm the gap rule holds for pre-signal segments.
  4. Pre-signal chain on a prio crossed by rungs: entry, combo, exit placement stays correct when the prio track is also flooded from the ML side.
  5. Signal update cost and the `Full` overflow on the longest prio the generator can make.
  6. Both directions: no train routes onto the prio track (dead-end) when the merging line forks into it.

## Gaps

- Pictures not legible at this size; the signal types, facings and the exact attach point of the prio track to the merging track are inferred.
- PM does not use the word "rung", does not say where S sits relative to the prio branch, and gives no facing.
- No source describes the pre-signal chain beside path-signal ML directly (only the picture name `Pbs_presig_prio.png`, R24).
- No measured CPU data for path vs block signals; only the code structure above.
- YAPF's treatment of reservations when choosing between the merge tile and ML (reserved-but-free) was not read beyond `pbs.cpp`.
- All code reasoning is by reading; nothing was built or run.
