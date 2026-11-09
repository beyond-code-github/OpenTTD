# C1: the compactness score (D153)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/c1`, branch `wt-c1`, head given in your launch prompt. You are the only writer there. Other agents work in sibling worktrees; never touch them or the main worktree `/home/pete/OpenTTD`. You add two new source files and one test file and register them; touch no existing source except the two CMakeLists.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`; `"[junction]~[slow]"` at the gate.
- Hardware note: a lone segfault/SIGILL under load is most likely the machine: rerun alone and report.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` row D153 and `RECON-organic.md` §7 (the measure and the numbers).

## Why (settled, D153)
The organic search needs a compactness term that counts wasted land. Measured on coop hubs: enclosed pockets are normal, slivers are not. The score counts surface-occupied tiles, sliver tiles and (reported but not yet weighted) enclosed tiles. The reference implementation is `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/compact/metric.py`: read it; the C++ must give the same numbers.

## Read first (once each)
- `CODINGSTYLE.md`.
- `metric.py` (above, ~50 lines).
- `src/junction_layout.h` (the `JunctionLayout`, `JunctionTrack`, `JunctionBridge`, `JunctionStationBlock` structs only).
- One small existing test file for style, e.g. `src/tests/junction_route.cpp` (head).

## Definitions (exactly as metric.py)
- Box: the layout's `size`, padded by one free tile on every side.
- Occupied: every tile with a `JunctionTrack`, both head tiles of every `JunctionBridge` (bridges and, later, tunnels alike), and every tile of every `JunctionStationBlock`. Tiles between bridge or tunnel heads are not occupied unless they hold track.
- Reachable: free tiles reached by a 4-neighbour flood fill from the padded corner (0, 0) through free tiles.
- `enclosed`: free tiles inside the layout's size that are not reachable.
- `slivers`: reachable free tiles inside the layout's size that lie in no 2x2 square of free tiles fully inside the layout's size.

## Tests (red first; record each red)
New file `src/tests/junction_compact.cpp`, tag `[junction][compact]`:
1. Two parallel straight X lines 10 tiles long with a 1-tile gap: occupied 20, slivers 10, enclosed 0.
2. The same with a 2-tile gap: slivers 0.
3. A closed square ring of track 6x6 (perimeter only): enclosed 16.
4. A line through a bridge (heads 4 apart) with a perpendicular line crossing under the span: the tiles under the span are not occupied unless they hold track; nothing enclosed.
5. Today's generator layout: `GenerateJunction(MakeTestJunctionSpec())` gives occupied 190, enclosed 34, slivers 53 (metric.py at b04736187e; RECON-organic §7). Tag this one `[slow]` if it takes over 2 s.
Red for each: before the function exists, link error; then with a stub returning zeros, the assertions fail (record).

## Changes
1. New `src/junction_compact.h`:
   ```cpp
   /** Compactness of a layout (D153): occupied surface tiles, sliver tiles and enclosed tiles, as RECON-organic §7 defines them. */
   struct JunctionCompactness {
   	int occupied = 0;
   	int slivers = 0;
   	int enclosed = 0;
   };

   /** Measure a layout's compactness (D153). */
   JunctionCompactness MeasureJunctionCompactness(const JunctionLayout &layout);
   ```
2. New `src/junction_compact.cpp` implementing it (a grid over the padded box, a queue flood fill, a 2x2 scan); the file header comment as in other junction files.
3. Register both files in `src/CMakeLists.txt` (beside `junction_layout.cpp`) and the test in `src/tests/CMakeLists.txt`.

## Do not
- Change `ScoreJunctionLayout`, the generator or any existing file other than the two CMakeLists.
- Weight or combine the three numbers into one score (the search design will).

## Gates
1. Build: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction]~[slow]" 8` and `"[compact]"`.
3. Commit: `Add: Junction layout compactness measure`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test
- Types, functions, members added: names only
- Suite results
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
