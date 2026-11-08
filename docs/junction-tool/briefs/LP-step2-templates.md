# Brief LP step 2: multi-track line geometry (corner templates), with no search

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **You run in your own git worktree,** based on `feature/junction-tool` at `{HEAD}`.
  - Build Release `openttd_test` only.
  - Commit on your branch.
  - Don't run any `tools/ottd` command.
- **Read:**
  - `docs/junction-tool/RECON-linepainter.md`, all of it, especially §C, option B and the corner problem;
  - D112;
  - `src/junction_route.h/.cpp`: the move grammar F, L, R, B<n>, `RasterizeJunctionRoute` and `JunctionPiece`;
  - `src/junction_generator.cpp`: `ParseJunctionNotation`.
- **Goal of this step:** a line is N parallel tracks at fixed spacing, as given by a notation gap (L_R means 2 tracks with a gap of 1 tile between them). This step defines how a **whole line** moves for one move (F, L or R) so that every track stays parallel at the right spacing, including through 45° turns. It's proven by rasterizing to tracks and checking spacing. No search yet.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/lp2/`

## 2. Changes

**New files `src/junction_line.h/.cpp`**, registered in `src/CMakeLists.txt`:

```cpp
/** A multi-track line: N parallel tracks, `gap` tiles apart between neighbours (0 means adjacent). */
struct JunctionLineSpec {
	uint8_t tracks = 2;
	uint8_t gap = 1;
};

/** A line program: a start (the centreline's first piece) and moves, as JunctionRoute, applied to the whole line. */
struct JunctionLineRoute {
	TileIndexDiffC start;
	Trackdir start_trackdir;
	std::string moves; ///< F, L, R only (no bridges in this step).
};

/** Rasterize a line program into one raster per track, outermost-left first; error text in the first raster when the line can't make a move. */
std::vector<JunctionRaster> RasterizeJunctionLine(const JunctionLineSpec &line, const JunctionLineRoute &route);
```

**Semantics (choose and document the exact construction, then test it):**
- The tracks are offset perpendicular to the start heading, at distance `(gap + 1)` tiles between neighbours along the perpendicular axis, for axis headings. Think through the equivalent for diagonal headings and document it.
- **A turn of the whole line:** each track turns at a point chosen so that after the turn the tracks are again parallel at the same perpendicular spacing. In general the inner and outer tracks make their 45° turns at different positions along the line (the corner problem in RECON-linepainter §C): an outer track has a longer diagonal or straight run than an inner one.
- **Derive the per-track move strings for one whole-line turn,** for 2-track lines with gap 0, 1 and 2, as a small table or formula. Prove it by rasterizing each track's derived `JunctionRoute` with `RasterizeJunctionRoute`.
- **If a clean equal-spacing construction doesn't exist for some heading or gap combination, stop.** Report which, with an ASCII example.

## 3. Tests (`src/tests/junction_line.cpp`, tag `[junction][line]`); red first

1. **Straight:** `"FFFF"` gives 2 rasters whose tiles are exactly (gap+1) apart perpendicular, all axis pieces.
2. **One left turn, for gap 0, 1 and 2:**
   - after the turn and a few F moves, the two tracks are parallel (same trackdir at the end) and at the correct perpendicular spacing for the new heading;
   - no tile is used by both tracks (except gap 0 diagonal pairs that share a tile as Upper/Lower or Left/Right, which is legal; check with the `TracksOverlap` rules);
   - each track's own raster has no error.
3. **The same for a right turn,** and for an S-bend (L then R).
4. **ASCII:** WARN each case's ASCII (`RenderJunctionAscii` of a layout built from the rasters) so the engineer can eyeball the shapes. Report them.

## 4. Do not

- add search, terrain, bridges or GUI
- touch the junction generator or router

## 5. Stop conditions

- No equal-spacing construction exists for some case. Report the case with ASCII.
- Anything not covered.

## 6. Gates

1. Release `openttd_test` build in the worktree, then the full suite, all passing.
2. Commit: `Add: Multi-track line geometry for the line painter`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report

- the branch and commit
- red evidence
- the construction, as a table or formula
- the ASCII per case
- suite totals
- stops
- `git status --porcelain` and `git log -1`, both in the worktree

When you have reported, stop.
