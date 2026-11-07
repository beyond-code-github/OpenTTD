# Brief: 1a step 6, drawing seams for the ghost

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Work only in the worktree `/home/pete/OpenTTD-wt/step6`, on branch `junction/1a-step6`, head `{HEAD}`.** It has its own built `build/` directory (Ninja, clang-18). Don't touch `/home/pete/OpenTTD` or any other worktree; other agents are working there.
- Plan: `/home/pete/OpenTTD/docs/junction-tool/PLAN-1a.md`, the "Drawing seams" section, which was revised after this worktree was branched. Read it from that absolute path. This brief is authoritative where they differ.
- **Purpose.** The ghost of a junction must be drawn with the game's own sprite code, but on tiles that are really grass. So the drawing functions must stop reading the rail type, ground type, signal type and so on from the map, and take them as parameters instead.
- **Rebase-friendliness rule (D42):** this fork will be rebased onto upstream OpenTTD for years. Change as few upstream lines as possible: add parameters, replace a map read with a parameter, remove `static`. Never reorganise or reformat upstream code. Ghost-only code goes in new `junction_*` files where possible.
- **Real tiles must draw exactly as before.** Drawing can't be unit-tested. Correctness comes from making only mechanical substitutions, and from the engineer's in-game check afterwards.
- The baseline in this worktree is 100 test cases: 99 pass, plus one known-red outer test, "JunctionLayout outer: test junction on flat clear land prices exactly". It's fixed on another branch.

## 2. Read first, once each (really read them)

