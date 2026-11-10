# LP10: a painted line to a layout with signals and a cost (RECON-linepainter LP10)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/lp10`, branch `wt-lp10`, head given at launch (feature/junction-tool). Create `build` configured like `/home/pete/OpenTTD/build` (Debug); build `openttd_test` only.
- Plan: `docs/junction-tool/RECON-linepainter.md` LP10; D97 (signal distance = drag signalling) in `docs/junction-tool/DECISIONS.md`.

## Test speed
New tests tagged `[linebuild]`, the tag under 5 s. Develop against it only; the wider gate once at the end.

## Read first (once each)
- `CODINGSTYLE.md`.
- `src/junction_line.h` (whole); `src/junction_line.cpp` only where `RasterizeJunctionLine` is defined.
- `src/junction_signal_pass.cpp` lines 150-210 (`FillSignals`, the D97 walk) and `PlaceSignal` (52-58).
- `src/junction_layout.h` (`JunctionLayout`, `EstimateJunction`, `JunctionPricing`).
- `src/junction_build.h` and `src/tests/junction_build.cpp` (the fake poster and how a layout becomes build steps).
- `ParseJunctionNotation` / `ParseLineNotation` docs and D142 in DECISIONS: which tracks travel in the route's direction and which against it.

## What to build (`src/junction_line.{h,cpp}`)
```cpp
/** A painted line ready to build (LP10): its layout with signals, and its cost. */
struct JunctionLineBuild {
	JunctionLayout layout;
	JunctionEstimate estimate;
	std::string error;
};
/**
 * Lay a line program as a layout, signal each track as drag signalling would (D97) in its direction of travel, and price it.
 * @param sd Signal distance (drag density, tiles).
 */
JunctionLineBuild BuildJunctionLine(const JunctionLineSpec &line, const JunctionLineRoute &route, uint8_t sd, SignalType signal_type, const JunctionPricing &pricing, TileIndex origin);
```
- Layout: `RasterizeJunctionLine`, then `AddRasterToLayout` per track (bridges and tunnels included). A raster error -> `error`, empty layout.
- Direction: each track's travel direction follows the notation convention you found (left group / right group). If the code does not define it, stop and report.
- Signals: for each track, walk its pieces in its travel direction and place signals with the D97 rule exactly as `FillSignals` counts (192 per axis piece, 128 per diagonal half piece, density sd x 192, minimise-gaps behaviour: an unsuitable piece moves the signal to the last suitable piece before it), starting with a signal on the track's first suitable piece. Unsuitable: bridge or tunnel pieces (heads and spans). Facing: along travel. To share the counting, extract the D97 walk from `FillSignals` into a function `PlaceDragSignals(JunctionLayout &layout, const std::vector<JunctionPiece> &pieces, uint8_t sd, SignalType type)` declared in `junction_signal_pass.h`, used by both `FillSignals` and your code; `FillSignals`' behaviour must not change (the signal pass and generator suites prove it).
- Cost: `EstimateJunction(layout, origin, pricing)`.

## Tests (`src/tests/junction_line_build.cpp`, new, registered; tag `[linebuild]`), red first (stub returning an empty build)
1. "a straight L_R line is signalled at SD 4": a 30-tile straight L_R line: both tracks carry signals 4 tiles apart (axis pieces), facing their own travel directions, first signal on the first piece.
2. "signals skip bridges": an L_R line with a B3 in the middle: no signal on any bridge piece; the signal due on the bridge moves to the last suitable piece before the near head.
3. "diagonal pieces count 128": a line with a 45-degree turn: the signal positions follow 192/128 counting (assert the logical distance between consecutive signals is <= sd x 192 and that the next piece would exceed it).
4. "the line builds through the fake poster": the layout's build steps through the existing sequencer and fake poster (as `tests/junction_build.cpp` does): all track, bridge and signal steps posted, tracks before signals on the same tile.
5. "the estimate counts every piece": `estimate` equals `EstimateJunction` on the layout and is > 0.
6. "the signal pass is unchanged": the existing signal pass tests still pass (run `[junction]` signal pass tests by name in the final gate).

## Do not
- No UI, no router changes, no terrain snapshot (LP9), no changes to `junction_generator.*` or `junction_organic.*`, no savegame/network/command changes.
- No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill` or bare `git stash`.

## Stop conditions
- Travel direction per track is not defined by the code or decisions.
- `FillSignals` cannot share the walk without a behaviour change.
- `[linebuild]` over 5 s.

## Gates
0 warnings; `[linebuild]` green under 5 s; once at the end `"[junction]~[slow]~[mapexport]"` green (report time). One commit on `wt-lp10`: `Feature: Junction painted line laid with drag signals and a cost`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; types and members added; suite results with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
