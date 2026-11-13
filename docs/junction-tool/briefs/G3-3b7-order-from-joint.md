# G3.3b-7: occupancy order from the joint solution (DESIGN-G3 §G3.3a item 3 (b), D236)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
In the b-6 loop, L_R x L_RR and L_R x LL_RR record no clash at all. Their fixed path's flat X solve fails at the recorded choices, so `add_occupancy`, `hug_heads` and `add_clearance_x` never run, and the joint model never sees occupancy.

Lazy clash constraints (D236 (b)) read the pairs from the solution: solve, read the order the solution shows, add the pairs, solve again. So a recording may take its order from given positions (the previous joint solution) instead of its own flat solve. Only a recording does this; the fixed path, the rounds, the seed and identity do not change.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes2`. First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-g33b7 <HEAD given at launch>`. Rebuild `openttd_test` in `build` and `build-rel` first.
- Another agent works in `.claude/nodes`, on topology: never touch it.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g33b7/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `src/junction/placement/junction_placement.h`: `JunctionSystem*`, `RecordJunctionPlacementSystem`, `PlaceTrackGraph`;
- `src/junction/placement/junction_placement.cpp`:
  - the site "The occupancy family and the bridge heads follow the order of the flat solve" and the lines after it, to the end of that `if`;
  - `add_occupancy`'s signature and its `vy` reads;
  - `hug_heads`, `add_clearance_x` (signatures only);
  - `RecordJunctionPlacementSystem`;
- `src/tests/junction/placement/junction_joint.cpp`: test "joint: clash rows replace the held occupancy" and the `[.g3loop]` test.

## Part A: where the fixed path stops (characterisation first)
In the `[.g3loop]` test, for every round, WARN:
- the fixed path's error;
- whether that round's recording reached the occupancy site. Add the recording member `bool occupancy_read; ///< The recording reached the occupancy family with its rows known.` to `JunctionPlacementSystem` to report it.

Run it before Part B. If, in any round of L_R x L_RR or L_R x LL_RR, the recording never reaches the site's `if (x_phase && rows_known && !relations_only)`, STOP. That covers a failure in the Y phase or before the rows are known. Report the round, the stage and the error.

## Part B: the order from given positions
- Add to `junction_placement.h`:
  ```cpp
  /** Positions of the placement points that a recording reads the occupancy, hug and clearance order from, instead of its flat solve (DESIGN-G3 §G3.3a, b-7). */
  struct JunctionSystemOrder {
  	std::vector<int> x, y; ///< Per point, its column and row.
  };
  ```
- Add a trailing parameter `const JunctionSystemOrder *order = nullptr` to `RecordJunctionPlacementSystem` and to `PlaceTrackGraph`. It is used only when `system != nullptr`.
- `add_occupancy` takes the rows it groups by as a parameter: `add_occupancy(const std::vector<int> &flat, const std::vector<int> &rows, bool legality)`.
  - Every read of `vy` inside it becomes `rows`.
  - Today's callers pass `vy`.
- At the site: when `order != nullptr && system != nullptr` and `order->x.size() == pt_count`:
  - skip `flat_solve`;
  - call `add_occupancy(order->x, order->y, false)`, `hug_heads(order->x)` and, unless `skip_clearance`, `add_clearance_x(order->x)`;
  - otherwise the site is unchanged.
- The fixed path goes on as today. A recording records what it reached.

## Part C: the loop reads the order from the previous round
In `[.g3loop]`:
- round 0 records with no order;
- round k >= 1 records with `order = {previous joint x, previous joint y}`;
- clashes are kept as in b-6.
Add one WARN field: "order from joint" yes or no.

## Tests
1. **"joint: a recording with the seed's own order changes nothing"** (outer, `[junction][milp]`). On T and LL_RR x L_R at the seed's choices:
   - record without an order;
   - solve the joint model;
   - record again with `order` = the seed placement's own point positions, using `columns` for x and the recorded Y solve's rows for y. If the rows of every point cannot be read from the placement result without a new member, use the first joint result's x and y instead, and say which you used.
   - Check that the second joint result has the same status and choices, and an objective within 1e-6.
   - This test is a guard, not a red test: it checks that the new path reproduces the seed. Write it after Part B. It must pass; if it fails, STOP and report the difference.
2. **"joint: a recording with a given order reaches occupancy"** (`[junction][milp]`). L_R x L_RR at all-floor choices (as `[.g3loop]` builds them):
   - round 0 joint solve;
   - record with its x and y as the order;
   - check `occupancy_read` and `!clashes.empty()`.
   - Red: before Part B, `occupancy_read` is false (Part A shows it).

Tests of b-2..b-6 stay exactly as they are.

## Do not
- Change the fixed path's behaviour when `order` is null, the rounds, the seed, the pipeline, the clash form or the hug relations.
- Make the loop live.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Part A's stop.
- Identity or any existing test changes.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`;
- `[.g3loop]` once in Release, its output in the report.

No CLAUDE.md change. One commit on wt-g33b7: `Add: Junction joint loop reads occupancy order from the joint solution`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- Part A's lines;
- red evidence;
- the `[.g3loop]` output verbatim, at most 70 lines;
- members added;
- gate and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
