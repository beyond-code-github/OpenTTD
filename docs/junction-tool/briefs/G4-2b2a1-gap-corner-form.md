# G4 2b2a1: the gap table and the Corner column form (DESIGN-G4 §3, §8 2b2a; D268)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
The static model will separate unrelated pieces on a row by a gap that depends only on their tracks (the gap table), and will read a band edge's column at an item row from an affine form of its bend, Corners included. This step adds both as geometry helpers with their tests. The occupancy family's three touch computations then call the gap helper, so today's rule and the table are one function. No layout, objective, hash or digest moves.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-2b2a1 <HEAD given at launch>`. Leave its untracked files alone. `build` (Debug) and `build-rel` (Release) exist; delete stale `.pch` files if the build complains.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2b2a1/`.
- Another agent changes one floor in the doubled-leg block of `junction_placement.cpp` (~1640-1650) in a different worktree. Do not touch that block.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part);
- `docs/junction-tool/DESIGN-G4-static.md` lines 108-127 (O1, and the gap table at the end);
- `src/junction/junction_track_geometry.h` (whole, short) and `src/junction/junction_track_geometry.cpp` lines 1-150 (`BendRasterOffsets`, `JunctionBendTileForms`, `JunctionSBendColumnForm`, `JunctionTrackTouches`, `JunctionTracksMayShareTile`);
- `src/tests/junction/junction_track_geometry.cpp` lines 1-40 and 140-201 (the share test and the S-bend column form test, your templates);
- `src/junction/placement/junction_placement.cpp` lines 1440-1470 (the three `touch` computations).

## Tests (red first: add each declaration with a stub body, build, see each test fail on an assertion, record the message; then write the code)
In `src/tests/junction/junction_track_geometry.cpp`, with the file's existing tags:
1. **"the gap table agrees with JunctionTracksMayShareTile on every kind pair"**: for every pair (low, high) of non-empty TrackBits made of the six tracks Upper, Lower, Left, Right, X, Y (63 x 63 pairs):
   - `JunctionUnrelatedTileGap(low, high).share == JunctionTracksMayShareTile(low, high)`;
   - `.apart` is 2 when some track of low touches the SW edge and some track of high touches the NE edge, by `JunctionTrackTouches` per single track; else 1.
   Stub: return `{false, 1}`.
2. **"a Corner's column at a row is affine"**: the S-bend column form test (lines ~140-201) done for Corners. Use the Corner raster (`JunctionBendMoves(hand, 2 * length + 1, true)`, 2L + 1 halves, so `2 * length + 3` raster pieces). The same per-column lo/hi/rows maps, s from columns 1 and 2 at L = 64, the reach T(L) per length, t1 = T(64) - T(63) and t0 = T(64) - 64 t1. Then require for every L in 2..64 that `t0 + t1 L` equals T(L), and that `JunctionCornerColumnForm(heading, hand)` returns lo1, hi1, s, t0, t1. Loop over the same headings and hands as the S-bend test. If the raster for some heading and hand has no columns 1 and 2, do what the S-bend test does for that case.
   Stub: return `{0, 0, 0, 0, 0}`.

## Changes
1. `src/junction/junction_track_geometry.h`, after `JunctionTracksMayShareTile`'s declaration:
   ```cpp
   /** The gap table of unrelated pieces on one row (DESIGN-G4 §3): share, whether they may stand on one tile; apart, the least column difference when they do not (2 when they would touch across the tile edge between them, else 1). */
   struct JunctionTileGap {
   	bool share;
   	int apart;
   };

   /**
    * The gap between two unrelated pieces on one row.
    * @param low The tracks of the piece on the lower column.
    * @param high The tracks of the piece on the higher column.
    * @return Whether they may share a tile, and their least column difference when they do not.
    */
   JunctionTileGap JunctionUnrelatedTileGap(TrackBits low, TrackBits high);
   ```
   and after `JunctionSBendColumnForm`'s declaration:
   ```cpp
   /** The column form of the Corner of 2L + 1 halves starting on the given heading with the given hand, read from the rasters, as JunctionSBendColumnForm's. */
   JunctionSBendColumns JunctionCornerColumnForm(int heading, int hand);
   ```
2. `src/junction/junction_track_geometry.cpp`:
   - `JunctionUnrelatedTileGap`: `share = JunctionTracksMayShareTile(low, high)`; `apart = (low.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::SW))) && high.Any(DiagdirReachesTracks(ReverseDiagDir(DiagDirection::NE)))) ? 2 : 1` (the expression the occupancy family uses today).
   - Move the body of `JunctionSBendColumnForm` into a file-static `static JunctionSBendColumns BendColumnForm(int heading, int hand, bool corner)` that passes `corner` to `BendRasterOffsets`. `JunctionSBendColumnForm` returns `BendColumnForm(heading, hand, false)`, `JunctionCornerColumnForm` returns `BendColumnForm(heading, hand, true)`. The function's comments move with it.
3. `src/junction/placement/junction_placement.cpp` ~1451, 1452 and 1463: each `touch` / `touch_reversed` becomes `JunctionUnrelatedTileGap(a.bits, b.bits).apart == 2` (or `(b.bits, a.bits)` for `touch_reversed`), keeping the `!tie &&` where it is today. Nothing else in that file changes.
4. `src/junction/CLAUDE.md`, the root-files line for `junction_track_geometry`: no change. `src/junction/placement/CLAUDE.md`, after the `JunctionTracksMayShareTile` line: "The least column gap between two unrelated pieces on one row is 2 when they would touch across the tile edge between them, else 1 (`JunctionUnrelatedTileGap`)."

## Do not
- Use either helper anywhere else, or add a row family, a recording member or a joint change.
- Touch the doubled-leg block (~1640-1650), any worktree but `ng1`, or `.claude/nodes2` (reserved for the engineer's game).
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- Test 2 finds `t0 + t1 L` differing from T(L) for some L, heading and hand: report the case, do not adjust the form.
- Any identity value moves (step 3 must change no behaviour), or any existing test fails.

## Gates (D264)
- `tools/junction/jt gate build-rel`, `tools/junction/jt sweep build-rel` and `tools/junction/jt identity build-rel`, each with a Bash timeout of at least 400000 ms: all pass; report cases and times.

One commit on wt-2b2a1: `Add: Junction gap table and Corner column form`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read; the commit; red evidence per test;
- types and functions added;
- the Corner forms found (heading, hand → lo, hi, s, t0, t1);
- gate, sweep, identity: cases and time; stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
