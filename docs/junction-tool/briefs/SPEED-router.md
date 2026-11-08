# SPEED-router: flat structures inside the router search

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD`, branch `feature/junction-tool`, head given in your launch prompt.
- Follows SPEED-pass (822a3e17ff) and SPEED-cache (d203ea21d4). After SPEED-cache, S1 ("GenerateJunction gives a rule-clean L_R T") takes a median of 1.37 s in `build-rel`.
- Scratch dir: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/speed/` (`golden.before`, `lib.before`; SPEED-cache's report describes how the temporary `[.libdump]` test writes the library in that format).

## Why
The engineer chose this step: "Go ahead and try the flat arrays approach first". The pre-cache profile put self-time in the router's `std::unordered_map` (`best`), malloc/memset (libc 11%; `best.reserve(max_expansions)` allocates about 50,000 buckets on every call, even for searches that expand a few hundred nodes), and the heap.

## Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (required; the previous agent skipped it).
- `src/junction_router.cpp` lines 1-180 and 370-435.

## Tests
This is output-identical. The existing router tests and the golden/library identity are the tests; no new test case. Before changing code, record the S1 timing (median of 3) and confirm the golden and library dumps still `cmp` identical to `golden.before`/`lib.before` on the current head.

## Changes (all in `src/junction_router.cpp`)
1. **`best` becomes a flat open-addressing table.** Add a file-local class in the anonymous namespace:
   ```cpp
   /** The cheapest g found per packed search state: linear probing, power-of-two capacity, grows at half full. */
   class BestCostTable {
   public:
   	explicit BestCostTable(size_t capacity);
   	/** Insert key with g, or lower its g. @return Whether the state is new or improved (false when the stored g is <= g). */
   	bool Improve(uint64_t key, Money g);
   	/** The stored g; the key must be present. */
   	Money Get(uint64_t key) const;
   private:
   	static constexpr uint64_t EMPTY = UINT64_MAX; ///< PackKey uses at most 50 bits, so never a real key.
   	std::vector<uint64_t> keys;
   	std::vector<Money> values;
   	size_t count = 0;
   	size_t mask = 0;
   	size_t Slot(uint64_t key) const;
   	void Grow();
   };
   ```
   - The hash in `Slot` is a 64-bit multiply mix: `(key * 0x9E3779B97F4A7C15ull) >> shift` or `^ (x >> 32)` then mask. Choose one, and document it in one line.
   - The initial capacity is 4096. Grow (double and reinsert) when `count * 2 >= capacity`.
   - In `Run`, replace the `best` map and its `reserve` with `BestCostTable best(4096);`. The `push` lambda's `try_emplace` logic becomes `if (!best.Improve(PackKey(state), g)) return;`. This keeps the semantics: a new key is inserted, and an existing key is replaced only when strictly cheaper. The pop-time check becomes `if (best.Get(PackKey(s)) < node_g) continue;`.
2. **Construct the fallback maps and sets only for tiles outside the grid.** In the `Router` constructor, insert into `tracks`, `heads`, `crossable` and `goal_tiles` only when `GridIndex(...)` is `-1` for that tile; every lookup already uses the grid when `gi >= 0` and the map/set only when `gi < 0`. Keep `goals` (the trackdir set, used by `IsGoal`) filled for all goals, unchanged. Before making the change, confirm by reading every use that no lookup reads a map or set for an in-grid tile. If one does, stop and report it.
3. **Reserve the search vectors.** In `Run`, `tree.reserve(1024)`, and build the `open` priority queue on a `std::vector<QueueItem>` reserved to 1024. Keep the `QueueItem` tuple and `std::greater` ordering exactly as they are. The order is a total order (the counter is unique), so pop order and output stay identical.

## Do not
- Change `PackKey`, the queue's ordering, `Successors`, `MakePiece`, the heuristic, or any rule or cost.
- Change the router's public interface, `JunctionRouteCache`, or anything outside `src/junction_router.cpp`.
- Use thread_local or static storage.

## Stop conditions
- A map/set lookup reads in-grid tiles (change 2).
- The golden or library dump differs.
- S1 gets slower.

## Gates
1. Build `build-rel` (`openttd_test`) and Debug `build` (`openttd_test` and `openttd`), with output to files. No errors and no new warnings.
2. Run the router tests (`./openttd_test "[router]"`, or the router test file's cases by name if there is no tag): pass.
3. Golden and library dumps: `cmp` identical to `golden.before` and `lib.before`. Remove the temporary `[.libdump]` test before committing.
4. Timing: S1 in `build-rel`, median of 3, before and after each of changes 1, 2 and 3 (cumulative), in a small table.
5. Run the full `build-rel/openttd_test` binary directly, with output to a file. Report failures.
6. Commit: `Codechange: Junction router uses flat search structures`, with the body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
- References read
- Commit: hash and subject
- Before/after timing table
- Types and members added or removed: names only
- Golden/library cmp results
- Suite results
- Stops, and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
