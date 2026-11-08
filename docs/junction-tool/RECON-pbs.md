# RECON: OpenTTD path signals (PBS) for a blocking-time model
Paths relative to /home/pete/OpenTTD/src. [I] = inference, not directly stated in code.
Key defaults (table/settings/pathfinding_settings.ini): pf.reserve_paths=false (l.87-89), pf.path_backoff_interval=20 (l.93-96),
pf.forbid_90_deg=true (l.43-44), wait_for_pbs_path=30 (days, l.78-81), reverse_at_signals=false (l.55-57).

## 0. Vocabulary
- SignalType: Block, Entry, Exit, Combo, Path (two-way PBS), PathOneWay. IsPbsSignal = Path|PathOneWay (rail_map.h:291).
- IsOnewaySignal = type != Path (rail_map.h:357): so every type except two-way Path is "one-way" for blocking purposes.
- Occupancy convention is the same for blocks and PBS: a vehicle is "on" the tile v->tile, which changes when the vehicle's
  reference point crosses the tile edge; the tail frees a tile (PBS) / block (signal.cpp via TrainMovedChangeSignals, train_cmd.cpp:3660)
  when the LAST moving vehicle changes tile (train_cmd.cpp:3560, ClearPathReservation; 3659 signal update). So "tail leaves" is identical for both.

## 1. What a path signal does on approach
- Safe waiting position (pbs.cpp:395-437, IsSafeWaitingPosition): (a) depot tile; (b) tile carrying a BLOCK signal (non-PBS) on the trackdir
  (the signal tile itself is safe); (c) the tile IMMEDIATELY BEFORE a tile with a PBS signal on our trackdir (single follow-on trackdir only;
  a split next tile is not safe); (d) when include_line_end: end of track, or next tile has a one-way PBS signal against us (treated as line end).
  Reservation never passes a block signal tile or a PBS signal tile: it stops at the tile before the PBS signal (T), the PBS signal tile S is NOT in it.
- Reservation is made as a path from the end of the current reservation to the first safe position: ExtendTrainReservation (train_cmd.cpp:2563-2668)
  walks single-track tiles reserving each; stops at a choice (split) or "target" (station/depot/waypoint tile) and hands over to YAPF
  (ChooseTrainTrack 2778-2945). YAPF reserves only up to the FIRST safe position on its path (yapf_rail.cpp:173-186, 389-398, 480-496; later
  nodes overwritten by earlier ones when walking back to origin; nodes beyond 2 signals ignored). So each reservation hop = signal-to-next-signal.
- When made (the "how far before the signal"): two triggers.
  (A) Look-ahead CheckNextTrainTile (train_cmd.cpp:2291-2345), called from the per-tick movement loop only when tick_counter % path_backoff_interval(20) == 0
      (3666) and on tunnel-exit (3606): if the head's tile is T and the NEXT tile S is unreserved with a PBS signal on our trackdir it calls ChooseTrainTrack(S)
      which sets S's signal GREEN and extends the reservation through S to the next safe position (train_cmd.cpp:2795-2802, 2818). Not done if on a platform that
      we stop at, or on a red BLOCK-signal tile (2325, 2332). So: reservation time = first tick with counter%20==0 after the head's centre enters T (0..19 ticks, avg ~10),
      retried every 20 ticks while on T (mark_stuck=false).
  (B) Fallback on entering S (3436-3439 -> ChooseTrainTrack, mark_stuck=true): same call. If it fails, S is set red again (2817), train is stopped at the tile edge
      (3459-3470 sets cur_speed=0) and flagged Stuck; retries via TryPathReserve every path_backoff_interval ticks (4094), reversing after wait_for_pbs_path days only if reverse_at_signals.
- Signal green: a PBS signal turns green ONLY inside ChooseTrainTrack (2801) (grep: only SetSignalStateByTrackdir(...Green) sites are 2660 [undo], 2801, signal.cpp:463).
  Otherwise signal.cpp:UpdateSignalsAroundSegment (428-465) gives an UNRESERVED PBS signal block-like behaviour: red if a train is in the segment ahead, ALSO red if the segment
  ahead has a split/junction (SigFlag::Split) or >1 entry (MultiEnter) (437-439), else green. Reserved PBS signals are skipped there (431).
  Head entering S sets it red again (3553-3556).
- [I] Stale-red effect: red_signals is read before ChooseTrainTrack (3414-3420) and tested after (3459), so a train that arrives at S while it was still red but succeeds in
  reserving stops dead for one tick (cur_speed=0, progress=255) and restarts next tick. Also TrainCheckIfLineEnds (3999-4015, TrainApproachingLineEnd 3865) clamps speed with
  _breakdown_speeds[x&0xF] over the last tile before ANY red signal tile, in both acceleration models; if look-ahead turns S green mid-tile the train re-accelerates. So a PBS signal
  whose segment ahead has a split costs a slowdown/stop unless the 20-tick look-ahead fires early on T (depends on tick phase and speed).

