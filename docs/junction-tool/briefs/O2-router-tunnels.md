# O2: tunnels in the line router (PLAN-organic O2, D155)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o2`, branch `wt-o2`, head given in your launch prompt (contains O1, O1b and T1). You are the only writer there; never touch sibling worktrees or the main worktree.
- Build: configure once as in `briefs/O1-router-heights.md`; `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`; `"[linerouter]~[slow]"` to iterate; `"[junction]~[slow]"` then `"~[.]"` at the gate. A lone segfault/SIGILL under load is the machine: rerun alone and report.
- Decisions: DECISIONS rows D149, D152, D152a, D154, D155 (this step's ruling: read it first). Game rules: `RECON-organic.md` §6.

## Why (settled, D155)
Crossings in organic junctions mostly go under, by tunnel, and lines cross ridges by tunnel when that is cheaper than climbing. T1 made a tunnel a `JunctionBridge` with `tunnel = true` everywhere downstream. O2 lets the line router find tunnels, mirroring the game's rule (`CmdBuildTunnel`): the entry portal is an inclined tile rising in the travel direction with the track at its low edge; the tunnel runs straight on until the first tile whose lowest corner is at the track's height, which must be inclined the other way (the exit portal). Tunnels cross only at different depths.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`; DECISIONS D149, D152a, D154, D155; `RECON-organic.md` §6.
- `src/tunnelbridge_cmd.cpp` `CmdBuildTunnel` (the loop that finds the far end, ~680-725) and `src/tunnel_map.cpp` 48-60.
- `src/junction_route.cpp` `RasterizeJunctionRoute` (the `'B'` case) and `src/junction_line.{h,cpp}` `ExtendJunctionLineBridge`, `RasterizeJunctionLine`.
- `src/junction_layout.h` `JunctionBridge` (with T1's `tunnel`).
- `src/junction_line_router.{h,cpp}` (whole) and the LP6 bridge tests plus O1 ramp helpers in `src/tests/junction_line_router.cpp`.

## Tests (red first; record each red)
In `src/tests/junction_line_router.cpp`, tag `[junction][linerouter]`, L_R gap 1 on an axis heading, terrain built with the O1 helpers (inclines set by hand), bounds that forbid going round:
1. "FindJunctionLineRoute: tunnels through a ridge": a ridge 3 levels high and 6 tiles thick with entry and exit portal tiles on its faces, `slope_piece` high so climbing is dear: the route has one `T<n>` move; rasterized, each track has one `JunctionBridge` with `tunnel == true` and `z ==` the start height, portals side by side. Red: no `T` move (route climbs or fails).
2. "FindJunctionLineRoute: tunnels cross at different depths": an `existing` layout with a perpendicular tunnel at z 0 under the ridge, the line's portals at z 1: route found with a `T` move. Red: no route or no `T`.
3. "FindJunctionLineRoute: tunnels at the same depth don't cross": as 2 with the line at z 0: no route through the ridge by tunnel (the route climbs, or "no route" when climbing is impossible; assert no `T` move whose tiles cross the existing tunnel).
4. "FindJunctionLineRoute: a tunnel longer than the limit is refused": `max_tunnel_length` below test 1's length: no `T` move.
5. "RasterizeJunctionLine: a tunnel move": `"FT3F"` on an axis heading gives each track one tunnel `JunctionBridge` (tunnel true) with portals 4 apart; on a diagonal heading it is an error.
6. A `[slow]` case: tests 1-4's requests give equal cost with `use_heuristic` true and false; if they differ, stop.

## Changes
1. `junction_layout.h`: `JunctionBridge` gains `int8_t z = 0; ///< A tunnel's floor height (D155); unused for bridges.` (JSON: write `"z":N` only for tunnels; orientation copies it.)
2. `junction_route.cpp` `RasterizeJunctionRoute`: a `'T'` move followed by decimal digits n, from an axis piece only (error otherwise, no D110 closing), appends the entry portal, n middle tiles and the exit portal as `on_bridge` pieces and a `JunctionBridge{near, far, tunnel = true}` (z left 0: the router sets it).
3. `junction_line.{h,cpp}`: `ExtendJunctionLineBridge` gains a trailing `bool tunnel = false` passed through as `"T{}"` instead of `"B{}"`; `RasterizeJunctionLine` accepts `'T'` with digits like `'B'`.
4. `junction_line_router.h`, request: `std::function<Money(uint)> tunnel_price; ///< Per track and tunnel, by middle-tile count; tunnels are tried only when set (D155). Admissible while tunnel_price(n) >= (n + 2) * rail_piece.` and `uint max_tunnel_length = 64; ///< Largest middle-tile count (the game's max_tunnel_length).`
5. `junction_line_router.cpp`, beside the bridge successor (same `after_bridge` rule: no tunnel or bridge directly after either):
   - For every track (axis last piece, at height z): the entry portal is the next tile; it must be in bounds, not blocked, Buildable, not steep, `incline == TrackdirToExitdir(trackdir)`, `z_min == z`, and free of own and existing track and heads. Scan on: the first tile with `z_min == z` is the exit portal and must have `incline == ReverseDiagDir(exit dir)`, be Buildable, not steep, free, in bounds; the tiles before it are middle tiles (count n, at most `max_tunnel_length`), in bounds, and must not lie under a perpendicular own or existing tunnel with the same z (own tunnels from the path state with their z; existing ones from `request.existing->bridges` with `tunnel`). Without a terrain, no tunnels.
   - All tracks must find the same n; then `ExtendJunctionLineBridge(..., n, bridges, error, true)` gives the pieces; set each bridge's `z` to its track's z.
   - Cost per track: `tunnel_price(n)` + both portals' `clear_cost`; z unchanged; occupy both portals; middle tiles stay free for surface pieces. Waypoints (D150) pass on portals only.
   - Move text `fmt::format("T{}", n)`.

## Do not
- Change bridges' rules, ramps, the heuristic, the generator, T1's downstream code (beyond JSON/orientation of `z`), terraform (O3).
- Allow tunnels from diagonal headings or with unequal lengths across tracks.

## Stop conditions
- Any existing test fails (beyond a lone hardware flake); test 6's costs differ; the game's end-finding rule can't be mirrored as described (report what differs).

## Gates
1. Build: no errors, no new warnings. 2. `"[junction]~[slow]"`, then `"~[.]"`. 3. Commit: `Feature: Junction line router tunnels under ridges and other tracks`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence per test; names added; suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
