# G3.3b-6: occupancy as kept clash rows in the joint model (DESIGN-G3 §G3.3a item 3, "The b-6 form"; D236)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Today the joint model holds every occupancy relation at a constant, read at the recorded sizes. The b-5 loop re-reads those constants every round, and that cycles. This step does two things:
- it writes each occupancy pair as a linear row in the points' columns and the bends' lengths (a clash);
- the loop keeps every clash it has seen.

The fixed path, the rounds and the seed do not change.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes2`. First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-g33b6 <HEAD given at launch>`. Build `openttd_test` in `build` and `build-rel` before the first run (configured with `-Dhighs_DIR=/home/pete/.local/highs/lib/cmake/highs`).
- Nobody else works in the worktrees during this step. Never touch `.claude/nodes`.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g33b6/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.3a (all of item 3, including "The b-6 form");
- `src/junction/placement/junction_joint.h/.cpp` (all);
- the `JunctionSystem*` types in `src/junction/placement/junction_placement.h`;
- `src/junction/placement/junction_placement.cpp`:
  - `bend_tiles` (grep "auto bend_tiles");
  - `RowPiece` and `row_pieces`;
  - `add_occupancy`, all of it (grep "auto add_occupancy");
  - `record_system` (grep "auto record_system", or "bend_id");
- `src/junction/junction_track_geometry.h` (whole) and `src/tests/junction/junction_track_geometry.cpp` (the P1 test, "bend rasters are affine in their length");
- `src/tests/junction/placement/junction_joint.cpp`: the helpers, test "joint: choices round-trip", and test "joint: the joint loop on the hubs".

