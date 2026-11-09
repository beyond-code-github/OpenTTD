# T1: tunnels in layouts, build steps, pricing, checker and rig (D154)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/t1`, branch `wt-t1`, head given in your launch prompt. You are the only writer there. Other agents work in sibling worktrees (one is changing `junction_line_router.*`, `junction_terrain.h` and `MakeJunctionTerrain` in `junction_game.cpp`): never touch those parts, sibling worktrees or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test openttd -j 8 > build.log 2>&1` (the game target too: the poster and GUI must compile).
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`; `"[junction]~[slow]"` then `"~[.]"` at the gate.
- Hardware note: this machine's CPU is unstable under heavy all-core load. A lone segfault/SIGILL is most likely that: rerun the case alone and report it.
- Plan: `/home/pete/OpenTTD/docs/junction-tool/PLAN-organic.md`; game rules for tunnels in `RECON-organic.md` §6. Decisions: rows D80, D152, D154 (this step's ruling: read it first).

## Why (settled, D154)
The organic search (PLAN-organic) will route movements under each other through tunnels (O2 adds them to the router). Before the router can emit one, everything downstream must carry, check, price, draw and build a tunnel. A tunnel behaves like a bridge for train movement and signalling (a wormhole between two heads, one block, no signals inside, surface track may cross the tiles between), so D154 represents it as a `JunctionBridge` with a `tunnel` flag: every place that treats bridges as wormholes then covers tunnels unchanged, and only the places that are about the structure itself branch on the flag.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- DECISIONS rows D154, D80.
- `src/junction_layout.h` (structs, `JunctionBridgeStep`, `JunctionPricing`) and in `src/junction_layout.cpp` every use of `bridges` / `JunctionBridgeStep` (grep: orientation ~114, JSON ~187, build steps ~294, estimate ~331-400).
- `src/junction_game.cpp`: `GameJunctionPricing::PriceBridge` and `GameJunctionStepPoster::Post` (~380-410).
- `src/junction_gui.cpp` ~1010-1030 (failed steps' bad tiles).
- `src/junction_route.cpp` `RenderJunctionAscii` (~210-260).
- `src/junction_rules.cpp`: every use of `layout.bridges` (grep; ~10 sites). You will list them in your report.
- `src/tunnelbridge_cmd.cpp` `CmdBuildTunnel` signature (grep `CmdBuildTunnel`), and `tools/ottd/gs/main.nut` ~410-470 (the rig's bridge building).
- The tests that cover bridges in build steps, estimate, orientation and JSON (grep `bridges.push_back` in `src/tests/`).

## Tests (red first; record each red)
Beside the existing bridge tests of each area (tag as they are):
1. Build steps: a hand-made layout with one tunnel (`{start, end, true}`) gives a `JunctionBridgeStep` with `tunnel == true` and the right tiles. Red: the member doesn't exist (record the compile failure, then the assertion with a stub).
2. Orientation: `ApplyJunctionOrientation` keeps the flag for all 8 orientations.
3. JSON: `JunctionLayoutToJson` writes `"tunnel":true` for a tunnel and nothing extra for a bridge (existing golden output unchanged).
4. Estimate: with the test pricing stub, a tunnel is priced by `PriceTunnel`, a bridge by `PriceBridge` (give the stub distinct prices and assert the total).
5. Checker: a hand-made layout where a straight line passes through a tunnel and another straight line crosses at right angles on the surface over a tile between the portals: `CheckJunctionRules` reports no conflict for the crossing tile, and the two portals are in one block (as the existing bridge block test checks). If an existing bridge-specific rule rejects this layout, stop and report which rule and message.
6. ASCII: `RenderJunctionAscii` draws `O` on a tunnel portal (bridges unchanged); tiles between portals are drawn as whatever lies on them.

## Changes
1. `junction_layout.h`:
   - `JunctionBridge` gains `bool tunnel = false; ///< A tunnel between the two portals instead of a bridge (D154).` Its doc: "A rail bridge or tunnel between two heads on the same row or column; ...".
   - `JunctionBridgeStep` gains `bool tunnel = false;`.
   - `JunctionPricing` gains `virtual CommandCost PriceTunnel(TileIndex start, TileIndex end) const = 0; ///< Test-only run of building a rail tunnel from start; an error when its far end isn't end.` Every implementation (game and test stubs) implements it.
2. `junction_layout.cpp`: orientation copies `tunnel`; JSON appends `,"tunnel":true` inside a tunnel's object; build steps copy the flag; the estimate calls `PriceTunnel` for tunnels (spans: a tunnel has no span tiles to clear; keep the D50 span handling for bridges only).
3. `junction_game.cpp`:
   - `PriceTunnel`: a test-only `Command<Commands::BuildTunnel>::Do` at `start` (`DoCommandFlag` as `PriceBridge` uses for its test), `TransportType::Rail` and the company's rail type as the other pricing calls get it; if it succeeds but `_build_tunnel_endtile != end`, return an error (`STR_ERROR_CAN_T_BUILD_TUNNEL_HERE` or the closest existing string; name it in the report).
   - `Post`: for `tunnel` steps, `Command<Commands::BuildTunnel>::Post(<that string>, CcJunctionStep, s.start, TransportType::Rail, this->railtype)`.
4. `junction_gui.cpp`: unchanged code path already adds both heads to the bad tiles; check it compiles.
5. `junction_route.cpp` `RenderJunctionAscii`: a tunnel's two head tiles draw `O`; no span glyph for tunnels.
6. `junction_rules.cpp`: no change unless test 5 fails (then stop). In the report, list each `layout.bridges` use with one line: "wormhole/block: covers tunnels" or "bridge-specific: <what>" and whether it should skip tunnels later.
7. `tools/ottd/gs/main.nut`: where a layout's bridges are built, a bridge with `"tunnel": true` is built with `GSTunnel.BuildTunnel(GSVehicle.VT_RAIL, <start tile>)` instead of `GSBridge.BuildBridge`.
8. The ghost (`junction_ghost*.cpp`): no change in T1; a tunnel shows only its portals' tile highlight if the existing code draws one for bridge heads, otherwise nothing. Report which.

## Do not
- Change the routers, the generator, `AddRasterToLayout`, the terrain snapshot, slopes or terraform.
- Add a separate tunnel struct or list.
- Change existing tests' expectations or the golden JSON for bridge layouts.

## Stop conditions
- Test 5 is rejected by an existing rule.
- `CmdBuildTunnel`'s parameters differ from the shape above in a way that needs a choice.
- Any existing test fails (beyond a lone hardware flake).

## Gates
1. Build `build-rel` `openttd_test` and `openttd`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction]~[slow]" 8`, then `"~[.]"`.
3. Commit: `Feature: Junction layouts carry, price, check and build rail tunnels`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test
- Types, functions, members, constants added or removed: names only
- The `junction_rules.cpp` bridge-use list (item 6) and the ghost finding (item 8)
- Manual checks not done (the game-side Post and the GS script are compiled/edited only; say so)
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
