# G3.4a: the column of a bridge over an S-bend as a choice (DESIGN-G3 §G3.4a)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
The relations "bridge over S-bend" pin a single bridge to the S-bend's column t = 1. The b-6 measure showed LL_RR x LL_RR is infeasible with those pins and feasible without them. This step makes t a choice:
- of the fixed path, defaulting to today's t = 1;
- a variable of the joint model.

Identity does not change.

## State
- Worktree `<WORKTREE given at launch>`. First: `git -C <WORKTREE> checkout -B wt-g34a <HEAD given at launch>`. Rebuild `openttd_test` in `build` and `build-rel` first (configured with `-Dhighs_DIR=/home/pete/.local/highs/lib/cmake/highs`).
- Never touch the other worktree under `.claude/`.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g34a/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.4 and §G3.4a;
- `src/junction/placement/junction_placement.h`: `JunctionPlacementChoices` and the `JunctionSystem*` types;
- `src/junction/placement/junction_placement.cpp`:
  - the choices check near the top (grep "the choices do not fit");
  - the block "A single bridge passes over the S-bend it crosses";
  - `record_system` (grep "bend_id");
  - where the fixed path reads `fixed->m` (grep "fixed->m");
- `src/junction/placement/junction_joint.h/.cpp` (all);
- `src/junction/junction_track_geometry.h` and its P1/P2 tests in `src/tests/junction/junction_track_geometry.cpp`;
- `src/tests/junction/placement/junction_joint.cpp`: the helpers, "joint: choices round-trip", and `[.g3loop]`.

## Part A: premise P3 (test first)
Test **"S-bend columns are affine in the column"** in `src/tests/junction/junction_track_geometry.cpp`, tagged as the P1 test.
- For every heading 0..3, both hands, and L = 2..64: rasterise the S-bend of 2L halves, as the placement's `bend_tiles` does.
- Take side = `(heading + hand + 4) % 4`. Column t holds the tiles with `x VX[side] + y VY[side] == t`; lo(t) and hi(t) are their least and greatest y.
- Find, per (heading, hand):
  - the slope s, from columns 1 and 2 at L = 64;
  - T0 and T1, such that for every L, T(L) = T0 + T1 L is the largest t for which every column 1..t has contiguous rows and satisfies `lo(t) = lo(1) + s (t - 1)` and `hi(t) = hi(1) + s (t - 1)`.
- CHECK the form for every L, and INFO the table: heading, hand, s, T0, T1, lo(1), hi(1).
- If T(L) is not affine in L, or lo(1) or hi(1) change with L, STOP and report the table. Do not write Parts B to D.
- Add a function to `junction_track_geometry.h/.cpp` that the test and the placement share:
  ```cpp
  /** The rows of an S-bend's sideways column 1 and how they move per column (DESIGN-G3 §G3.4a): lo(t) = lo + s (t - 1), hi(t) = hi + s (t - 1) for t = 1..t0 + t1 L. */
  struct JunctionSBendColumns {
  	int lo, hi, s, t0, t1;
  };
  JunctionSBendColumns JunctionSBendColumnForm(int heading, int hand);
  ```
  It computes the form from the rasters, as the test does. The test checks it against every L.
- Red: write the test with the function returning zeros. It fails on the form check.

## Part B: the choice in the fixed path
- Add to `JunctionPlacementChoices`: `std::vector<std::vector<int>> over; ///< Per edge, per token: for a single bridge's Over token over an S-bend, its sideways column t - 1 (DESIGN-G3 §G3.4a); empty means 0 everywhere.`
- The choices check accepts `over` empty, or shaped like `m`.
- In the block "A single bridge passes over the S-bend it crosses", with t = 1 + the choice (0 when empty or when not fixed):
  - X: `eq(0, el.first, crossed->first, VX[side] * t)`.
  - Y: the heads at `lo + s (t - 1) - 1` and `hi + s (t - 1) + 1`, mirrored for south exactly as today. lo, hi and s come from `JunctionSBendColumnForm`. If t > t0 + t1 max(m, 2), fail with "a bridge over an S-bend beyond its diagonal".
  - The system tag stays linear: a = 0, b = the constant at the recorded t.
- With t = 1 every relation equals today's. Identity must not move.
- The recording records one new entry per such bridge. Add:
  ```cpp
  /** A single bridge over an S-bend whose sideways column t is a choice (DESIGN-G3 §G3.4a). */
  struct JunctionSystemOver {
  	int edge; ///< The bridge's edge.
  	int tok; ///< Its Over token.
  	int bend; ///< The crossed S-bend's id.
  	int t; ///< The column at the recording.
  	int t0, t1; ///< t <= t0 + t1 L(bend).
  };
  ```
  - Add `std::vector<JunctionSystemOver> overs;` to `JunctionPlacementSystem`.
  - Add `int over = -1; ///< When >= 0: c also has + d * (t(over) - t at the recording).` and `int d = 0;` to `JunctionSystemRelation`.
  - Tag the X relation with d = `VX[side]`, and the Y relations with d = s (or -s, as the mirror gives).
  - Carry this through a new `SystemTag` member `int over = -1, d = 0;`.

## Part C: the joint model
- Per `overs` entry:
  - one integer variable t with bounds [1, 64];
  - the row `t - t1 L(bend) <= t0`;
  - objective `+ EPS t`.
- A relation with `over >= 0` gets the term `- d t` on the side that `c` is on, and `+ d t_recorded` in its constant. Write the row once in the comment.
- Fill `result.choices.over` (t - 1 per entry) shaped like `m`, or leave it empty when `overs` is empty.

## Tests (`src/tests/junction/placement/junction_joint.cpp`), outer first
1. **"joint: a bridge over an S-bend may move along it"** (outer). LL_RR x LL_RR at all-floor choices (as `[.g3loop]` builds them):
   - the round-0 joint solve is Optimal (it is Infeasible today);
   - then place through the fixed path at its choices. WARN whether it places, and the error if not;
   - WARN the t of every `overs` entry.
   - Red: today it is Infeasible; record the conflict.
2. **"joint: choices round-trip"** (B1) must stay exactly as it is, with `over` all 0 on T and LL_RR x L_R. If the joint picks t != 1 there, STOP and report the bridge and both objectives.
3. **`[.g3loop]`:** run it and report its output. The round lines gain the sum of t - 1.

## Do not
- Touch the clash rows, curve gaps, hug relations, rounds, seed or pipeline, or make the loop live.
- Add tail-side columns (t beyond T(L)).
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- P3 fails.
- B1 moves.
- Identity or any existing test changes.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`;
- `[.g3loop]` once in Release.

No CLAUDE.md change (the joint model is not live, and the fixed path's default is today's rule). One commit on wt-g34a: `Add: Junction bridge over an S-bend may stand in any column of its diagonal`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- P3's table;
- red evidence;
- test 1's lines;
- the `[.g3loop]` output verbatim, at most 70 lines;
- members added;
- gate and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
