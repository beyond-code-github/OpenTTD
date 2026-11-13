# G3 R2: build() emits only rows that need no solve; detect(x) adds the rest (DESIGN-G3 §G3.6 v2, D240)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Today `build()` runs its own flat solve in the X phase and adds the occupancy, hug-pin and X-clearance families from that solve's order (the block "The occupancy family and the bridge heads follow the order of the flat solve"). When that inner solve fails, those families are silently missing. That is why the joint model's rulebook is incomplete on the failing hubs.

This step moves the families that read columns out of `build()` into one lambda, `detect(x)`, which the caller runs at a point it hands in. The fixed path hands it the same flat solve as today: over exactly the rows `build()` had emitted when the old block ran, so this is a pure refactor and identity does not change. R3 will hand it the joint loop's canonical point instead.

The families that move: `compute_node_tracks`, `add_occupancy`, `hug_heads` and `add_clearance_x`; and `plan_clearance`'s side rule (the Y clearance), as `detect_y`.

The hug of a doubled bridge's heads around its crossers ("bridge heads hug crossers", its inequalities and the `hugs` it fills) stays in `build()` unchanged: it reads only the rows, which the X phase has, and the flat solve must see it.

## Amendment (2026-11-14, after the first attempt stopped)
The first attempt (patch: `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/r2/r2-attempt.patch`) solved the flat point after `build()` returned, so the solve also saw the x floors, the facts and the box bound, and the T seed failed. The brief was wrong; this version fixes it by solving over a prefix of `cons`. The guard test and `SystemDigest` from that attempt are uncommitted in the worktree and were captured on the unchanged code: keep them as they are (read them; do not re-capture) and use them as the guard.