## Part A: tile forms (premise P2, test first)
Add to `src/junction/junction_track_geometry.h/.cpp`:
```cpp
/** One tile of a bend's raster as an affine form of the bend's length L: a head tile's offset from the bend's first tile is constant; a tail tile's is (dx0 + dx1 L, dy0 + dy1 L), counted by its index from the raster's end. */
struct JunctionBendTileForm {
	bool tail;
	int dx0, dx1, dy0, dy1;
};

/** Per tile of the raster of a bend of length L (an S-bend: 2L halves; a Corner: 2L + 1), its form, read from the rasters at L and L + 1. */
std::vector<JunctionBendTileForm> JunctionBendTileForms(int heading, int hand, int length, bool corner);
```
- Rasterise as the placement's `bend_tiles` does: `RasterizeJunctionRoute({{0, 0}, JUNCTION_AXIS_TRACKDIR[heading], JunctionBendMoves(hand, halves, corner)})`, with offsets relative to piece 1.
- Tile j at L is a head tile when its offset at L + 1, same index j, is equal. Then dx1 = dy1 = 0, and (dx0, dy0) is the offset.
- Otherwise, with i = n(L) - 1 - j, take d = offset at L + 1 of index n(L + 1) - 1 - i, minus offset at L of index j. Then dx1, dy1 = d and dx0, dy0 = offset - L d.
- **Test P2, "bend tile forms predict the raster"** (in `src/tests/junction/junction_track_geometry.cpp`, tags as the P1 test). For corner false and true, every heading 0..3, both hands and L = 1..63:
  - each tile's form, evaluated at every L' = L + 1..64, equals the raster's tile at L';
  - for a head tile that is index j, and for a tail tile index n(L') - 1 - i.
- Red: write the test with the function returning an empty vector; it fails on the size. If it fails after the function is written, STOP and report the first (corner, heading, hand, L, L', j) that fails. Pete decides then.

## Part B: clashes recorded (outer test first)
Add to `junction_placement.h`:
```cpp
/** A column of the joint model: x[pt] + c0 + c1 * L(bend); bend is -1 when c1 is 0. */
struct JunctionSystemColumn {
	int pt;
	int c0 = 0;
	int c1 = 0;
	int bend = -1;
	auto operator<=>(const JunctionSystemColumn &) const = default;
};

/** An occupancy pair of the last X round as a clash row: b's left column minus a's right column at least gap, while both stand on one row (DESIGN-G3 §G3.3a, b-6). */
struct JunctionSystemClash {
	std::string key; ///< The pair's identity across recordings at other sizes: both occupants' edge, copy, node and columns, and tie.
	std::string reversed; ///< The key of the same pair with a and b swapped.
	JunctionSystemColumn a_right;
	JunctionSystemColumn b_left;
	int gap; ///< As the recorded relation: 1, 2 when the two touch, or the recorded apart of a tie.
	bool held; ///< A column's c1 multiplies a bend the joint model gives no length variable; the model uses the recorded size.
	std::string reason; ///< The occupancy reason text, as the relation's.
};
```
- Add `std::vector<JunctionSystemClash> clashes; ///< The occupancy pairs of the last X round whose rows were known, as clash rows.` to `JunctionPlacementSystem`.
- In `add_occupancy`, when `system != nullptr`, give each `Occupant` its two parametric ends. Add `JunctionSystemColumn left, right;` to `Occupant`, filled at each site:
  - a tile with `dx` exact: `{pt, dx}`;
  - a run: `{a, 1}` and `{b, -1}` (the order follows `sgn`, as `add_run` does);
  - a node's tile: `{pt}`;
  - a prio footprint tile: `{prio_el.last, -step * sgn}`;
  - a bend's tile j: `{pt, f.dx0, f.dx1, bend}`, with f = `JunctionBendTileForms(heading, hand, L, corner)[j]` at the bend's L.
    - L is `halves / 2` for an S-bend and `(halves - 1) / 2` for a Corner.
    - The bend is the `Elem *` mapped through the same `bend_id` map that `record_system` builds for `SystemTag::bend`. Carry the `Elem *` to `add_bend` as one new parameter.
    - A row piece's Bend gets one new `RowPiece` member: `const Elem *bend = nullptr; ///< Bend: its chain element.`
    - If a bend tile's chain element cannot be named, STOP.
  - When `add_tile` merges a tile into the previous occupant, the columns stay the first one's (they are the same tile).
- In `add_relation` (the one site that emits occupancy pairs), when recording, also push a `JunctionSystemClash` built from a's `right` and b's `left`:
  - the gap: the relation's constant evaluated before the tile offsets, so for the inequality `touch ? 2 : 1`, and for a tie `apart`;
  - for a tie, push the inequality only (the relation stays an equality in the fixed path);
  - key and reversed as described above.
- `record_system` copies them like `routes`: from the X phase, only when the rows are known.

## Part C: the joint model reads clashes
- In `SolveJunctionJoint`, when `system.clashes` is not empty:
  - skip every relation whose reason starts with "occupancy";
  - add one row per clash: `x[b.pt] + b.c1 L(b) - x[a.pt] - a.c1 L(a) >= gap - b.c0 + a.c0`.
- A column whose bend has no length variable uses the recorded `m` as a constant, and the clash is `held`.
- A column whose bend has a zero branch gets `- BIG (1 - z)` on the right side, once per such bend, in the same way as `AddCurveGapCounters` does.
- Clash rows carry the clash's reason, so an infeasible set names them.

## Tests (`src/tests/junction/placement/junction_joint.cpp`), outer first
1. **"joint: clash rows replace the held occupancy"** (outer). On T and LL_RR x L_R at the seed's choices:
   - the joint result with `system.clashes` gives the same status, the same `choices` and an objective within 1e-6 of the same system with `clashes` cleared;
   - `clashes` is not empty;
   - every clash, evaluated at the recorded columns and sizes, holds.
   - Red: before Part B, `clashes` is empty.
2. **"joint: recordings at other sizes keep their points"** (inner). Record T at the seed's choices and at those choices with one free S-bend's `m` + 1:
   - equal `pt_count`;
   - equal bends (edge, copy, tok, first, last).
   - If this fails, STOP: keys of point ids then mean nothing across rounds.
3. **`[.g3loop]` with kept clashes.** Change the loop test so each round keeps the clashes:
   - Each round: for each clash of the new recording, add it to a `std::map<std::string, JunctionSystemClash>` unless its key or its reversed key is already there. Then set the recorded system's `clashes` to the map's values in key order before the joint solve.
   - A repeat of choices counts as a cycle only when no clash was added since that round.
   - The round's WARN line gains: new clashes kept, kept total, held count.
   - For LL_RR x LL_RR only, after the loop, also solve its round-0 system once with every relation whose reason starts with "bridge over S-bend" removed. WARN the status, and the conflict if it is infeasible. This is a measure for the next step.

Tests 1 and 2 of b-3, the b-2 prototype test, A1 and B1 must stay exactly as they are.

## Do not
- Change the fixed path, the rounds, the seed or the pipeline, or make the loop live.
- Touch the hug relations, curve gaps or the bridge-over-S-bend relations in the model.
- Add a row-equality constraint or any binary for which row a pair stands on.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- P2 fails.
- Test 2 fails.
- Test 1 differs (status, choices or objective).
- Identity or any existing test changes.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`;
- `[.g3loop]` run once in Release, its output in the report.

No CLAUDE.md change (the joint model is not live). One commit on wt-g33b6: `Add: Junction joint model keeps occupancy as clash rows`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence for P2 and test 1;
- test 1's clash counts per hub, and held counts;
- the `[.g3loop]` output verbatim, at most 70 lines;
- members and functions added;
- gate and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
