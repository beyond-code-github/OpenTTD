# G3 R2: build() emits only rows that need no solve; detect(x) adds the rest (DESIGN-G3 §G3.6 v2, D240)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Today `build()` runs its own flat solve in the X phase and adds the occupancy, hug and X-clearance families from that solve's order (the block "The occupancy family and the bridge heads follow the order of the flat solve"). When that inner solve fails, those families are silently missing. That is why the joint model's rulebook is incomplete on the failing hubs.

This step moves every family that needs positions out of `build()` into one lambda, `detect`, which the caller runs at a point it hands in. The fixed path hands it the same flat solve as today, so this is a pure refactor and identity does not change. R3 will hand it the joint loop's canonical point instead.

The families that move:
- the doubled bridge heads' hug of their crossers ("bridge heads hug crossers"), both its inequalities and the `hugs` it fills;
- `compute_node_tracks`, `add_occupancy`, `hug_heads` and `add_clearance_x`;
- `plan_clearance`'s side rule (the Y clearance), as `detect_y`.

Decided here (orchestrator, reversible): the hug inequalities of a straight crosser move with the rest of the hug, because which crosser tiles lie on a head row needs the rows. §G3.6 lists them as static; R3 may promote them.

## State
- Worktree `<WORKTREE given at launch>`. First: `git -C <WORKTREE> checkout -B wt-r2 <HEAD given at launch>`. Rebuild `openttd_test` in `build` and `build-rel` first (configured with HiGHS).
- Never touch the other worktrees under `.claude/`.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/r2/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.6 (lines 95-170);
- `docs/junction-tool/RECON-R2-families.md` (a map of `build()`'s families; its line numbers predate R1 by a few lines);
- `src/junction/placement/junction_placement.cpp`:
  - `edge_tiles` and `row_pieces`;
  - `flat_solve`;
  - `add_occupancy`, `hug_heads`, `collect_bridges`, `add_clearance_x`;
  - `build()` from its start to its first relation, the hug block (grep "bridge heads hug crossers"), the clearance Y rows (grep "bridge middle clearance"), and the occupancy block (grep "follow the order of the flat solve");
  - `plan_clearance`;
  - every call of `build()` (grep "build()");
- `src/tests/junction/placement/junction_joint.cpp`: the helpers, "joint: choices round-trip", "record: the measured sizes given back change nothing" (from R1), and `[.g3loop]`'s all-floor choices.

## Changes in `junction_placement.cpp`
1. **The hug moves.** In `build()`, the loop over `bridge.crossed` inside "bridge heads hug crossers" goes, with the `hugs.push_back` and `if (x_phase) hugs.clear();`.
   - Keep, in `build()`, everything around it that needs no rows: the `why`, `u`, and the `HugCandidate` heads.
   - In its place, push into a new local `std::vector<HugSite> hug_sites;` (cleared at `build()`'s start in the X phase) one entry per bridge, holding what the moved loop needs:
     ```cpp
     /** A doubled bridge whose heads hug their crossers, found by build() and filled by detect (DESIGN-G3 §G3.6, R2). */
     struct HugSite {
     	uint32_t e; ///< The row edge.
     	int r; ///< Its row.
     	size_t b; ///< The bridge's index in bridges_of[e].
     	int s; ///< The heading's sign along X.
     };
     ```
   - Declare it next to `HugCandidate`.
   - The old `if (!rows_known) break;` becomes: `build()` pushes the site only when `rows_known`.
2. **`detect`.** Add, after `build()`:
   ```cpp
   /* The families that read positions (DESIGN-G3 §G3.6, R2): at columns x and the rows vy, the hug of every doubled bridge's heads, the occupancy, the hug pins and the X clearance; false with build_why when a crosser has no tile on a head row. */
   auto detect = [&](const std::vector<int> &x) -> bool { ... };
   ```
   Its body, in this order:
   - `hugs.clear()`;
   - for each `hug_sites` entry, the moved crosser loop, unchanged: same `why`, same `tag` handling, same "Z row" `build_why` and `return false`, and the `hugs.push_back`;
   - `compute_node_tracks();`
   - `if (!add_occupancy(x, false)) return false;`
   - `hug_heads(x);`
   - `if (!skip_clearance) add_clearance_x(x);`
   - `return true;`
   
   Then delete the block "The occupancy family and the bridge heads follow the order of the flat solve" from `build()`.
   - The `why` of each family stays its own, so the reasons are unchanged.
   - `detect` adds rows to `cons` after `build()`'s rows, the floors and the box bound. If a test that pins an explanation chain changes text only because of that order, STOP and report it. Do not reorder to fix it.
3. **Every X-phase call of `build()`** where the old block could run (`rows_known && !relations_only`) becomes a call of a new lambda:
   ```cpp
   /* build() and, when the rows are known, detect at the flat solve's columns, as the old build() did (DESIGN-G3 §G3.6, R2). */
   auto build_x = [&]() -> bool {
   	if (!build()) return false;
   	if (!rows_known || relations_only) return true;
   	std::vector<int> flat;
   	if (!flat_solve(flat)) return true;
   	return detect(flat);
   };
   ```
   - Calls in the Y phase, and `plan_clearance`'s own calls, keep `build()`.
   - The first X pass of the rounds (rows not known) may call either; use `build_x()` for every X call outside `plan_clearance`.
   - If `detect` returns false, the call site treats it as a failed `build()` does today.
4. **`detect_y`.** Move `plan_clearance`'s loop "for (const BridgeLine &line : lines)" into:
   ```cpp
   /* The Y-phase clearance of the X-axis bridges at a point (DESIGN-G3 §G3.6, R2): a head whose column lies strictly within an X-axis bridge keeps its side of the bridge's row by one; on the row itself, both sides (as today). */
   auto detect_y = [&](const std::vector<int> &x, const std::vector<int> &y) { ... };
   ```
   - It clears `clear_y` and fills it exactly as today, calling `collect_bridges` itself.
   - `plan_clearance` keeps its two flat solves and calls `detect_y(flat_x, flat_y)` where the loop was.
   - `build()`'s rows from `clear_y` stay where they are.

Nothing else changes. That includes `edge_tiles`, `row_pieces`, `add_occupancy`'s body, the facts, the curve gaps, the rounds' order, the joint model, the hover and the pipeline.

## Tests (`src/tests/junction/placement/junction_joint.cpp`, `[junction][milp]`)
This is a refactor, so its tests are guards captured before the change. Write them first and run them on the unchanged code. Copy the printed values into the CHECKs. They must then hold after the change.

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
