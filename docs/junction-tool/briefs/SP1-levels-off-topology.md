# SP1: the level solve stops depending on topology.h (identity step)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. Run `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-sp1 <head>` with the head given in your launch prompt. Builds: `build` (Debug) and `build-rel` (Release). Ignore docs/junction-tool/wip/. Never use git stash.
- D226 (DECISIONS.md, last row) splits junction_topology.cpp along the RS1 seam. This is its first step.
- docs/junction-tool/RECON-topology-placement-seam.md §4 shows that placement/junction_levels.h includes topology/junction_topology.h only for these three things:
  - `JunctionTrackPlacement`, of which levels.cpp reads only `.nodes` and `.ports` (around junction_levels.cpp:1519-1520);
  - `JunctionTrackRoute` and `JunctionTrackPieceTag`;
  - `JunctionLayout`, which already lives in the root junction_layout.h.

## Read first
src/junction/CLAUDE.md, src/junction/placement/CLAUDE.md, src/tests/junction/CLAUDE.md, the RS1 recon §4, src/junction/placement/junction_levels.h (all of it), src/junction/junction_route.h (all of it), and the two struct definitions in topology/junction_topology.h (around lines 345-375).

## Changes
1. Move `JunctionTrackPieceTag` and `JunctionTrackRoute` VERBATIM, with their doc comments, from topology/junction_topology.h to the end of the declarations in src/junction/junction_route.h. Add only the includes they need. topology/junction_topology.h includes "../junction_route.h" if it does not already, so every existing user still compiles unchanged. If either struct needs a type that is declared only in junction_topology.h, stop.
2. Change `SolveJunctionLevels`: replace the parameter `const JunctionTrackPlacement &placement` with `const std::vector<TileIndexDiffC> &nodes, const std::vector<TileIndexDiffC> &ports`, in the same position.
   - Its doc comment gets two @param lines, adapted from the existing "@param placement The placement (ports and nodes)." line:
     - `@param nodes The placement's node tiles (JunctionTrackPlacement::nodes).`
     - `@param ports The placement's port boundary tiles (JunctionTrackPlacement::ports).`
   - In junction_levels.cpp, `placement.nodes` becomes `nodes` and `placement.ports` becomes `ports`, keeping the same order of use.
   - If levels.cpp reads any other field of `placement`, stop.
3. Every caller of SolveJunctionLevels passes `placement.nodes, placement.ports` (or whatever its placement variable is called) in place of the placement. Find them all with grep or LSP, in src/junction and in the tests.
4. junction_levels.h replaces `#include "../topology/junction_topology.h"` with `#include "../junction_route.h"`. junction_levels.cpp keeps any other include it needs. If levels.cpp still needs junction_topology.h for anything, list exactly what and stop.
5. CLAUDE.md: in src/junction/CLAUDE.md, add `JunctionTrackRoute`/`JunctionTrackPieceTag` to the description of junction_route on the root-files line. Remove nothing else.

## Do not
Change any logic, iteration order, error text or reason literal. Do not move other types. Do not touch junction_topology.cpp beyond what the compiler needs, which should be nothing.

## Stop conditions
The ones named above. Any identity hash or test count change.

## Gates
- Before the change, run `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` from the worktree root to get a baseline. The last baseline was 489 cases and 3041662 assertions for the gate, and 4 cases and 255 assertions for identity.
- After the change:
  - Debug and Release both build `openttd` and `openttd_test`, with no new warnings. The known set is 16 in Debug.
  - gate and identity show the same counts.
  - `tools/junction/jt fast build '[topology]'` passes, and so does `tools/junction/jt fast build '[levels]'` if that tag exists in TESTS.md.
  - `grep -n 'junction_topology.h' src/junction/placement/junction_levels.h` returns nothing.
- One commit: `Codechange: Junction level solve no longer depends on the topology header`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- References read.
- Commit hash and subject.
- Baseline and after counts.
- Callers changed, as a count, plus each file.
- Includes changed.
- Stops.
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
