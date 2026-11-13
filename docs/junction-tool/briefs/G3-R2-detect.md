# G3 R2: build() emits only rows that need no solve; detect(x) adds the rest (DESIGN-G3 §G3.6 v2, D240)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Today `build()` runs its own flat solve in the X phase and adds the occupancy, hug-pin and X-clearance families from that solve's order (the block "The occupancy family and the bridge heads follow the order of the flat solve"). When that inner solve fails, those families are silently missing. That is why the joint model's rulebook is incomplete on the failing hubs.

This step moves the families that read columns out of `build()` into one lambda, `detect(x)`, which the caller runs at a point it hands in. `build()` still calls it at the old place with the same flat solve, so this is a pure refactor and identity does not change. R3 will hand it the joint loop's canonical point instead.

The families that move: `compute_node_tracks`, `add_occupancy`, `hug_heads` and `add_clearance_x`; and `plan_clearance`'s side rule (the Y clearance), as `detect_y`.

The hug of a doubled bridge's heads around its crossers ("bridge heads hug crossers", its inequalities and the `hugs` it fills) stays in `build()` unchanged: it reads only the rows, which the X phase has, and the flat solve must see it.

## Amendment 2 (2026-11-14, after the second attempt stopped)
Two attempts stopped, and both stops were brief defects:
- Attempt 1 (`scratchpad/r2/r2-attempt.patch`) solved the flat point after `build()` returned, over the floors, facts and box too.
- Attempt 2 (`scratchpad/r2/r2-v2.patch`) solved over the right prefix, but added `detect`'s rows after the facts block. The facts block prunes each point by the relations already in `cons`, so "placement on real terrain" (temperate-hilly-12) changed outcome.

So R2 now only extracts the column-reading families into `detect(x)`, and `build()` still calls it at exactly the old place, at the same flat point. There is no `build_x`, no `detect_rows`, and no prefix parameter on `flat_solve`. Where `detect`'s rows go relative to the facts when the point comes from outside is R3's question, not this step's.

The guard test and `SystemDigest` are uncommitted in the worktree and were captured on the unchanged code. Keep their bodies and digests exactly as they are; only their tags change (see Tests). Use `r2-v2.patch` as the base for `detect_y` and `detect`'s body.

## State
- Worktree `<WORKTREE given at launch>`, which holds the uncommitted guard test. First: `git -C <WORKTREE> checkout -B wt-r2 <HEAD given at launch>`. The guard test file is not changed between the old and new head, so the uncommitted change carries over. If the checkout refuses, STOP.
- Rebuild `openttd_test` in `build` and `build-rel` (delete stale `.pch` files if the build complains about them).
- Never touch the other worktrees under `.claude/`.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/r2/`; name them `*-v3.*`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `scratchpad/r2/r2-v2.patch`;
- `src/junction/placement/junction_placement.cpp`: the old block (grep "follow the order of the flat solve"), `plan_clearance`, and whatever the patch touches.

## Changes in `junction_placement.cpp`
1. **`detect`.** Add, just before `build()`:
   ```cpp
   /* The families that read columns (DESIGN-G3 §G3.6, R2): at columns x, the occupancy, the hug pins and the X clearance; false when the occupancy refuses. */
   auto detect = [&](const std::vector<int> &x) -> bool { ... };
   ```
   Its body: `if (!add_occupancy(x, false)) return false;`, `hug_heads(x);`, `if (!skip_clearance) add_clearance_x(x);`, `return true;`.
2. **The old block in `build()`** becomes exactly:
   ```cpp
   if (x_phase && rows_known && !relations_only) {
   	/* The column-reading families (detect) follow the order of the flat solve of the relations above (D217); when the relations contradict each other the solve below explains it. R3 hands detect another point. */
   	compute_node_tracks();
   	std::vector<int> flat;
   	if (flat_solve(flat) && !detect(flat)) return false;
   }
   ```
3. **`detect_y`**, as in `r2-v2.patch`: it holds `plan_clearance`'s loop "for (const BridgeLine &line : lines)", with the comment from the earlier brief text. It clears and fills `clear_y`, calling `collect_bridges` itself. `plan_clearance` calls `detect_y(flat_x, flat_y)` where the loop was. Locals that the move leaves unused go.

There is no `HugSite`, no `build_x`, no `detect_rows`, and no change to `flat_solve`. The hug loop stays in `build()` unchanged. Nothing else changes.

## Tests (`src/tests/junction/placement/junction_joint.cpp`)
This is a refactor, so its tests are guards captured before the change. They exist already (uncommitted, from the first attempt, captured on the unchanged code); they must hold after the change. The spec below is what they are. One change to the guard test: its tags become `[junction][milp][.sweep]`. It takes about 50 s in Debug and about 10 s in Release, so it runs in `jt sweep` (the hidden slow sweeps, Release) and not in `jt fast` or the wide gate.

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
- The guard test in the worktree does not match the spec in Tests.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt sweep build-rel` (6 cases now, with the guard test; budget 90 s);
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