- `CODINGSTYLE.md`, in full.
- `src/viewport.cpp` lines 550–600 (`DrawGroundSpriteAt`, `DrawGroundSprite`), 660–700 (`AddSortableSpriteToDraw`) and 820–860 (`AddChildSpriteScreen`). Also `src/viewport_func.h`.
- `src/rail_cmd.cpp`:
  - 1895–1960 (`DrawSingleSignal`)
  - 2095–2300 (`DrawTrackBitsOverlay`, `DrawTrackBits`)
  - 2380–2500 (the rest of `DrawTrackBits`, `DrawSignals`, and `DrawTile_Track`'s calls)
  - 2600–2640 (`DrawTrainDepotSprite`, the precedent for `INVALID_TILE` sprite lookups)
- `src/signal_func.h` lines 15–55, and in `src/rail_map.h`, the definitions of `IsSignalPresent`, `GetPresentSignals` and `HasSignalOnTrackdir`.
- `src/tunnelbridge_cmd.cpp`:
  - 150–205 (the sprite table helpers)
  - 1080–1130 (`DrawBridgePillars`)
  - 1440–1545 (the bridge head branch of `DrawTile_TunnelBridge`)
  - 1555–1700 (`CalcBridgePiece`, `GetBridgeTilePillarFlags`, `DrawBridgeMiddle`)

## 3. Tests

Drawing has no unit-test seam in this codebase, so there are **no new unit tests** in this step. That's named in the plan. The gates are a clean build, the unchanged unit suite, and the engineer's in-game check (section 7). Don't add tests.

## 4. Changes (four commits, in this order)

### Commit 1: viewport ghost palette override

In `src/viewport.cpp`:
1. Add `static PaletteID _viewport_ghost_palette = PAL_NONE; ///< Palette forced on every sprite while drawing a ghost; PAL_NONE when off.`
2. Add a documented `void SetViewportGhostPalette(PaletteID pal)` that sets it.
3. In each of `DrawGroundSpriteAt`, `AddSortableSpriteToDraw` and `AddChildSpriteScreen`: once the function's existing handling of its own `transparent` argument has run (where it has one), and before the sprite is used, add:
   ```cpp
   if (_viewport_ghost_palette != PAL_NONE) {
   	SetBit(image, PALETTE_MODIFIER_TRANSPARENT);
   	pal = _viewport_ghost_palette;
   }
   ```
   If the function's existing transparent branch also clears other bits, such as `PALETTE_MODIFIER_COLOUR`, do exactly what it does, but with `_viewport_ghost_palette` as the palette. If `DrawGroundSprite` just forwards to `DrawGroundSpriteAt`, don't touch it.

Declare `void SetViewportGhostPalette(PaletteID pal);` in `src/viewport_func.h`, next to similar drawing declarations.

Message: `Codechange: Allow forcing a palette on viewport sprites, for ghost drawing`

### Commit 2: track drawing takes its values as parameters

In `src/rail_cmd.cpp`:
1. **Change the signatures:**
   - `static void DrawTrackBits(TileInfo *ti, TrackBits track, RailType rt, RailGroundType rgt, TrackBits reserved, TileIndex sprite_tile)`
   - `static void DrawTrackBitsOverlay(TileInfo *ti, TrackBits track, const RailTypeInfo *rti, RailGroundType rgt, TrackBits reserved, TileIndex sprite_tile)`

   Update their doc comments with the new parameters. `sprite_tile` is "tile used for NewGRF sprite resolution; INVALID_TILE for ghosts".
2. **Inside these two function bodies only,** replace:
   - `GetRailType(ti->tile)` with `rt`
   - `GetRailGroundType(ti->tile)` with `rgt`. Delete the local variable line if it only held that read.
   - `GetRailReservationTrackBits(ti->tile)` with `reserved`. Keep any surrounding `_settings_client.gui.show_track_reservation ? ... : TrackBits{}` condition as it is.
   - `HasReservedTracks(ti->tile, X)` with `reserved.Any(X)`. First check `HasReservedTracks` in `rail_map.h` to confirm it's exactly `(GetRailReservationTrackBits(tile) & X)` being non-empty. If it isn't, stop.
   - In `GetCustomRailSprite(rti, ti->tile, ...)` calls, the `ti->tile` argument with `sprite_tile`
   - The call `DrawTrackBitsOverlay(ti, track, rti)` with `DrawTrackBitsOverlay(ti, track, rti, rgt, reserved, sprite_tile)`
3. **Update the call in `DrawTile_Track`** (around line 2480) to `DrawTrackBits(ti, rails, GetRailType(ti->tile), GetRailGroundType(ti->tile), GetRailReservationTrackBits(ti->tile), ti->tile);`. Check for any other callers of the two functions with grep. If there are others, update them the same way and list them.
4. **If a helper called from these two functions also reads** the rail type, ground type or reservation from `ti->tile`, stop and report. Don't change helpers.
5. **Add the ghost entry point** after `DrawTrackBits`:
   ```cpp
   /**
    * Draw track on a tile for the junction placement ghost, without reading the tile's map data.
    * @param ti Tile to draw on (must be flat).
    * @param track Track pieces to draw.
    * @param rt Rail type to draw.
    */
   void DrawRailTrackGhost(TileInfo *ti, TrackBits track, RailType rt)
   {
   	DrawTrackBits(ti, track, rt, RailGroundType::Grass, {}, INVALID_TILE);
   }
   ```
6. **Create `src/junction_ghost_draw.h`** (licence header, `@file`, guard `JUNCTION_GHOST_DRAW_H`). Include the minimal headers for the types used, and declare `void DrawRailTrackGhost(TileInfo *ti, TrackBits track, RailType rt);`. Add it to `src/CMakeLists.txt` alphabetically, after `junction_layout.h`. Don't declare it in `rail.h`; the upstream header stays untouched.

Message: `Codechange: Pass rail type, ground type and reservation into track drawing`

### Commit 3: signal drawing takes its values as parameters

In `src/rail_cmd.cpp`:
1. **Change `DrawSingleSignal`** to `static void DrawSingleSignal(TileIndex tile, const RailTypeInfo *rti, Track track, SignalState condition, SignalOffsets image, uint pos, SignalType type, SignalVariant variant, TileIndex sprite_tile)`.
   - Delete its two map reads of the type and variant, which are now parameters.
   - Pass `sprite_tile` instead of `tile` to `GetCustomSignalSprite`.
   - Keep `tile` for everything else, such as the position.
   - Update the doc comment.
2. **Extract the position enumeration from `DrawSignals`.** Move the body of `DrawSignals`, everything after the `MAYBE_DRAW_SIGNAL` lambda definition, unchanged into:
   ```cpp
   /**
    * Call a function for every possible signal position on a tile with the given tracks.
    * @param rails Tracks on the tile.
    * @param f Called as f(uint8_t signalbit, SignalOffsets image, uint pos, Track track).
    */
   template <typename F>
   static void ForEachSignalPosition(TrackBits rails, F &&f)
   ```
   Inside the moved body, `MAYBE_DRAW_SIGNAL(...)` calls become `f(...)` with the same arguments.

   `DrawSignals` keeps its signature and becomes:
   ```cpp
   ForEachSignalPosition(rails, [&](uint8_t signalbit, SignalOffsets image, uint pos, Track track) {
   	if (IsSignalPresent(tile, signalbit)) DrawSingleSignal(tile, rti, track, GetSingleSignalState(tile, signalbit), image, pos, GetSignalType(tile, track), GetSignalVariant(tile, track), tile);
   });
   ```
3. **Verify that `IsSignalPresent(tile, signalbit)` tests bit `signalbit` of the same field** that `SignalAlongTrackdir` masks are applied to (`GetPresentSignals`). If it doesn't, stop.
4. **Add the ghost entry point** after `DrawSignals`:
   ```cpp
   /**
    * Draw one one-way signal for the junction placement ghost, without reading the tile's map data.
    * @param tile Tile to draw on.
    * @param rt Rail type whose signal sprites to use.
    * @param facing Trains travelling along this trackdir see the signal.
    * @param type Signal type.
    * @param variant Signal variant.
    */
   void DrawRailSignalGhost(TileIndex tile, RailType rt, Trackdir facing, SignalType type, SignalVariant variant)
   {
   	const RailTypeInfo *rti = GetRailTypeInfo(rt);
   	Track ghost_track = TrackdirToTrack(facing);
   	uint8_t mask = SignalAlongTrackdir(facing);
   	ForEachSignalPosition(TrackBits{ghost_track}, [&](uint8_t signalbit, SignalOffsets image, uint pos, Track track) {
   		if (track == ghost_track && HasBit(mask, signalbit)) DrawSingleSignal(tile, rti, track, SignalState::Green, image, pos, type, variant, INVALID_TILE);
   	});
   }
   ```
5. Declare it in `src/junction_ghost_draw.h`.

Message: `Codechange: Pass signal type and variant into signal drawing`

### Commit 4: bridge ghost drawing

1. **In `src/tunnelbridge_cmd.cpp`, remove the `static` keyword** from exactly these functions, changing nothing else about them: `GetBridgeSpriteTable`, `GetBridgeRampDirectionBaseOffset`, `GetBridgeMiddleAxisBaseOffset`, `DrawBridgePillars` and `CalcBridgePiece`. Declare them in `src/junction_ghost_draw.h` with their exact signatures, under a comment: "Bridge sprite helpers defined in tunnelbridge_cmd.cpp". Forward-declare `struct TileInfo;` and include the minimal type headers. Don't edit `bridge.h`.
2. **Create `src/junction_ghost_draw.cpp`** (licence header, `@file`, `stdafx.h` first, `safeguards.h` last) with:
   ```cpp
   void DrawBridgeHeadGhost(TileInfo *ti, BridgeType type, RailType rt, DiagDirection dir);
   void DrawBridgeMiddleGhost(const TileInfo *ti, BridgeType type, RailType rt, Axis axis, int bridge_z, uint piece_north, uint piece_south);
   ```
   Declare both in the header.
   - **`DrawBridgeHeadGhost`** follows the bridge head branch of `DrawTile_TunnelBridge` for a **flat** tile, line by line, with these differences:
     - It skips the foundation, the ground or shore or snow drawing, reservation, catenary, and the trailing `DrawBridgeMiddle` call.
     - The rail-type offset is `GetRailTypeInfo(rt)->bridge_offset` instead of `GetBridgeSpriteTableBaseOffset`.
     - It uses `INVALID_TILE` for `GetCustomRailSprite`.
     - It keeps the `is_custom_layout || rti->UsesOverlay()` rail deck block with `StartSpriteCombine`/`EndSpriteCombine`, exactly as the real code does.
   - **`DrawBridgeMiddleGhost`** follows `DrawBridgeMiddle` from the point after its map reads, with these differences:
     - `piece = CalcBridgePiece(piece_north, piece_south)`. The caller passes the same values the real code passes to `CalcBridgePiece`.
     - It passes `false` where the real code passes `IsTransparencySet(TransparencyOption::Bridges)`. The ghost palette override provides the transparency.
     - It skips reservation and catenary.
     - It always draws pillars (no blocking test), with `drawfarpillar` computed exactly as the real code computes it.
   - Each function gets a doc comment noting it mirrors the real code, with the function and file it mirrors, so future rebases know to keep them in sync.
3. Add `junction_ghost_draw.cpp` to `src/CMakeLists.txt` alphabetically.

Message: `Add: Ghost drawing for junction bridges`

## 5. Do not

- Reformat, reorder or "improve" any upstream code. Change only the lines these substitutions need.
- Change `rail.h` or `bridge.h`.
- Add the `DrawTileSelection` hook, or call any ghost function from anywhere. That's step 7.
- Add tests, or work outside the worktree.

## 6. Stop conditions

- A map read listed in commit 2 or 3 isn't where described, or helpers read the same data.
- `HasReservedTracks` or `IsSignalPresent` don't match the assumptions above.
- A function listed in commit 4 doesn't exist, or can't be made non-static without touching its body.
- Any build warning or error you can't fix within these rules, or any unit test failing other than the known-red outer test.
- Anything not covered here.

## 7. Gates

Logs go to `.../s6_*.log`; grep them.

1. After **each** commit, run `cmake --build build --target openttd openttd_test` with no errors and no new warnings.
2. After the last commit, the full suite: 100 test cases, 99 passing, with only the known outer test failing.
3. `git diff --stat {HEAD}..HEAD -- src/viewport.cpp src/rail_cmd.cpp src/tunnelbridge_cmd.cpp`: report the per-file line counts, so the upstream diff size is visible.
4. **Manual check for the engineer,** listed in your report and not done by you: load a game with plain rail, electric rail, a NewGRF rail type if available, signals of every type, reservations shown, snowy or desert track, half-tile foundations, and bridges. Everything must look exactly as before.

## 8. Report format

- **References read**
- **Commits:** hash and subject, four expected
- **Red evidence:** "none: drawing has no unit-test seam (named in brief)"
- **Types and members added or removed:** names only, including every function whose signature changed and every function made non-static
- **Upstream diff size:** per-file line counts from gate 3
- **Suite results:** the summary line
- **Manual checks:** the engineer's checklist, not done by you
- **Stops, and anything left undone**
- **State:** `git status --porcelain` and `git log -4 --format='%h %s'`, both run in the worktree

When you have reported, stop.
