# N2: per-tile facts with O(1) run and next-bad queries (D183, DESIGN-nodes §2 heights, §9; FABLE-nodes-perf §1.2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
In the node model, surface track height is not a choice: it follows the tile's slope and the track on it (D152, D180), so every terrain fact is known before the solve. The solve needs those facts as O(1) queries: is this tile usable for a straight piece along an axis and at which entry/exit heights, can a switch or a half (corner) piece sit here, does a straight run from a to b on one row connect at matching heights, where is the next bad tile. This step builds that table from a JunctionTerrain snapshot. Nothing uses it yet.

## State
Worktree `/home/pete/OpenTTD/.claude/n2`, branch `wt-n2`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache); build `openttd_test` only. Tests only through `tools/junction/jt` with bracketed tags: `'[facts]'` (new; budget 1 s Release, 5 s Debug); `jt gate build-rel` once before the commit. Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n2/`. Another agent works in `.claude/n0b` on `junction_chain.*`: do not touch those files.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `src/junction_terrain.h` (whole, short: JunctionTileClass, JunctionTerrainTile, JunctionTerrain::At/UsableAt/CostAt/SpanOk); in `src/junction_line_router.cpp` lines ~195-262: HeightStep, RampStep, PieceUsable (the rules this table must match); `src/junction_positions.h` (style reference); `src/tests/CMakeLists.txt` and the junction_positions lines in `src/CMakeLists.txt`.

## The rules (exact, from HeightStep/RampStep/UsableAt)
For a tile (x, y) and an axis piece along axis X travelling +x (exit SW in canonical terms; use TrackdirToExitdir of Trackdir::X_SW) or along axis Y travelling +y (Trackdir::Y_SE):
- not usable if the tile is outside the grid, its class is not Buildable, or it is steep.
- if the tile's `incline` is the exit direction: a ramp up, entry z_min, exit z_min + 1.
- if the tile's `incline` is the reverse of the exit direction: a ramp down, entry z_max, exit z_max - 1.
- otherwise usable iff UsableAt(x, y, z_max) (flat, or a foundation with build_on_slopes): entry = exit = z_max.
A half (corner) piece and a switch tile (two tracks): usable iff UsableAt(x, y, z_max), height z_max (no ramps).
Two neighbouring straight pieces along the same axis connect iff the first's exit height equals the second's entry height.

## Types (new `src/junction_facts.h` / `.cpp`, registered next to junction_positions in `src/CMakeLists.txt`)
```cpp
/** Per-tile facts of a terrain snapshot for the node model, with O(1) queries along rows and columns (DESIGN-nodes §2, FABLE-nodes-perf §1.2). Coordinates are the snapshot's canonical ones. */
class JunctionFacts {
public:
	explicit JunctionFacts(const JunctionTerrain &terrain);
	/** Whether an axis piece along the axis (travelling +x for Axis::X, +y for Axis::Y) can sit on the tile; entry/exit receive its heights. */
	bool Straight(int x, int y, Axis axis, int8_t &entry, int8_t &exit) const;
	bool Switch(int x, int y) const; ///< A switch tile (two tracks): UsableAt at z_max.
	bool Half(int x, int y) const; ///< A half (corner) piece: UsableAt at z_max.
	/** Whether straight axis pieces cover every tile from a to b (inclusive, either order) on row y (Axis::X) or column x (Axis::Y) and connect at matching heights. O(1). */
	bool StraightRun(Axis axis, int line, int a, int b) const;
	/** The first position >= from on the line where the fact fails (Straight for kind Straight on that axis, Switch, Half), or INT_MAX if none. O(1). */
	int NextBad(JunctionFactKind kind, Axis axis, int line, int from) const;
	/** The last position <= from where it fails, or INT_MIN. O(1). */
	int PrevBad(JunctionFactKind kind, Axis axis, int line, int from) const;
};
enum class JunctionFactKind : uint8_t { Straight, Switch, Half }; // declared before the class
```
Internals (private, named so nothing else is invented): the snapshot's origin, width, height; per tile and axis `int8_t entry, exit` and a `bool straight`; per tile `bool sw, half`; per row (Axis::X) and per column (Axis::Y) arrays `run_id` (a new id starts at every tile that is not straight or does not connect to its predecessor), and `next_bad`/`prev_bad` per kind. Plain `std::vector`s indexed by grid cell. Queries outside the grid behave as "bad".

## Tests, red first (new `src/tests/junction_facts.cpp`, tags `[junction][facts]`)
1. "facts: a flat tile, a ramp and a foundation": a 4x1 terrain built by hand: flat z 1; inclined rising along +x (incline = SW-equivalent exit direction, z_min 1, z_max 2); flat z 2; a slope inclined across the axis (z_min 1, z_max 2, incline the Y direction). Asserts Straight X heights (1,1), (1,2), (2,2), (2,2); StraightRun(X, 0, 0, 3) true; making tile 2 flat z 3 -> false. Red: stub returns false; record.
2. "facts: switches and halves are flat or foundation only": the ramp tile is Straight along X but not Switch and not Half.
3. "facts: blocked and steep tiles are bad": NextBad/PrevBad on a row with a Blocked tile at x 5 and a steep one at x 9.
4. "facts: queries agree with walking": `std::minstd_rand` seeded 1; 200 random terrains 16x8: per tile random class (Buildable 80 %), steep 5 %, z_min in 0..3, z_max in z_min..z_min+1, incline random among the four DiagDirections or Invalid when z_min == z_max (Invalid also 50 % of sloped tiles), build_on_slopes random. For random queries (1000 per terrain): StraightRun, NextBad, PrevBad compared with a naive walk over the tiles using Straight/Switch/Half. Red: stub fails.
5. "facts: a 128x128 snapshot builds fast": build on a random 128x128 terrain; `WARN` the microseconds (std::chrono); no assertion on time.

## Do not
No change to the terrain, the router, generation, junction_chain or junction_positions. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
A rule above contradicts HeightStep/RampStep/UsableAt as you read them (report both); OVER BUDGET; anything not covered.

## Gates
0 warnings; `jt fast build '[facts]' 5`, `jt fast build-rel '[facts]' 1`, `jt gate build-rel`. Add a `[facts]` row to TESTS.md. One commit on `wt-n2`: `Feature: Junction per-tile facts with O(1) row queries (D183)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence per test; types and members added; test 5's microseconds; jt lines with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