## State
- Worktree `<WORKTREE given at launch>`, branch `wt-r2` already checked out at `<HEAD given at launch>`, with the guard test uncommitted in `src/tests/junction/placement/junction_joint.cpp`. Do not reset or check out over it.
- `build/` was rebuilt after deleting stale .pch files; `build-rel/` was not: rebuild `openttd_test` there before its gates (delete its stale .pch files the same way if the build complains about them).
- First, before any change to `junction_placement.cpp`: `tools/junction/jt fast build '[milp]' 10` passes with the guard test.
- Never touch the other worktrees under `.claude/`.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/r2/` (name new files `*-v2.*`).

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.6 (lines 95-170);
- `src/junction/placement/junction_placement.cpp`:
  - `flat_solve` (grep "auto flat_solve");
  - `add_occupancy`, `hug_heads`, `collect_bridges`, `add_clearance_x`;
  - `build()` (grep "auto build = ") to the box bound, in particular the occupancy block (grep "follow the order of the flat solve");
  - `plan_clearance`;
  - every call of `build()` (grep "build()");
- the first attempt's patch, for the call sites it already found;
- `src/tests/junction/placement/junction_joint.cpp`: the uncommitted guard test and `SystemDigest`.

## Changes in `junction_placement.cpp`
1. **`flat_solve` takes a prefix.** `flat_solve(std::vector<int> &val, size_t count = SIZE_MAX)` uses only the first `std::min(count, cons.size())` rows of `cons`, in its bound as well as its differences. Every existing call passes nothing and is unchanged.
2. **`build()` marks the point.** A new local `size_t detect_rows = SIZE_MAX;` (next to `hugs`). `build()` sets it to `SIZE_MAX` at its start, and, where the old block stood, when `x_phase && rows_known && !relations_only`, sets `detect_rows = cons.size();`.
3. **`detect`.** Add, after `build()`:
   ```cpp
   /* The families that read columns (DESIGN-G3 §G3.6, R2): at columns x, the occupancy, the hug pins and the X clearance; false when the occupancy refuses. */
   auto detect = [&](const std::vector<int> &x) -> bool { ... };
   ```
   Its body: `compute_node_tracks();`, `if (!add_occupancy(x, false)) return false;`, `hug_heads(x);`, `if (!skip_clearance) add_clearance_x(x);`, `return true;`. Then delete the old block from `build()` (the `if (x_phase && rows_known && !relations_only) { ... }` around "follow the order of the flat solve"), leaving only the `detect_rows` mark in its place.
   - The `why` of each family stays its own.
   - `detect` adds rows to `cons` after `build()`'s rows, the floors and the box bound. If a test that pins an explanation chain changes text only because of that order, STOP and report it. Do not reorder to fix it.
4. **`build_x`.** Every X-phase call of `build()` outside `plan_clearance` becomes a call of:
   ```cpp
   /* build() and, when the rows are known, detect at the flat solve of the rows build() had made at the old point, as the old build() did (DESIGN-G3 §G3.6, R2). */
   auto build_x = [&]() -> bool {
   	if (!build()) return false;
   	if (detect_rows == SIZE_MAX) return true;
   	std::vector<int> flat;
   	if (!flat_solve(flat, detect_rows)) return true;
   	return detect(flat);
   };
   ```
   If `build_x` returns false, the call site treats it as a failed `build()` does today. Calls in the Y phase, and `plan_clearance`'s own calls, keep `build()`.
5. **`detect_y`.** Move `plan_clearance`'s loop "for (const BridgeLine &line : lines)" into:
   ```cpp
   /* The Y-phase clearance of the X-axis bridges at a point (DESIGN-G3 §G3.6, R2): a head whose column lies strictly within an X-axis bridge keeps its side of the bridge's row by one; on the row itself, both sides (as today). */
   auto detect_y = [&](const std::vector<int> &x, const std::vector<int> &y) { ... };
   ```
   - It clears `clear_y` and fills it exactly as today, calling `collect_bridges` itself.
   - `plan_clearance` keeps its two flat solves and calls `detect_y(flat_x, flat_y)` where the loop was.
   - `build()`'s rows from `clear_y` stay where they are.

No `HugSite`. Nothing else changes. That includes `edge_tiles`, `row_pieces`, the hug loop, `add_occupancy`'s body, the facts, the curve gaps, the rounds' order, the joint model, the hover and the pipeline.

## Tests (`src/tests/junction/placement/junction_joint.cpp`, `[junction][milp]`)
This is a refactor, so its tests are guards captured before the change. They exist already (uncommitted, from the first attempt, captured on the unchanged code); they must hold after the change. The spec below is what they are.

- **Helper `SystemDigest(const JunctionPlacementSystem &)`** returns `std::pair<size_t, uint64_t>`:
  - the number of relations plus clashes;
  - FNV-1a 64 over the relations' strings `fmt::format("{} {} {} {} {} {} {} {} {} {} {}", axis, v, u, c, bend, a, b, nonlinear, zero_branch, order_pin, reason)` and the clashes' `fmt::format("{} {} {}", key, gap, held)`, each list sorted first.
1. **"rule source: detection at the flat solve is today's rulebook"**:
   - T and LL_RR x L_R, at the seed's choices and at each sized bend's m + 1 (R1's `JunctionBendSizes`, one bend at a time: the bends of source "rows" and "port_rows" with m >= 1, at most 6 per hub, in id order);
   - L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR, at `[.g3loop]`'s all-floor choices.
   - Per case, CHECK the digest against the value printed before the change. Keep the cases in one table in the test.
Every existing test stays exactly as it is.

## Do not
- Change any relation's value, tag or reason, or the order of the rows `build()` itself emits.
- Change the record API, the joint model or `[.g3loop]`.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Identity, an existing test, or a test 1 digest changes after the change.
- `detect` would need a value that is neither `x`, `vy`, nor state `build()` left.
- The guard test in the worktree does not match the spec below, or does not pass on the unchanged code.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`. Other agents build in parallel. If a gate is over budget, re-run it once and report both times.

No CLAUDE.md change (a refactor). One commit on wt-r2: `Codechange: Junction placement detects position families at a given point`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- the digests (case, count, hash) before and after;
- members and lambdas added;
- gate and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
