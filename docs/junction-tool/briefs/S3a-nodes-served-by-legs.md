# S3a: nodes are served by their legs (D234)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-s3a <HEAD given at launch>`. Builds `build`, `build-rel`, `build-asan` exist; build `openttd_test`. Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/s3a/`. Never touch `.claude/nodes2`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `docs/junction-tool/DECISIONS.md` row D234; `docs/junction-tool/RECON-s3-probe.md` (all; it has the cycles, points and line numbers); `docs/junction-tool/DESIGN-G2-general-graph.md` §G2.4 and §G2.6; `src/junction/placement/CLAUDE.md`; `src/junction/placement/junction_placement.cpp` 240-300 (band chain points), 540-560 (`node_item`), 610-625, 1320-1395 (its callers); `src/junction/junction_pipeline.cpp` 105-195 (`CutJunctionTrackBridges`).

## Tests, red first (`src/tests/junction/topology/junction_topology.cpp`, tag `[junction][topology]`; if `[topology]` goes over 5 s Debug, tag the new tests `[topologysweep]` and say so)
1. "placement: a node lies on the item its in edges end on": LL_RR x LL_RR, pairing any, through `MakeJunctionGraphSeed` (as the existing pairing test does). Assert the error text, if any, does not contain "stack rows (row 1) <- row nodes" (the RECON class (b) cycle). Red: today's text contains it.
2. "placement: an edge leaving a merge meets it by a run": L_R x LL_RR, pairing any. Assert the error text, if any, does not contain "curve gap (edge 17) <- S-bend size (edge 9)". Red: today's text contains it.
3. "bridge cut: the reported failure is the default cut's": L_R x L_RR, pairing any. Assert the error text is the default cut's (RECON: "the placement does not settle"), not "bridge heads hug crossers". Red: today's text.
Record each red text. A test may go green while the seed still fails later with another reason: that is expected; report the new reason per topology.

## Changes
A. `node_item` (jp ~544) becomes a node's item: for a node n, the last item-naming token (Fan, Corner, SBend with index != NO_ROW) of each edge with `to.node && to.index == n`; edges that name none are skipped; if two in edges name different items, `return fail("a node's in edges end on different items")`-style refusal through the function's existing failure path (match how nearby code refuses); none -> NO_ROW (today's fallback). Callers (jp 620, 1330, 1334, 1389) pass the node instead of an edge: 1330/1389 the in edge's `to` node, 1334 `out.from`, 620 the node its in edge ends at. Edges starting at a node are never read. Update the comment on each caller.
B. Band chain points (jp ~273-276): an element's `first` is the edge's `start` only when the edge starts at a split and the element is its first token and is a Diverge S-bend or a Corner (the split-leaving Corner); its `last` is `stop` only when the edge ends at a merge and the element is its last token and is a Converge S-bend (as today). Every other first/last is `new_pt()` and is joined to the node by the chain run that the floor table already holds for that pair. If the floor table has no entry for (node, Corner) or (node, Fan) at a merge start, STOP and report the missing pair; do not add a floor. Express "starts at a split / ends at a merge" with the graph's node kind as the surrounding code already does (find the existing predicate; do not add a new member).
C. `CutJunctionTrackBridges` (junction_pipeline.cpp): keep the search; when no cut succeeds, report the error of the default cut (the cap-2 series from the start, the one tried first by the existing loop, or name the condition that identifies it) instead of `last_error`. If the default cut is not identifiable from the loop without a new mechanism, STOP and report.
D. CLAUDE.md, no D-ids or design/brief/recon references:
 - `src/junction/placement/CLAUDE.md`: replace "A row's in or out edge meets its node on the row of the stack item named by its nearest Fan, Corner or S-bend token; bridge tokens name edges, not items." by "A node lies on the stack item its in edges end on (their last Fan, Corner or S-bend token); an edge starting at a node is never read for its item; bridge tokens name edges, not items." Replace "An S-bend is pinned to a node only by its role (Diverge to its split, Converge to its merge), never by its token position." by "Only a token that serves a node shares its point: a Converge S-bend ending at a merge, a Diverge S-bend or a Corner leaving a split; an edge starting at a merge or ending at a split meets it by a chain run."
 - `src/junction/CLAUDE.md` (root) Pipeline: "- The bridge-cut search reports the default cut's failure (series of at most the cap from the start) when no cut places."

## Do not
No new types, members, floors or relations; no change to programs or topology; no change to the objective or rounds; no hiding or retagging of existing tests except the new ones per the budget rule. No `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Identity hashes or an existing test change (T and LL_RR x L_R seeds must not move: report any moved cell and stop).
- The floor table lacks a needed pair (B), or the default cut is not identifiable (C).
- A crash or ASan error in build-asan on the three topologies.
- Anything not covered.

## Gates
0 new warnings; the three new tests in build-asan (no ASan error); `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt gate build-rel`; `tools/junction/jt identity build-rel`. Commits on wt-s3a: A+B with test 1-2 `Fix: Junction nodes are served by their legs`; C with test 3 `Fix: Junction bridge cut reports the default cut's failure`; each ends with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commits; red evidence; per topology (L_R x L_RR, L_R x LL_RR, LL_RR x LL_RR) the seed result after the change (success, or the new error text in full); gate and identity counts and times; stops; `git status --porcelain`, `git log -2`. When you have reported, stop.
