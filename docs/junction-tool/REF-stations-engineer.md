# Engineer's reference stations (from screenshots, 2026-11-08)

**Caveat (engineer): "my stations are FAR from optimum".** They show shape and practice, not targets: use them as calibration cases and as baselines for the generator to beat (D119/D121).

Read from in-game screenshots at normal zoom, then measured exactly from the save with `scratchpad/savread` (platforms are 5 tiles in all four stations). Signal types and exact tile counts are not readable; items marked (?) need the savegame or the engineer to confirm.

## Stations shown
1. **Buborough Bay Woods**, wood drop at a sawmill: a few platforms, staggered in pairs along the line's diagonal; entry signals in front of each platform group; exits rejoin and leave to the right.
2. **Frindworth Valley**, factory pickup: about 6–7 roofed platforms. The incoming lines cross water on parallel bridges, so each lane gets its own track early, then runs straight to its platform past a signal. The exits fan out to parallel lines down the right edge.
3. **Fledhattan East**, goods drop with 4 incoming lanes, beside a town:
   - Platforms in pairs, each pair offset diagonally from the next: a staircase.
   - The four lanes arrive on separate rows, so their splits are staggered.
   - Exit tracks with signals before the merges (exit bays). The exits turn back round large loops and leave past a "Goods drop out" waypoint.
4. **Chinningpool Falls Factory Drop**, mega factory drop with 4 incoming lanes:
   - About 8–10 long platforms in a staircase along the diagonal.
   - The incoming lanes come from the upper right and cross each other on many doubled, staggered bridges: a bridge-built balancer, so (?) every lane reaches every platform.
   - They then fan into parallel tracks running down to the platforms, with a long run-in per platform where trains queue (entry bays). The entry signals sit on a diagonal line, one per track, following the staircase.
   - Exits leave from the platforms' far ends onto lines that run away at the side.
   - Engineer: "four lanes go in and only three come out". The exit side carries the same trains on fewer lines; see the open question below.

## Common pattern (candidate rules for station v2, extends D120)
- **R-S1 Staircase:** platforms are offset step by step along the line's diagonal, not lined up in a rectangle. Every platform has a straight run-in and run-out of equal length. That keeps the footprint narrow along the diagonal and the route costs balanced.
- **R-S2 Entry bays:** each platform has a straight run-in holding at least one train (1 TL (?)) behind its entry signal. The entry signals form a diagonal line matching the staircase.
- **R-S3 Early split:** lanes split, and cross by doubled staggered bridges where balancing needs it, well before the platforms. The tracks then run parallel to the bays; no split sits at a platform mouth.
- **R-S4 Exit bays:** each platform's run-out has a signal before its merge, so a train waiting to join never holds its platform.
- **R-S5 Staggered splits and merges:** with several lanes, each split or merge falls at its own step, so none shares a block with another lane's (as D113/D118).
- **R-S6 Turn-back exits (option, later):** exit lines may loop back rather than continue straight through.

## Open (need the savegame or the engineer)
- Is each station fully balanced (any lane to any platform), or does each lane feed a platform group?
- Platforms per step, and the offset between steps in tiles.
- Entry and exit signal types (pre-signal, path or block).
- Bay length in tiles relative to TL.
- Why 4 in / 3 out works at Chinningpool. Main reason (engineer): "there's a ML split just before entering the station drop so the four lines are far from saturated": only part of the four lines' traffic reaches the station (as RULES-stations S25: mixed lines can't all be full). Secondary candidates for the throughput model: empty trains are lighter and clear blocks sooner; dwell spreads departures more evenly than arrivals.
- Consequence for the generator: size the exit (and platform count) from the station's expected traffic, not from the incoming line count. The station's input is a traffic figure (from the throughput model or the player), and the number of exit lines is an output.
