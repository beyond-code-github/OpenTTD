# BRIEF SX1: probe H11 (a structure's crossers parallel) behind a flag (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-shape, head {HEAD}. Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel (persistent; do not reconfigure).
- Implements: wip/SPEC-slots.md §0.1 finding (a) and §5 Q1 (option A): measure whether Shape still shapes every variant, and at what turning cost, when a structure's crossers must be parallel. A probe: an environment flag, off by default, so nothing changes unless it is set.

## Read first (once, with offset/limit)
- /home/pete/OpenTTD/docs/junction-tool/wip/SPEC-slots.md §0.1 and §5 Q1.
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/planar_shape_layer.cpp lines 596-760 (`ShapePlanarStructureVariant`: the H variables, the `eq` and `var` helpers, the crossing rows, the capacity rows).
- /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_shape_layer.cpp: the last TEST_CASE, "planar sweep: Structure and Shape over T1-T3, LL2 and LL3a" (commit 15f22f629e).
- CODINGSTYLE.md (C++ part).

## Changes
1. In `ShapePlanarStructureVariant` (src/junction/planar/planar_shape_layer.cpp), directly after the capacity-rows loop, add:
   ```cpp
   /* Probe (SPEC-slots Q1, PLANAR_H11): a structure's crossers are parallel at their crossings (headings equal mod 4). */
   if (std::getenv("PLANAR_H11") != nullptr) {
   	for (size_t i = 0; i < sv.structure.structures.size(); i++) {
   		const PlanarStructure &s = sv.structure.structures[i];
   		for (size_t k = 1; k < s.crossings.size(); k++) {
   			const uint32_t h0 = H[s.crossers[0]][EventIndex(map.events[s.crossers[0]], s.crossings[0]) + 1];
   			const uint32_t hk = H[s.crossers[k]][EventIndex(map.events[s.crossers[k]], s.crossings[k]) + 1];
   			eq({{hk, 1}, {h0, -1}, {var(-16, 16, 0, true), -4}}, 0, fmt::format("h11 {}", i));
   		}
   	}
   }
   ```
   Add `#include <cstdlib>` if the file lacks it. If `EventIndex` returns SIZE_MAX for any of these (a crosser lacking the crossing), call `fail` with "structure <i> lists a crossing its crosser lacks" exactly as the "Structures: one axis" loop does for its separator.
2. In the sweep TEST_CASE: (a) when `BuildPlanarStructure` returns no variants, print `SWEEP <hub> STRUCTURE_WHY <why>` and, for each entry of `skipped`, `SWEEP <hub> SKIPPED <count> <reason>`, plus `evaluated=<n>`; (b) after the SWEEP summary line of each hub, also print `SWEEP <hub> shape_cost_sum=<sum of v.cost over shaped variants>`.
3. Add one line to src/junction/planar/CLAUDE.md, at the end of the "Shape layer" section: "- `PLANAR_H11` (probe, SPEC-slots Q1): a structure's crossers are made parallel at their crossings (headings equal mod 4); off by default."

## Run
- Build: `cmake --build /home/pete/OpenTTD/.claude/ng1/build-rel --target openttd_test -j16 > /home/pete/OpenTTD/.claude/ng1/build-rel/sx1-build.log 2>&1`; grep for `error`.
- Twice, by exact test name (Catch2 splits names on commas, so the comma is escaped):
  - `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar sweep: Structure and Shape over T1-T3\, LL2 and LL3a" > sx1-off.txt 2>&1`
  - `cd /home/pete/OpenTTD/.claude/ng1/build-rel && PLANAR_H11=1 timeout 900 ./openttd_test "planar sweep: Structure and Shape over T1-T3\, LL2 and LL3a" > sx1-on.txt 2>&1`
  - then `grep SWEEP` each, keeping only the summary lines, the shape_cost_sum lines, UNSHAPED, BREACH, STRUCTURE_WHY and SKIPPED lines (drop C1/C2 detail lines).
- Fast gate: `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`.

## Do not
- Change any other file or row; turn the flag on by default; fix any unshaped variant, breach or LL3a's failure; add REQUIREs on measurements.
- Run the wide gate or any other heavy job.

## Stop conditions
- A run exceeds 900 s: report what printed and stop.
- An unshaped variant's `why` with the flag on names rows other than `h11`, `cap`, `cross`, `axis`, `face`, `struct`, `node`/`split`/`merge`, `port`, `route`, `beside`, `mirror`: report it verbatim (it is data, not a stop) — stop only if the build or the flag-off run differs from commit 15f22f629e's numbers (variants, shaped, breaches, closes, c2_bad) for T1, T2, T3, LL2.

## Gates and commit
- Build clean; `[planarshape]` fast green inside 10 s; the flag-off numbers equal 15f22f629e's.
- One commit on wt-shape: `Add: Junction planar PLANAR_H11 probe: a structure's crossers parallel` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short, this format)
- References read
- Commit: hash and subject
- Flag off and flag on: the kept SWEEP lines verbatim, off first
- `[planarshape]` result: cases, passed, time
- Stops, anything left undone
- State: `git status --porcelain` and `git log -1 --oneline`
When you have reported, stop.