## 2. What blocks a reservation
- TryReserveRailTrack (pbs.cpp:80-138): plain rail fails if the track bit is already reserved or the new bit overlaps a crossing reserved track (rail_map.h:225-234 TracksOverlap);
  station tile fails if the tile is reserved; depot/crossing/bridge-end likewise. Whole platform: ReserveRailStationPlatform (yapf_rail.cpp:73-90) fails if ANY tile reserved.
- Occupied tiles: trains reserve under themselves (3507 on entry; ReserveTrackUnderConsist 3185), so occupied implies reserved. Reservation does NOT test vehicles directly.
- Safe target must be free: IsWaitingPositionFree (pbs.cpp:439-462) requires safe tile and the tile after it unreserved; TryReservePath checks it first (yapf_rail.cpp:200). Any failure -> all-or-nothing
  rollback (UnreserveSingleTrack loop 205-220; Extend backs out 2638-2663; FreeTrainTrackReservation 2475-2531 on 'found but not reservable' 2869/2911).
- Block signals ignore reservations: ExploreSegment (signal.cpp:~300-345) only looks at vehicles on tiles (EnsureNoTrainOnTrackBits / HasVehicleOnTile), never at reservation bits.
  Reservations never cross a block signal tile, so a block signal facing a PBS area is green iff no train is physically in the block.
- Freeing: ClearPathReservation (2434-2468) is called when the LAST vehicle (tail) enters the next tile (3560): the tile just left is unreserved TILE BY TILE (plain rail: just that track).
  Exceptions: station tile -> whole platform is freed only when the tail leaves the platform's last tile into a non-station tile (2455-2461, SetRailStationPlatformReservation 57-72);
  bridge/tunnel -> both ends freed when tail leaves (2438-2453). FreeTrainTrackReservation (forward reservation) is only for reversal/crash/failed re-reserve.
- Merging of opposing reservation: a green two-way path signal opposing the reserved path is forced red (2614, 2626; yapf_rail.cpp:118).

## 3. One-way vs two-way PBS
- PathOneWay: signal only on our side; from behind it blocks: HasOnewaySignalBlockingTrackdir (rail_map.h:547): Extend breaks (2569), FollowReservation breaks (pbs.cpp:~232), YAPF follower won't enter;
  IsSafeWaitingPosition treats an opposing one-way PBS as line end (pbs.cpp:429-433). A train facing it from the other side is stopped at red with reverse only via wait_oneway_signal (3463-3470).
- Path (two-way): signals on both sides; can be entered from behind, but only by reserving (green only via reserve; opposing side forced red when we reserve through). YAPF charges rail_pbs_signal_back_penalty
  (15 tiles) for passing a PBS signal from behind (yapf_costrail.hpp:262). Two-way PBS also sets SigFlag::Pbs so the segment state is Path (signal.cpp:336, 554).
- Passing from behind a two-way: ExtendTrainReservation sees an opposing PBS signal -> AddSideToSignalBuffer re-establishes default state after reservation freed (2519-2521).

## 4. Mixed areas
- Path signal -> block signals: reservation hop from S ends at the tile carrying the block signal (that signal tile is reserved, safe position (b)); the block signal state is then ordinary
  (green iff no train in its block). PBS segments are bounded by ordinary signals in ExploreSegment; the block signal at the end sees only vehicles.
- Block signal -> path signal: block B->S ends at S (one-way PBS facing the train is not an exit, not a Pbs flag); B green iff no train in B..S. At S normal PBS logic starts.
  Entering a PBS block through a non-PBS signal (segment state Path): TrainMovedChangeSignals (3172-3181, caller 3640-3656) makes the train reserve under itself and TryPathReserve; failure -> Stuck.
- Pre-signals (Entry/Exit/Combo): state logic (signal.cpp:441-455) only sees train presence + exit signal states. IsPresignalExit/Entry are false for PBS types, so a PBS signal is NOT an exit
  for an entry pre-signal (the entry sees no exit -> behaves as block signal). Reservations are invisible to pre-signals. [I] An entry pre-signal in front of a PBS area is green iff no train is in its block.
- reserve_paths=false (default): reservation starts only at a PBS signal (or entering Path segment via block signal); non-PBS areas behave as pure blocks (ChooseTrainTrack 2778-2790).

## 5. Occupation rule for the evaluator
Per PBS hop (S0 -> next safe tile Tn), per tile t on the hop, hold = [t_res, t_clear(t)]:
- t_res(all tiles of the hop) = the single instant when the extension succeeds = max(first look-ahead tick after head enters T = tile before S0 [A], time all blocking reservations are gone ahead).
  Model as t_res = max(head_enters_T + phi, ready), phi in [0,19] ticks (backoff phase; use worst case 20 or expected 10), ready = earliest time every tile + whole platform of the hop is unreserved.
  If the evaluator cannot do phi, use head_enters_T as optimistic and head_enters_T+20 ticks as pessimistic. If the hop's blockers clear later than that, the train waits at S0 edge (head at tile edge of S) and
  t_res = clear time rounded up to the next multiple of 20 ticks of the train's own tick counter (retry period) [B].
