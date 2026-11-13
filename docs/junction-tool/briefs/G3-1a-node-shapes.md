# G3.1a: node shapes and edge ends as program data (DESIGN-G3 §G3.1), no behaviour change

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
The programs stage emits each node's shape and each edge end's heading as data. Placement does not read them yet, so no layout changes. A test compares the data with what placement infers today, and lists the disagreements on the failing topologies.

## State
Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-g31a <HEAD given at launch>`. Builds `build` (Debug) and `build-rel` exist. Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g31a/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.1;
- `docs/junction-tool/REVIEW-graph-model.md` Q2 (the "Node geometry", "Which bend serves which node", "A node's row" and "Edge headings" items);
- `src/junction/topology/junction_topology.h` 90-110 and 200-243;
- `src/junction/topology/junction_topology.cpp`:
  - 966-1010 (start of `ProgramJunctionTrackGraph`);
  - 1060-1095 (`siblings`, `diverges`);
  - 1145-1165 (`heads_south`);
- `src/junction/placement/junction_placement.cpp`:
  - 240-300 (band chain heading and first/last pinning);
  - 540-560 (`node_item`);
  - 960-1005 (node tracks inference);
- `src/junction/topology/CLAUDE.md`.

## Types (in `junction_topology.h`, next to `JunctionTrackPrograms`)
```cpp
/** Where an edge meets a node or port: its heading there and the token whose end is that point (DESIGN-G3 §G3.1). */
struct JunctionTrackEdgeEnd {
	static constexpr uint32_t NO_TOKEN = UINT32_MAX;
	uint8_t heading; ///< Travel heading at the end: 0 N, 1 E, 2 S, 3 W (as placement's chain headings).
	uint32_t token = NO_TOKEN; ///< Index into the edge's program of the token whose end is the end's point, or NO_TOKEN when a run joins it.
	bool operator==(const JunctionTrackEdgeEnd &) const = default;
};

/** A node's shape: its directions and row, never its position (DESIGN-G3 §G3.1). */
struct JunctionTrackNodeShape {
	uint32_t item = JunctionTrackStep::NO_ROW; ///< The stack item the node lies on; NO_ROW for a band node.
	uint8_t heading; ///< The trunk's travel heading at the node: 0 N, 1 E, 2 S, 3 W.
	uint32_t trunk; ///< The edge into a split, or out of a merge.
	uint32_t straight; ///< The leg that keeps the trunk's heading.
	uint32_t diverging; ///< The other leg.
	int8_t hand; ///< The diverging leg's side: +1 right, -1 left, seen in the travel direction (as JunctionTrackStep::hand).
	bool operator==(const JunctionTrackNodeShape &) const = default;
};
```
In `JunctionTrackPrograms` add:
- `std::vector<JunctionTrackNodeShape> nodes;` with the comment "Per graph node.";
- `std::vector<std::pair<JunctionTrackEdgeEnd, JunctionTrackEdgeEnd>> ends;` with the comment "Per graph edge: its start and its end (copy 0 for a doubled edge)."

## How each field is computed (in `ProgramJunctionTrackGraph`, after the edge programs exist; reuse the existing lambdas, add no new helper type)
- **`trunk`, `straight`, `diverging`, `hand`.** A split's trunk is its single in edge, and a merge's is its single out edge. The legs are the node's two other edges. Choose `straight` and `hand` by the rule of `diverges` (jt ~1082): for band nodes use `diverges` itself. For row nodes, use the leg that stays on the node's row as `straight`. The doubled-leg rule decides which leg that is; find it in the code and cite the line. If a row node's straight leg cannot be read from existing code without choosing, STOP and report.
- **`heading`.** The trunk's heading at the node, by the same rule that sets `cur` for the trunk edge at jp ~246-252. That rule is the row heading on a row, else south for exit paths and north for joiner paths, matching `heads_south` at jt ~1153.
- **`item`.** The last Fan, Corner or S-bend token with `index != NO_ROW` of each edge that ends at the node. A band node gets NO_ROW. If two in edges disagree, keep the trunk's (for a split) or the straight leg's (for a merge), and record the disagreement for the test below. Do not refuse.
- **Edge ends.**
  - `heading`: the heading at the edge's start or end, from the chain rule (start: the `cur` rule; end: after applying each token's turn, as placement does at jp ~265-271).
  - `token`: the start token is the first token if the edge starts at a split and that token is a Diverge S-bend or a Corner leaving a split (`joiner_div`, jp ~274). The end token is the last token if the edge ends at a merge and that token is a Converge S-bend. Everything else is NO_TOKEN.
  - Edges at ports: heading only, and token NO_TOKEN unless a Fan is the port end's first or last token, in which case it is that Fan's index.

## Test (`src/tests/junction/topology/junction_topology.cpp`)
1. "programs: node shapes match placement's inference". This is outer and characterisation only, so no red is expected. Record that.
   - For T and LL_RR x L_R, at the seed's structure and programs, compare each node's `item` with what placement uses today: `node_item` for row nodes, via a test-visible computation that reproduces jp ~544 over the same programs. Compare each node's `heading` and each edge end's `heading` with jp ~246-252 (start) and the chain walk (end). All must be equal.
   - For L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR (pairing any), WARN each disagreement as `node n: field old -> new`, plus each node whose in edges end on different items. CHECK nothing on these three.
   - Tag `[junction][topology]`. If `[topology]` goes over 5 s Debug, tag it `[topologysweep]` and say so.
   Reproducing placement's inference in the test is a test-only copy of today's rule. Name the jp lines in a comment. Do not export placement internals.
2. "programs: every node has a shape and every edge two ends". For the five topologies under pairing any, wherever programs succeed: `nodes.size() == graph.nodes.size()` and `ends.size() == graph.edges.size()`, a trunk is an incident edge, `straight != diverging`, and `hand` is ±1. Red: the members don't exist yet, so stub them empty first, run, and record the failure.

## Do not
- Change placement, render, the level solve or any layout, or any existing program token.
- Remove or change `index`, `role`, `node_item` or the jp inference.
- Add a field beyond those listed.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Identity or any existing test changes.
- A field cannot be computed from existing code without choosing between rules: name the field and the candidates.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`.

CLAUDE.md, with no D-ids or design, brief or recon references, in `src/junction/topology/CLAUDE.md` under "Graph, order and structure", add: "- The programs emit each node's shape (its stack item, trunk heading, trunk, straight and diverging legs with the diverging hand) and each edge's two ends (heading, and the token whose end is the node's point, if any)."

One commit on wt-g31a: `Add: Junction programs emit node shapes and edge ends`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence for test 2;
- test 1: equal on T and LL_RR x L_R (yes or no); the disagreement list on the three failing topologies, verbatim, at most 40 lines;
- gate and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.
When you have reported, stop.
