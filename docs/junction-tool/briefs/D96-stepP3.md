# Brief D96 step P3: router inner loop (same routes, faster)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Plan:** `docs/junction-tool/PLAN-D96.md`, step P3. P2 (236119c3fd) moved the router onto hash containers.
- **The remaining hot spots** (from the P1 profile):
  - tile lookups (`TileFree`, heads, crossable, goal tiles): about 13%;
  - the heap with `OverflowSafeInt` tuples: about 9%;
  - a `std::string` per tree node: about 5%.
- **This is a pure refactor.** Every route must come out identical, checked with the `[.golden]` test.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/p3/`
- **Don't run any `tools/ottd` command:** a rig run is live.

## 2. Read first

- `src/junction_router.cpp`, all of it.
- `src/tests/junction_generator.cpp`: the `[.golden]` test (env `JUNCTION_GOLDEN`).

## 3. Before any change

1. `[.golden]` → `p3/before.txt`, from `build-rel`.
2. Time S1 and S3 in `build-prof` (RelWithDebInfo, already configured). Take the median of 3 for `build-prof/openttd_test "GenerateJunction gives a rule-clean L_R T"` and `"GenerateJunction avoids a block of houses"`. Rebuild `build-prof` first.

## 4. Changes (all in `junction_router.cpp`)

1. **Dense tile grid.**
   - In the `Router` constructor, define a rectangle `grid_min = request.min − (12, 12)`, `grid_max = request.max + (12, 12)`. Twelve covers a bridge of 10 middle tiles plus its heads.
   - Add `std::vector<TrackBits> track_grid` and `std::vector<uint8_t> flag_grid`, both sized to that rectangle, row-major.
   - Flag bits: 1 = head, 2 = crossable, 4 = goal tile.
   - Fill both from the same sources as `tracks`, `heads`, `crossable` and `goal_tiles`.
   - Lookups inside the rectangle use the grids. Lookups outside it fall back to the existing hash containers, which stay and keep being filled. Add one private helper, `int GridIndex(int x, int y) const`, returning −1 outside the rectangle.
   - `goals` (with trackdir) stays a hash set.
2. **Move text without heap strings.**
   - Add `struct MoveText { std::array<char, 4> c{}; uint8_t n = 0; };`.
   - Use it in place of `std::string` in `TreeNode::moves`, in `Successors`' output pairs, and in `push`. Moves are "F", "L", "R", "B<n>" with n ≤ 10, or two-move pairs, so at most 3 chars.
   - Build the route's move string only when a goal is reached, by appending each `MoveText` in the same order as today.
   - `fmt::format("B{}", n)` becomes writing 'B' and the digits into `MoveText`.
   - Add `assert` that n ≤ 3 when writing.
3. **Plain integers in the queue.**
   - `QueueItem` becomes `std::tuple<int64_t, int64_t, uint64_t, int>`, using `f.base()` and `g.base()`, or whatever accessor `OverflowSafeInt` offers for the raw value; check `core/overflowsafe_type.hpp`.
   - Comparison order and tie-breaks are unchanged.
4. Nothing else changes.

## 5. Gates

1. `[.golden]` → `p3/after.txt`, then `cmp`. They must be identical.
2. Release suite, all passing.
3. `cmake --build build --target openttd openttd_test > p3/dbg.log 2>&1`: no errors or warnings in `junction_router.cpp`.
4. S1 and S3 timings after the change, in `build-prof`.

Commit message: `Codechange: Faster junction router inner loop`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Do not

- change costs, ordering, the heuristic or tie-breaks
- remove the hash containers (they back lookups outside the grid)
- touch any other file

## 7. Stop conditions

- The golden files differ.
- Anything not covered.

## 8. Report

- commit
- cmp result
- suite totals
- S1 and S3 timings, before and after
- `git status --porcelain` and `git log -1`

When you have reported, stop.