- t_clear(t) = tail reference leaves tile t (same as block rule "tail leaves"), tile by tile for plain rail/junction tiles; for a station platform ALL platform tiles clear at the same instant = tail leaves last platform tile;
  for bridge/tunnel both ends at tail exit.
- Unreserved hold is NOT begun at head arrival: tiles ahead of the head are held from t_res even though the head has not reached them (this is the extra occupation vs blocks). A tile behind the signal
  S0 in the previous hop is held until the tail leaves it; hops overlap at T/S0 (T last tile of hop k, S0.. first tile of hop k+1; S0 also occupied by head).
- Two reservations conflict iff they share a track bit or have overlapping crossing bits (X/Y on one tile; TracksOverlap) over overlapping time intervals; blocks conflict on whole signal segments. Opposing movements on the same tile conflict (same bit, any direction).
- Block-signal tail of a PBS hop: the tile with the block signal is held [t_res, tail leaves]; block segment beyond it is held head-arrival to tail-clear as today.
- Exit-of-hop safety: a hop is only granted if Tn and the tile after Tn are free (IsWaitingPositionFree) -- the next tile after the safe tile must also be unreserved, i.e. one extra tile of margin.

## 6. Station entry (one-way path signal S on the tile before the platform, after the last split)
- Train approaching: first hop ends at T (tile before S) if S sits behind T on the lane. At T look-ahead (A) or at S (B): Extend reserves S then sees the platform tile as a "target" and calls YAPF
  from the platform tile. YAPF picks among platforms by cost: reserved platform adds rail_pbs_station_penalty (8 tiles, per tile) (yapf_costrail.hpp:184-192, ini l.252-255); a different free platform is preferred,
  but if the best path is reserved at the target, TryReservePath fails (IsWaitingPositionFree 200) -> whole attempt rolls back, signal red, train waits (stuck flag) retrying every 20 ticks.
  [I] The train picks the platform at the throat split (the choice before S) and reserves up to the first safe tile; so at the split ChooseTrainTrack (mark_stuck=true) runs YAPF over the platforms with the penalty; if the lane's S hop is not reservable the train is held earlier (at the throat signal) instead of at S.
- Platform reservation covers every tile of the platform from entry to end (yapf_rail.cpp:73-90) at t_res (not as the train advances), held while loading, freed when tail leaves the last platform tile.
- Beyond the platform: the hop also extends to the first safe tile after the platform (platform end tile is safe if the exit tile has a PBS signal; with a block signal exit the block signal tile is reserved and
  must also be free; failure to find that path also fails the whole reservation: train_cmd.cpp:2889-2912). So a train is not granted a platform unless its exit hop is free.
- Waiting: train stands with head at the S tile edge (stopped by 3459 with cur_speed=0), tail extended back along the lane; the lane tiles behind S keep their previous-hop reservation until the tail clears them, so a queue of trains blocks the throat.

## 7. Original vs realistic braking
- Signal approach is identical: TrainApproachingLineEnd clamp (3865-3898) is acceleration-model independent; no signal look-ahead braking exists in this tree (grep lookahead in train.h/train_cmd.cpp: none).
- Look-ahead reservation distance is NOT braking-based: it is "head on T and counter%20==0" (2291, 3666), same under both models. Difference appears only for station stops (GetCurrentMaxSpeed 395-420, Realistic only) and
  curve/slope limits, which change the head's time on T and hence how many look-ahead ticks occur there.
- GetCurrentMaxSpeed realistic: station approach slowdown when the head is on a platform tile whose station we stop at; also CheckNextTrainTile is skipped then (2338).

## Open uncertainties
1. Tick-phase of look-ahead (tick_counter%20) is per-vehicle (not simulated deterministically in the evaluator); decide expected vs worst case. Confirm vehicle tick_counter definition.
2. Stale-red one-tick stop (3414-3459) and whether a train that receives green mid-tile loses speed: derived from code reading; verify in-game with the rig.
3. Whether the reference point for head/tail "tile entry" for PBS equals the block model's (v->tile of vehicle centre): same code path, so likely, but numeric tail offset not rechecked here.
4. Platform choice at the split: YAPF cost inputs (rail_pbs_station_penalty) depend on look-ahead depth (rail_look_ahead_max_signals=10); did not trace which node YAPF returns for multi-platform equal-cost cases.
5. Behaviour of trains standing in a platform: unchecked whether any code frees the forward reservation (station_cmd) while loading; assumed it keeps it.
6. Depots, level crossings, wormholes only skimmed.
