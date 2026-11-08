# Brief D96 step P2: router data structures (same routes, faster)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The suite has 235 cases; 234 run by default and 1 is hidden as `[.slow]`. All pass.
- **Why:** D96 (`docs/junction-tool/DECISIONS.md`). The P1 profile (RelWithDebInfo, test spec, 1.12 s) found:
  - the router is 91% of `GenerateJunction`;
  - inside it, `std::map<NodeKey, Money> best` (a 9-field tuple key) is about 52% of the run, and tuple comparison alone about 30%;
  - `TileFree` (a `std::map` lookup) is 6.6%;
  - `std::set::contains` (heads, crossable, goals) is about 6%;
  - the per-pop `TreeNode` copy (including its `std::string`) is about 1.5%.
- **This step is a pure refactor.** Every route, and so every generated layout, must come out identical. Tie-breaks depend on push order and the `counter` field, not on container iteration order, so swapping lookup containers keeps behaviour as long as nothing iterates them.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/p2/` (create it).

## 2. Read first

- `src/junction_router.cpp`, all of it (344 lines).
- `CODINGSTYLE.md`: the naming and comment style only.

## 3. Golden capture (do this before any code change)

Add one hidden test to `src/tests/junction_generator.cpp`:

```cpp
TEST_CASE("Junction generator golden JSON", "[.golden]")
```

- It writes `JunctionLayoutToJson` of the `GenerateJunction(spec, terrain)` layout for 3 scenarios, one line each, to the file named by env `JUNCTION_GOLDEN` (skip the test when that is unset):
  1. `MakeTestJunctionSpec()` with no terrain;
  2. the same with `branch_gap = 0` and `tl = 6`;
  3. `MakeFlatTerrain()` with `SetTerrainRect(t, 18, 27, 4, 10, JunctionTileClass::Blocked)`, as in the test "GenerateJunction avoids a block of houses". Both helpers already exist in that file. Place the golden test after them.
- When a scenario gives no layout, write the line `none` instead of the JSON.
- Build `build-rel` and run it with `JUNCTION_GOLDEN=p2/before.txt`. **Commit nothing yet.**

## 4. Changes, all in `src/junction_router.cpp`

1. **Packed key.**
   - Replace `NodeKey` and `MakeKey` with `uint64_t PackKey(const RouterNode &n)`. The fields go low to high:
     - x + 32768 (16 bits);
     - y + 32768 (16 bits);
     - trackdir (4 bits);
     - u (8 bits);
     - hand (2 bits);
     - prev_gap_ok, pair, first and on_bridge (1 bit each).
   - Add `assert(n.u < 256)` and `assert(n.x >= -32768 && n.x < 32768)`, and the same assert for y.
2. **`best` becomes `std::unordered_map<uint64_t, Money>`,** with `best.reserve(request.max_expansions)`.
   - The push lambda does a single lookup: use `try_emplace`, and if the entry existed with a value ≤ g, return; otherwise assign g.
   - The pop check uses `best.find(...)->second` instead of `operator[]`. The key always exists there, so `assert` that it does.
3. **Tile sets.**
   - Add `static uint32_t PackTile(int x, int y)`, with x + 32768 in the low 16 bits and y + 32768 in the high 16 bits.
   - `tracks` becomes `std::unordered_map<uint32_t, TrackBits>`.
   - `heads`, `crossable` and `goal_tiles` become `std::unordered_set<uint32_t>`.
   - `goals` becomes `std::unordered_set<uint64_t>`, keyed by `PackTile(x, y) | (uint64_t(trackdir) << 32)`.
   - Update every use. `SelfCrossing`'s local `own` map stays as it is (it runs once per found route).
4. **No copy on pop.**
   - Replace `const TreeNode node = tree[index];` with `const RouterNode s = tree[index].state; const Money node_g = tree[index].g;`.
   - Use `s` and `node_g` below it. `tree` may reallocate during `push`, so don't keep a reference.
   - Drop the later `const RouterNode &s = node.state;`.
5. Nothing else changes: costs, successors, the heuristic, the queue item and the move strings stay as they are.

## 5. Tests and gates

1. **Golden:** rebuild `build-rel`, run `[.golden]` with `JUNCTION_GOLDEN=p2/after.txt`, then `cmp p2/before.txt p2/after.txt`. They must be identical. Report the cmp result.
2. **Full suite,** Release: `build-rel/openttd_test`, all passing.
3. **Debug build** of `openttd` and `openttd_test` in `build/`: no errors or warnings in `junction_router.cpp`. Don't run the Debug suite.
4. **Timing,** before and after: run S1 and S3 from P1 in `build-prof` (RelWithDebInfo, already configured) as `build-prof/openttd_test "GenerateJunction gives a rule-clean L_R T"` and `"GenerateJunction avoids a block of houses"`. Take the median of 3 for each.
   - The "before" timing must come from a `build-prof` build of the head **before** your changes. Take it right after the golden capture, before you edit the router.
5. **Commit:**
   - commit the golden test and the router change together, as `Codechange: Faster junction router lookups`;
   - signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`;
   - don't commit the `p2/` files.

## 6. Do not

- change any cost, ordering, heuristic or tie-break
- change the generator, signal pass or checker
- replace the priority queue or the move strings (a later step may)
- iterate any of the new unordered containers. If some code iterates one of the old containers, stop and report.

## 7. Stop conditions

- The golden files differ.
- Any container you are told to replace is iterated somewhere.
- Anything not covered.

## 8. Report (short)

- commit
- cmp result
- suite totals
- S1 and S3 timings, before and after
- `git status --porcelain` and `git log -1`

When you have reported, stop.
