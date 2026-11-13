# HH2: a bridge middle never spans a Blocked tile (D175)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. Run `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-hh2 <head>` with the head given in your launch prompt. Builds: `build` and `build-rel`. Ignore docs/junction-tool/wip/. Never use git stash.
- Review finding F6 (docs/junction-tool/REVIEW-opus-since-move.md):
  - JunctionFacts has no fact for a span tile, so placement never checks the middle tiles of a bridge.
  - D175 says bridges never span blocked tiles. D104 says they may span plain rail and road (class Spannable) and water.
- Tile classes are in terrain/junction_terrain.h: Buildable, Water, Spannable, Blocked.
- terrain/CLAUDE.md: "Only hard facts fail a solve".
- Tunnels are exempt, because they may pass under anything (D175).

## Read first
src/junction/CLAUDE.md, terrain/CLAUDE.md, placement/CLAUDE.md, src/tests/junction/CLAUDE.md, review F6, terrain/junction_facts.{h,cpp}, and the bridge part of `PlaceTrackGraph` in placement/junction_placement.cpp. That part is the end of the function, where bridge heads, edge spans and bridges are filled into the result and the max-span checks refuse with an error. Find it by `max_bridge` or `bridges`.

## Tests (red first)
1. In src/tests/junction/terrain/junction_facts.cpp, test "a span fact is false only on Blocked tiles and outside the snapshot", tags `[junction][facts]`.
   - On a small hand-built JunctionTerrain with one tile of each class, `facts.Spannable(x, y)` is true for Buildable, Water and Spannable, false for Blocked, and false outside the snapshot.
   - Red: before the member exists this does not compile. So first add the member returning `true` always, then record the assertion failure on Blocked.
2. In src/tests/junction/topology/junction_topology.cpp (the file with the existing placement-on-facts tests; copy their setup), test "a bridge middle over a Blocked tile fails the placement naming the tile", tags `[junction][topology]`:
   - Place the T on flat terrain with facts, the way an existing test does.
   - Take the first non-tunnel bridge of the result and one middle tile strictly between its heads. Make that tile Blocked in a copy of the terrain, rebuild the facts, and place again with the same fixed choices and offset.
   - Assert that placement fails and that the error contains "bridge span over a blocked tile".
   - Red: before the change, the placement succeeds.

## Changes
- `bool JunctionFacts::Spannable(int x, int y) const` in terrain/junction_facts.{h,cpp}. Doc comment: "Whether a bridge middle may pass over the tile: inside the snapshot and not Blocked (D175, D104)". Store the class per cell only if it is not already stored; reuse what Buildable() reads.
- In PlaceTrackGraph's bridge phase, after the heads of every bridge and every copy are known and before the result is returned:
  - For each non-tunnel bridge, check every middle tile, in the facts frame and using the same offset as the other fact queries, with `Spannable`.
  - On the first failure, set the error to `fmt::format("bridge span over a blocked tile at ({}, {})", x, y)` in the placement frame, following how the max-span refusal sets its error, and return failure the same way.
  - Only when facts are given. The no-facts overloads skip the check, as the other fact checks do.
- terrain/CLAUDE.md or placement/CLAUDE.md, whichever holds the bridge or fact facts: add "A bridge middle never spans a Blocked tile; placement fails naming the tile; tunnels are exempt (D175, HH2)."

## Do not
Turn this into a constraint or relation; it is a hard-fact check only. Change no other check. Do not touch tunnels.

## Stop conditions
- The bridge phase has no single place where every head is known with facts available.
- Any identity hash or planned count changes. Report which values, and do not update expected values.
- A [realterrainsweep] max goes over 100 ms.

## Gates
- Before the change, run `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` from the worktree root. Expect 490/3041671 for the gate and 4/255 for identity.
- After the change:
  - Debug and Release build, with no new warnings.
  - gate passes, with 2 cases added.
  - identity is unchanged.
  - `tools/junction/jt fast build '[facts]'` and `tools/junction/jt fast build '[topology]'` pass.
- One commit: `Fix: Junction placement refuses a bridge middle over a blocked tile`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- References read.
- Commit hash and subject.
- Red evidence for both tests.
- Members added.
- Before and after counts.
- Stops.
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
