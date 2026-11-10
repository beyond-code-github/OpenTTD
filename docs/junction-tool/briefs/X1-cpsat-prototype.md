# X1: does a constraint solver build our T in under 100 ms? (off-repo experiment)

This is a research experiment, not product code. You make the modelling choices listed as yours below, and report every one. Keep your context small: send solver logs to files and report numbers.

## Where
- Work only in `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/x1/` (create it). Never edit anything under `/home/pete/OpenTTD` (read-only reference is fine). Don't touch running OpenTTD processes.
- Python 3 in a venv there: `python3 -m venv venv && venv/bin/pip install ortools` (OR-Tools CP-SAT). Licensing is not a concern (engineer).
- The machine is unstable under all-core load: CP-SAT `num_workers` at most 4; never run two solves at once.
- At the end, copy `x1/` (without the venv) to `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/x1/`.

## Why
The engineer discussed this problem with another model, which proposed solving the whole junction as one constraint model (multi-commodity flow over track bits, bridges as a second level, minimise area) and claimed 15-45 ms. Our current approach (negotiated routing plus feedback, `src/junction_organic.cpp`) is slow. Measure whether the solver approach is fast on our real case before we choose.

## The case (flat ground, OpenTTD track geometry)
- Box W = 44, H = 16 tiles, x right, y down. OpenTTD tracks per tile: X (NE-SW edges), Y (NW-SE edges), UPPER, LOWER, LEFT, RIGHT (corner pieces; see `src/track_type.h`, `src/track_func.h` for which edges each joins: `TrackToTrackBits`, `TrackdirToExitdir`, `TrackdirReachesTrackdirs` etc.). A trackdir is a track with a direction (12). Edges: NE = -x, SW = +x, NW = -y, SE = +y.
- Ports (tile, trackdir on that tile; In = entering the box, Out = leaving): EIn (0,0) X_SW; EOut (43,0) X_SW; WIn (43,2) X_NE; WOut (0,2) X_NE; NIn (20,15) Y_NW; SOut (22,15) Y_SE.
- Six movements (commodities): EIn->EOut, EIn->SOut, WIn->WOut, WIn->SOut, NIn->WOut, NIn->EOut. Each must be a connected directed path of trackdirs from its In port to its Out port.
- Sharing: two movements with the same In port, or the same Out port, may use the same trackdir on a tile. A split (same In port: both enter a tile through the same edge and leave by different tracks) and a merge (same Out port: enter by different tracks and leave through the same edge) are allowed. Any other two different tracks on one tile used by different movements that overlap or cross (OpenTTD's `TracksOverlap`) are forbidden: no level crossings. Opposite directions on the same track are forbidden. A movement never uses two overlapping tracks on one tile.
- Bridges: a movement may jump straight along X or Y from a head tile to a head tile 2..11 tiles apart (1..10 middle tiles), both heads carrying that axis track in that direction; head tiles hold nothing else (two movements may share an identical bridge only when they could share its track). Bridge middles leave the surface free for other track; two bridges never pass over the same tile. No head on a port tile; no track on the box boundary except the port tiles and the main-line rows y = 0 and y = 2.
- Curves: no 90 degree turn on two consecutive pieces (a corner piece followed directly by the corner that completes a right angle); report whether you also modelled `curve_spacing` (the junction uses 4 half-units of straight between turns): yours to decide; first without, then with if time allows.
- Objective: minimise occupied tiles (any track or head), then pieces + 5 x bridge middle tiles.

## Yours to choose (report each)
The variable encoding (per tile and trackdir per movement, or per edge flow, or other), how bridges are represented (enumerated candidate bridges are fine), symmetry breaking, hints, search parameters.

## Measure
1. Model size (variables, constraints), build time in Python, and solve time: time to the first feasible solution, to the best found within 10 s and 60 s, and whether optimality is proven. Objective values over time.
2. The same on a smaller box (W 30, H 12, ports moved with the same pattern: E ports at x = 0 / W-1 on rows 0 and 2, branch ports at x = W/2 - 2 and W/2 on the bottom row) to see scaling.
3. An ASCII render of the best layout (one char per tile is fine; mark bridges) for each run.
4. Compare with our numbers: the organic negotiation's best clean T used 211 occupied tiles (with prios and rules); today's template generator 190 (with prios, signals, bays). Your model has no prios, signals or train-length rules, so note that it is a lower bound on what the full rules would need.

## Stop
If the model cannot find any feasible solution within 120 s on the 44 x 16 box, stop and report the size and the solver log summary.

## Report
Environment set-up done; encoding choices; per run: size, build time, time to first feasible, best at 10 s and 60 s, optimal or not, occupied tiles; ASCII renders; what you'd add next for the coop rules and your estimate of its cost; where the files are (scratchpad and OneDrive). When you have reported, stop.
