# SPEED-cache: memoise router searches within one generation

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `4fa6cb800f`.
- This step follows SPEED-pass (`briefs/SPEED-pass.md`, commit 822a3e17ff). Background: `docs/junction-tool/PLAN-sim-driven.md` §1.
- Scratch dir for logs: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/speed/` (golden.before, lib.before and helper scripts from SPEED-pass are there).

## Why (measured by the orchestrator)
One `GenerateJunction` on the test spec (S1 "GenerateJunction gives a rule-clean L_R T") makes **37,600** `FindJunctionRoute` calls, but only **2,311** distinct (layout, request) inputs. 94% are repeats, including 10,176 of the 10,504 failed searches. The router is about 84% of generation time. `FindJunctionRoute` is a pure function of its inputs, so memoising it is output-identical.

## Read first (once each, with offset/limit)
- `CODINGSTYLE.md`
- `src/junction_router.h` (whole, 52 lines) and `src/junction_router.cpp` lines 1-60 and 370-445.
- `src/junction_generator.h` lines 70-100.
- `src/junction_generator.cpp` lines 135-170 (`RouteAndAdd`, `RouteFencedAndAdd`), 205-215, 580-620 (`BuildJunctionCandidate`), 685-800 (`GenerateJunction` and the library build).
- `src/tests/junction_router.cpp`: the first existing TEST_CASE, to copy its layout/request setup.

## Tests (write first, see red)
In `src/tests/junction_router.cpp`, add:

1. `TEST_CASE("JunctionRouteCache returns the same route and reuses it")`
   - Use an existing test's layout and request that finds a route.
   - `JunctionRouteCache cache;` then `a = FindJunctionRoute(layout, request, &cache)` and `b = FindJunctionRoute(layout, request, &cache)`.
   - Assert `a == FindJunctionRoute(layout, request)` (no cache) and `b == a`. If `JunctionRoute` has no `operator==`, compare its members field by field in the test; do not add an operator.
   - Assert `cache.Hits() == 1` and `cache.Misses() == 1`.
   - Then change one input (add one track tile to a copy of the layout, on a tile the route does not use) and call again: assert `cache.Misses() == 2`.
2. `TEST_CASE("JunctionRouteCache stores failed searches")`
   - A request with no route (copy an existing failing-route test if there is one; otherwise a goal outside `min`/`max`).
   - Two calls with the cache: both `std::nullopt`, `Hits() == 1`, `Misses() == 1`.

**Red step:** first add the class with `Lookup` always missing and `Store` doing nothing (so the code compiles). Run both tests and record the failing `Hits() == 1` assertion. Then implement.

## Changes
### `src/junction_router.h`
Add, before `FindJunctionRoute`:

```cpp
/**
 * Remembers router results within one generation, keyed by every input the router reads.
 * Only valid while bridge_span_ok, prices and the terrain stay the same: scope one cache to one GenerateJunction call or one library build.
 * Safe to share between threads.
 */
class JunctionRouteCache {
public:
	bool Lookup(const std::vector<int32_t> &key, std::optional<JunctionRoute> &result);
	void Store(std::vector<int32_t> key, const std::optional<JunctionRoute> &result);
	uint64_t Hits() const { return this->hits; }
	uint64_t Misses() const { return this->misses; }
private:
	std::mutex mutex;
	std::map<std::vector<int32_t>, std::optional<JunctionRoute>> results;
	std::atomic<uint64_t> hits{0};
	std::atomic<uint64_t> misses{0};
};
```

Change the declaration to `std::optional<JunctionRoute> FindJunctionRoute(const JunctionLayout &layout, const JunctionRouteRequest &request, JunctionRouteCache *cache = nullptr);` and add `@param cache` to its doc comment ("When set, results are looked up and stored here."). Add the includes `<atomic>`, `<map>`, `<mutex>`.

### `src/junction_router.cpp`
- A file-local `static std::vector<int32_t> RouteCacheKey(const JunctionLayout &layout, const JunctionRouteRequest &request)` that appends, in this order:
  - starts count, then each start's x, y, trackdir;
  - goals count, then each goal's x, y, trackdir;
  - min.x, min.y, max.x, max.y, curve_spacing, s_bend_window, max_bridge_middle, max_expansions, plane;
  - crossable count, then each crossable tile's x, y;
  - `crossable_cost` and `turn_cost`, each as two int32 halves of `.base()`;
  - layout tracks count, then each track's offset x, y and `tracks.base()`;
  - layout bridges count, then each bridge's start x, y and end x, y.
  - Do not include `bridge_span_ok`, `prices` or `terrain`: they are fixed per cache scope (see the class comment). If the router reads any other field of `JunctionLayout` or `JunctionRouteRequest` than those listed, stop and report it.
- `Lookup`: lock, find; on a hit, copy the result, increment `hits` and return true; else increment `misses` and return false.
- `Store`: lock, `try_emplace`.
- `FindJunctionRoute`: when `cache` is set, build the key, return the cached result on a hit, otherwise run the router and `Store` the result (found or `std::nullopt`) before returning.

### `src/junction_generator.h`
Add a member to `JunctionCandidateParams`: `JunctionRouteCache *route_cache = nullptr; ///< When set, router results are shared through it (one cache per generation).` Add a forward declaration `class JunctionRouteCache;` above the struct.

### `src/junction_generator.cpp`
- `RouteAndAdd` and `RouteFencedAndAdd` gain a trailing parameter `JunctionRouteCache *cache`, passed to `FindJunctionRoute` (and from `RouteFencedAndAdd` to `RouteAndAdd`).
- Every call to them inside `BuildJunctionCandidateWithGap` passes `params.route_cache`.
- In `GenerateJunction`, declare one `JunctionRouteCache route_cache;` before the plane loop and set `params.route_cache = &route_cache;` where each `params` is built.
- In the library build (around line 793), declare one cache per library build in the same way.
- Any other caller of `RouteAndAdd`/`RouteFencedAndAdd` passes `nullptr`.

Worked call site:
```cpp
const std::optional<JunctionRaster> e_to_s = RouteFencedAndAdd(l, request, column_fence, params.route_cache);
```

## Do not
- Change router internals (hash maps, heap, successors), the sweep ranges or order, the scoring, or the rules.
- Add a cache anywhere else (candidates, rules, signal pass).
- Add a global or thread_local cache.

## Stop conditions
- The router reads an input not in the key list.
- The golden or library dump differs from before.
- Any change to game state or savegames would be needed.

## Gates
1. Build `build-rel` and `build` (Debug) targets `openttd_test` (and `openttd` for Debug), with output to files. No errors and no new warnings.
2. The two new tests pass.
3. Output identity: re-run the SPEED-pass golden check and library dump exactly as you did for 822a3e17ff (`[.golden]`, plus a temporary `[.libdump]` test you remove before committing). `cmp` against `golden.before` and `lib.before`: identical.
4. Timing: S1 in `build-rel`, median of 3 wall times, before and after. Also record `Hits()`/`Misses()` from one S1 run, via a temporary print you remove before committing.
5. The full `openttd_test` binary (Catch2 directly, not ctest), output to a file. Report failures; the 4 `regression_*` ctest entries are known-environmental and not in scope.
6. Commit: `Codechange: Junction generation memoises router searches` with the body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Leave unrelated files alone.

## Report
- References read
- Commits: hash and subject
- Red evidence: each new test and its failure message before the implementation
- Types and members added or removed: names only
- Timing table (before/after, median of 3) and hits/misses
- Golden/library cmp results
- Suite results, naming any failure you did not cause
- Stops, and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
