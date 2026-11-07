# Brief D78 step T2: the generator on terrain

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Worktree:** `/home/pete/OpenTTD-terrain`, branch `junction-terrain`, head `a06206c944` (T1: `JunctionTerrain` plus router support). Build dir `/home/pete/OpenTTD-terrain/build-terrain`. Logs go to `{LOG}`.
  - Another agent works in `/home/pete/OpenTTD`; don't touch it.
- **Plan:** `/home/pete/OpenTTD/docs/junction-tool/PLAN-D78.md`, design points 1, 5 and 6, step T2. Decision D93.

## 2. Read first

- `src/junction_terrain.h`
- `src/junction_router.h` (the `terrain` and `plane` fields)
- `src/junction_generator.h/.cpp`: `JunctionCandidateParams`, `BuildJunctionCandidate`, `GenerateJunction`, `ScoreJunctionLayout`
- `src/tests/junction_generator.cpp`

## 3. Changes (`junction_generator.h/.cpp`)

1. **Signatures:**

   ```cpp
   std::optional<JunctionLayout> BuildJunctionCandidate(const JunctionSpec &spec, const JunctionCandidateParams &params, const JunctionTerrain *terrain = nullptr);
   JunctionGenerateResult GenerateJunction(const JunctionSpec &spec, const JunctionTerrain *terrain = nullptr);
   ```

   Add `int plane = 0;` to `JunctionCandidateParams`.

2. **The terrain's frame.** Terrain coordinates are the generator's working coordinates before cropping and normalising (x 0..63, rows from 0). The anchor (D93 point 6) is the hub point (xI, 0) = (30, 0).
   - Callers build the terrain so that its cell (30 − origin.x, 0 − origin.y) is the hovered tile; that is step T3's business.
   - Document this on the `terrain` parameter.

3. **In `BuildJunctionCandidate`, when `terrain` is set:**
   - every router request gets `terrain` and `plane = params.plane`;
   - after the crop, and before `NormaliseJunctionLayout`, every remaining track tile must pass `terrain->UsableAt(x, y, plane)`, every bridge head must pass `UsableAt`, and every bridge middle tile must pass `SpanOk`. Otherwise return `std::nullopt`. This covers the pre-laid rows, stubs and lanes, which the router never checked.

4. **In `GenerateJunction`, when `terrain` is set:**
   - **Planes:** collect `z_max` over the terrain tiles within ±4 of the anchor (both axes), excluding non-Buildable tiles. Order the distinct values by count (descending, ties lower first) and keep at most 3. If there are none, return `NoLayout`.
   - **Sweep** each plane in that order. Within each, sweep exactly as today; the early stop applies per plane.
   - **Score:** `ScoreJunctionLayout`'s cost plus the terrain cost (the sum of `CostAt(x, y, plane)` over occupied track tiles and bridge heads, counted once per tile). Compute it before normalising, while the coordinates are still terrain coordinates. Keep the tie-break order.
   - Without terrain, behaviour is unchanged. The existing tests must stay green and identical.

## 4. Tests (`src/tests/junction_generator.cpp`, tag `[junction][terrain]`)

**Helper:** a flat Buildable terrain at z 0 covering x 0..63, y 0..40, with `foundation_cost` 250. Record red evidence against stubs that ignore terrain.

1. **Flat equals none:** `GenerateJunction(spec, &flat)` gives the same tracks, bridges and signals as `GenerateJunction(spec)`.
2. **House block:**
   - Mark Blocked every tile that the no-terrain layout's E→S branch uses below the W track. To find them, generate without terrain once, take the tiles holding track in rows ≥ g+3 west of xI, and mark them in the terrain before normalising. If the frame makes this awkward, mark a fixed rectangle x 18..27, y 4..10 instead, and say so.
   - The result is checker-clean and occupies no Blocked tile (check before normalisation: expose nothing; compare via a second `BuildJunctionCandidate` call if needed, or simply check that the layout differs from the flat one and is clean). If it gives `NoLayout`, report that instead of failing, and the test asserts only that no exception or crash occurs. I want to see what happens.
3. **Plane:** the whole terrain is flat at z 3. The result equals the flat-z0 result (same tracks), because the plane is 3 everywhere.
4. **Pond under a bridge:** Water tiles across the E→S/N→E crossing area of the W track (x 26..34, y g+1..g+2). The result is clean. Either the lanes bridge the water (allowed by `SpanOk`) or it routes around. Report which.

## 5. Do not

- change the router, the checker, the signal pass or the game side
- add map access

## 6. Stop conditions

- Test 1 differs. Report the first difference.
- Anything not covered.

## 7. Gates

1. `cmake --build /home/pete/OpenTTD-terrain/build-terrain --target openttd openttd_test > {LOG}/t2_build.log 2>&1`. There must be no errors or warnings in junction files.
2. The full suite in that build. Everything passes; the baseline there is 216 cases.

Commit message: `Add: Junction generator takes a terrain snapshot`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- red evidence
- functions changed
- the test 2 and test 4 outcomes, with ASCII
- suite totals
- `GenerateJunction` timing, with and without terrain
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
