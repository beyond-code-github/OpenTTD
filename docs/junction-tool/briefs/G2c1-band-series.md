# G2c-1: a band edge carries a series of single bridges, one per crossing (DESIGN-G2 G2c, D228)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
The structure already gives a band edge (a non-row edge) one single bridge per crossing, each with one crossed edge, and its program has one Over (or Tunnel) token per crossing (RECON-G2c.md). Placement refuses every band edge with more than one bridge, "a bridge over several band crossings is not ruled yet", because its non-row chain builder and three later sites read `bridges_of[e][0]`. That refusal blocks LL_RR x LL_RR under keep and every 3-lane hub.

After this step:
- Each Over/Tunnel token of a band edge becomes its own Heads element, for the bridge whose `crossed` holds that token's crossed edge.
- Every site that read `bridges_of[e][0]` for a band edge's Heads element finds the element's own bridge the same way.
- One bridge over several crossings (a bridge whose `crossed` has more than one edge) stays refused, with the same text.

The bridges of a band edge are not in travel order in the structure (RECON-G2c.md §3), so a lookup by index is wrong. Always look a bridge up by its crossed edge.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes2`. First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-g2c1 <HEAD given at launch>`. Rebuild `openttd_test` in `build` and `build-rel` (delete stale `.pch` files if the build complains about them). Leave untracked files alone.
- Another agent works in `.claude/nodes` on `junction_placement.cpp` (the fixed path, `add_occupancy`, `build()`'s detect block). Never touch that worktree. Keep your edits to the sites named here, so the two merge cleanly.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g2c1/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/RECON-G2c.md`;
- `src/junction/placement/junction_placement.cpp`:
  - the non-row chain builder (grep "a bridge over several band crossings");
  - the row-edge chain builder's lookup of a bridge by `crossed[0] == step.index` (grep "crossed\[0\] == step.index"), the pattern to copy;
  - every other `bridges_of[e][0]` and `bridges_of[edge][0]` (grep "bridges_of\[[a-z]*\]\[0\]"): `collect_bridges`, `heads_of`, the result emission of an undoubled edge's Over token;
- `src/junction/topology/CLAUDE.md`, the "Bridges and tunnels" and "Graph, order and structure" bullets about band edges;
- `src/tests/junction/placement/junction_joint.cpp`: `MakeJointInputs`, `DefaultCut`.

## Changes in `junction_placement.cpp`
1. **The non-row chain builder.** For an Over or Tunnel step:
   - Find `b` = the bridge in `bridges_of[e]` whose `crossed` contains `step.index`, as the row builder does.
   - Refuse with the existing text "a bridge over several band crossings is not ruled yet" when no such bridge exists, when it is doubled, or when its `crossed.size() != 1`.
   - `el.cross = static_cast<int>(step.index)`. Nothing else in the element changes.
2. **Every other `bridges_of[e][0]` that serves a non-row edge's Heads element or Over token** finds the bridge by the element's `cross` (or the token's `index`), by the same lookup. That covers `collect_bridges` part (a), the undoubled branch of `heads_of`, and the result emission of an undoubled edge's Over token.
   - `heads_of(b, copy)` for an undoubled bridge returns the Heads element of `chain[edge]` whose `cross` is the bridge's crossed edge, not the first Heads element.
   - If a site needs a bridge index that neither `cross` nor the token gives, STOP and report it.
3. Nothing else changes: not the row edges, the doubled machinery, the structure, the cut search, render or signals.

## Tests
1. **"placement: a band edge carries a series of single bridges"** (`src/tests/junction/placement/junction_joint.cpp`, `[junction][milp]`; tag it `[junction][milp][.sweep]` if it takes over 1 s in Debug). Red first.
   - Inputs: LLL_RRR x LL_RR (`MakeJointInputs("LLL_RRR", "LL_RR")`, pairing any, `DefaultCut`).
   - CHECK that the structure has at least one band edge with two or more bridges (a precondition; if not, STOP).
   - Run `PlaceJunctionTrackGraph` (the rounds), and CHECK that the error, if any, is not "a bridge over several band crossings is not ruled yet".
   - Do the same for LL_RR x LL_RR under pairing keep, building its inputs the way the test "topology: LL_RR x LL_RR under each lane pairing seeds or names a reason" builds the keep topology.
   - WARN each hub's error or "places".
   - If a hub places: CHECK that for every band edge and every Over token t, `placement.edges[e][t]`'s span holds the column or row of its own crossed edge's track (the same check the row edges' Over spans get in the existing tests: read one and copy its form). If no such existing check exists, CHECK only that the span is non-empty and say so in the report.
   - Red: today's refusal text. Record it.
2. Every existing test stays as it is. Identity must not change: today no placed hub has a band edge with two bridges, because they were all refused.

## Do not
- Add bridge merging (one bridge over several crossings), a cut choice for band edges, or the structure's travel order.
- Touch the cut search's speed (RECON-G2c.md: LLL_RRR x LL_RR's cut search does not finish; that is a later step).
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Identity or an existing test changes.
- A site from change 2 cannot find its bridge from `cross` or the token.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt sweep build-rel`;
- `tools/junction/jt identity build-rel`. If a gate is over budget, re-run it once and report both times.

Update `src/junction/topology/CLAUDE.md`, standalone and without D-ids or references. Replace "a band edge carries at most one bridge, over one crossing; placement refuses more" with: a band edge carries one single bridge per crossing, in series along it; placement refuses a single bridge over several band crossings. Keep the rest of that bullet. One commit on wt-g2c1: `Feature: Junction band edges carry a series of single bridges`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- the sites changed;
- each hub's new error or "places";
- gate, sweep and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
