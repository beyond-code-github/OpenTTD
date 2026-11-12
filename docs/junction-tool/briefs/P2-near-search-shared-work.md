# P2: the near search does offset-independent placement work once (hover headroom; F1, LS1, HC1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-p2 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes2 (on src/junction_topology.cpp roles): keep your topology changes small and local. Use absolute paths.
- HOVERNEAR (PlanJunctionHoverNear, src/junction_hover.cpp: up to 153 offsets) is at 28.7 ms mean / 90.7 ms max (one run 94.0) against the 100 ms limit after HC1 (da3e73a2b0). F1's per-offset timing (docs/junction-tool/briefs/F1-near-frame-search.md Amendment 1 report, quoted): placement 0.28-0.38 ms per offset of which the flat Y+X solve (plan_clearance) 0.08-0.12 ms, the Y solve ~0.03 ms, the X solve 0.15-0.20 ms; render+signals 0.4 ms when the placement succeeds; the level solve now ~3.5 ms when reached. On a fixture with no plan ~150 placements dominate.
- The facts placement's Y phase does not read facts ("Rows stay as the structure gives them", junction_topology.h facts overload doc), and the flat reference solve (plan_clearance) is made without facts: both look offset-independent. Verify.

## Read first (once each)
CODINGSTYLE.md (C++ part); src/junction_hover.{h,cpp}; src/junction_topology.h the facts overload of PlaceJunctionTrackGraph; src/junction_topology.cpp its body: which parts read `facts`, `offset`, `terrain`.

## Step 0 (findings, no code change)
List the stages of the facts placement and, for each, whether it reads facts/offset/terrain (file:line). Measure (temporary timers, removed after) the per-offset time of each stage on temperate-hilly-15 and temperate-mountainous-6. If neither the flat solve nor the Y phase is offset-independent, stop and report.

## Change
The offset-independent stages are computed once per PlanJunctionHoverNear call and reused for every offset, through ONE new input of the facts overload: `const JunctionPlacementShared *shared = nullptr` (a struct in junction_topology.h holding exactly the results step 0 shows independent, with a `MakeJunctionPlacementShared(...)` function taking the same graph/structure/programs/spec/choices inputs). Without it, the overload computes them itself as today. PlanJunctionHover (single offset) does not need it. No other change.

## Tests (record each red)
1. **Outer:** `[realterrainsweep]` LEVELHASH (hash and planned unchanged: identity), HOVERPLAN unchanged, HOVERNEAR planned 31 with ms_mean/max reported; the identity of every plan (layout tracks/signals, levels, terraform) with and without `shared` over all 48 fixtures x 153 offsets (a `[realterrainsweep]` check, or fold it into the LEVELHASH test by computing both: say which).
2. Report HOVERNEAR max before/after (3 runs each).

## Stop conditions
- Any identity difference; the saving is below 15% of HOVERNEAR max (report the stage times and stop without committing); anything not covered.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `'[topology]' 5`; `build-rel/openttd_test "[realterrainsweep]"`; `tools/junction/jt gate build-rel` (count, time). TESTS.md rows.
- One commit: `Change: Junction near search computes the offset-independent placement once`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus step 0's table and HOVERNEAR before/after. When you have reported, stop.
