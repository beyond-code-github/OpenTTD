# G3.1b: band chains and row items read the programs' shapes (DESIGN-G3 §G3.1), identity kept

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Placement stops inferring three things from token order and reads them from `programs.nodes` and `programs.ends` (added in G3.1a): a row node's item, a band edge's start heading, and which band-chain bend is pinned to its node's point. No layout may move on T or LL_RR x L_R; identity stays.

## State
Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-g31b <HEAD given at launch>`. Builds `build` (Debug) and `build-rel` exist. Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g31b/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.1;
- `src/junction/topology/junction_topology.h`: `JunctionTrackEdgeEnd`, `JunctionTrackNodeShape`, `JunctionTrackPrograms`;
- `src/junction/topology/junction_topology.cpp` 1219-1300 (how `ends` and `nodes` are computed);
- `src/junction/placement/junction_placement.cpp`:
  - 205-300 (the `Elem` struct and the band chain builder: heading `cur` at ~251, pinning at ~273-277);
  - 540-560 (`node_item`) and its callers at ~620, ~1330, ~1334, ~1389;
- `src/tests/junction/topology/junction_topology.cpp` 5000-5130 (the two G3.1a tests and `NodeShapeTopologies`);
- `src/junction/placement/CLAUDE.md`.

## Today's pin rule (band chains, jp ~273-277), restated exactly
For token `t` of band edge `e`:
- `first` is the start node's point when `t == 0`, the edge starts at a node, and either the token is a Corner, or the start node is a band split and the token is not a Converge S-bend. Otherwise `first` is a new point.
- `last` is the end node's point when `t` is the last token, the edge ends at a merge, and the token is an S-bend whose role is not Diverge. Otherwise `last` is a new point.

The data's rule (`ends`, jt ~1240-1252): start token 0 when the edge starts at a split and token 0 is a Corner or a Diverge S-bend; end token `size - 1` when the edge ends at a merge and the last token is a Converge S-bend. Port ends are not part of this step: at ports placement keeps `new_pt()` as today.

The two rules differ in wording (a Corner out of a merge; a Fan or a role-None S-bend at a band split's start; a role-None S-bend at a merge's end). Whether they differ in fact is what step 1 measures.

## Tests, in order
1. **Characterisation first (outer, before any placement change).** Extend "programs: node shapes match placement's inference" with a test-only copy of today's pin rule above (cite jp ~273-277 in a comment). For every band edge (`is_row_edge` false, as the test already decides it) and each end at a node: today pinned (yes/no, token) against `ends[e]` token.
   - On T and LL_RR x L_R: CHECK equal.
   - On L_R x L_RR, L_R x LL_RR, LL_RR x LL_RR: WARN each disagreement as `edge e start|end: old <token|none> -> new <token|none>`; CHECK nothing.
   Run it. **If T or LL_RR x L_R disagree anywhere, STOP here**: report the disagreement list, revert to a clean tree and commit nothing. Otherwise record the WARN list and go on.
2. The switch below is a refactor under identity: no new red test. The evidence is the identity hashes, the gate and test 1 staying green.

## Changes (`src/junction/placement/junction_placement.cpp` only)
1. **Start heading.** `int cur = ...` (~251) becomes `int cur = programs.ends[e].first.heading;`. Keep the loop that refuses a band edge carrying exit and joiner paths, and keep `from_main` only if something else still reads it; if nothing does, remove it.
2. **Pinning.** Replace the three pin lines with:
   ```cpp
   el.first = edge.from.node && programs.ends[e].first.token == t ? start : new_pt();
   el.last = edge.to.node && programs.ends[e].second.token == t ? stop : new_pt();
   ```
   (`t` is `size_t`; compare with a cast, no warning.) Leave `el.joiner_div` and `el.free_m` as they are. `from_band_split` and `to_merge` stay only if still read.
3. **Row items.** Remove the `node_item` lambda. Each caller reads the node the edge meets on its row:
   - `node_item(in_edge[r])` → `programs.nodes[graph.edges[in_edge[r]].to.index].item`;
   - `node_item(out_edge[r])` → `programs.nodes[graph.edges[out_edge[r]].from.index].item`.
   Worked call site (~1334): `eq(1, node_pt[out.from.index], item_pt[programs.nodes[out.from.index].item], 0);`. If at any caller that end is not a node, STOP.
   Update the comments that described the inference to say the item comes from the programs' node shape. Every comment explaining why a rule exists stays.
4. Test 1's copy of `node_item` stays in the test (it is the oracle); fix its comment to say the placement rule it copies was removed in this step, as of this commit.

CLAUDE.md, with no D-ids or design, brief or recon references: in `src/junction/placement/CLAUDE.md` add, where the chains are described: "- A band chain's start heading, which of its bends begins or ends exactly on a node's point, and a row node's stack item are read from the programs' edge ends and node shapes; placement infers none of them from token order."

## Do not
- Change the topology stage, the programs, render, the level solve, the row-edge chain builder beyond the one `node_item` call, or any rule or floor.
- Touch port-end pinning.
- Remove `index`, `role` or `joiner_div`.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Test 1 disagrees on T or LL_RR x L_R.
- Identity moves, or any existing test changes outcome or message (including the failing topologies' pinned failure texts in the sweeps): report which, old and new, and revert.
- A `node_item` caller's edge does not meet a node at that end.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`.

One commit on wt-g31b: `Codechange: Junction placement reads node shapes and edge ends`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- test 1: equal on T and LL_RR x L_R (yes or no); the token disagreement list on the three failing topologies, verbatim, at most 40 lines;
- members or lambdas removed;
- gate and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.
When you have reported, stop.
