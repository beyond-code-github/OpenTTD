# HH3: levels and terraform stay inside the map's height range (REVIEW-opus-since-move F7, first half)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2. Run `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-hh3 <head>` with the head given in your launch prompt. Builds `build` (Debug), `build-rel` (Release); if a build dir is missing or stale, configure it as the existing one in /home/pete/OpenTTD/.claude/nodes is configured and say so. Ignore docs/junction-tool/wip/. Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes; never touch it.
- Finding F7 in docs/junction-tool/REVIEW-opus-since-move.md (lines 76-86): the level solve (src/junction/placement/junction_levels.cpp, about line 352) lets levels run from 3 below the lowest ground to 3 above the highest, with no bound from the map. The game refuses a corner above `_settings_game.construction.map_height_limit` (terraform_cmd.cpp:108, :343: "too high") and below 0 ("already at sea level"). A plan that needs such a corner shows green and fails in game after the irreversible terraform.
- The rule: every corner the plan ends at lies in [0, map height limit]. The one place that bounds corners is the corner cone (`MakeCornerCone`, junction_levels.cpp:231): its `lower`/`upper` per corner already filter level options (about lines 403-430) and clamp the terraform (about line 699). The clamp goes there and nowhere else.
- The second half of F7 (terraform test-run, tunnels under snapshot tiles) is HH4 and not this step.

## Read first
src/junction/CLAUDE.md, src/junction/placement/CLAUDE.md (Heights), src/junction/terrain/CLAUDE.md if present, src/tests/junction/CLAUDE.md, CODINGSTYLE.md (naming, comments); junction_levels.cpp 205-300 and 340-440 and around 699; src/junction/terrain/junction_terrain.h 40-90; junction_terrain_json.cpp 35-70; src/junction/game/junction_game.cpp around line 364 (`max_bridge_height` in the snapshot); the snapshot tests in src/tests/junction/game/junction_game.cpp around lines 301 and 385.

## Change
1. `JunctionTerrain` (junction_terrain.h) gains `int max_height = 255; ///< The highest corner height the game allows (map_height_limit); the lowest is 0.` next to `max_bridge_height`.
2. The game snapshot (junction_game.cpp, next to line 364) sets `terrain.max_height = _settings_game.construction.map_height_limit;`.
3. The JSON writer and reader (junction_terrain_json.cpp) write `max_height`, and read it when present (`in.contains("max_height")`), keeping the default otherwise (the 48 fixtures have no such field, so they keep 255 and do not move).
4. `MakeCornerCone`: after the two distance-transform passes, clamp every corner: `lower[k] = std::max(lower[k], 0)`, `upper[k] = std::min(upper[k], terrain.max_height)`. If a corner then has `lower > upper` (unmovable ground outside the range cannot occur in a game snapshot), leave it; the existing filters refuse every option there. Update the function's doc comment: "... and every corner stays within [0, max_height], the game's height range (terraform_cmd.cpp)."

## Tests (red first)
1. In src/tests/junction/game/junction_game.cpp, extend the existing snapshot test at about line 301 (the one that checks `max_bridge_height`) with `CHECK(terrain.max_height == _settings_game.construction.map_height_limit);`. This is the one existing test you may change, by that one line. Red: does not compile before change 1.
2. A new test "levels: no plan leaves the map's height range (F7)", tags `[junction][realterrain]` in src/tests/junction/tool/junction_real_terrain.cpp. Find the red case first, as a probe, before change 4: for the 48 fixtures and the offsets the existing `[realterrain]` tests use, set `terrain.max_height` to the fixture's highest corner (`z_max` over all tiles) and plan; collect every case whose plan's levels or terraform corners exceed `max_height` or go below 0. Pick the first such case (fixture, offset) and make the test assert, for that case, that the plan either is refused or keeps every level and every terraformed corner in [0, max_height]. Record the red text. Keep the test inside the `[realterrain]` budget (docs/junction-tool/TESTS.md); one fixture, one offset.
   - If the probe finds no such case in any fixture, stop and report that (the hole is then unreachable on the fixtures; the orchestrator will decide).

## Do not
- Do not bound the level variables' range (`base`, `top`) separately; the cone carries the rule.
- Do not change terraform posting, pricing, the hover search, or HH4's items.
- Do not change any existing test other than the one line in test 1.

## CLAUDE.md (same commit)
In src/junction/placement/CLAUDE.md, Heights, after the line beginning "The level solve costs terraform exactly": add "Every corner a plan ends at lies in [0, map height limit], as the game's terraform allows; the corner cone carries the bound (HH3, terraform_cmd.cpp)."

## Stop conditions
- Any identity number changes (the fixtures keep max_height 255, so none may move), or any existing `[realterrain]`/`[realterrainsweep]` count drops.
- No red case on the fixtures (see test 2).

## Gates
- Before: `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` for a baseline.
- After: Debug and Release build with no new warnings; gate passes with 1 case added; identity unchanged; `tools/junction/jt fast build '[realterrain]'` and `tools/junction/jt fast build '[game]'` within budget.
- One commit: `Fix: Junction levels and terraform stay inside the map's height range`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit hash and subject; the red case (fixture, offset, which corner) and red text; counts before and after; stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
