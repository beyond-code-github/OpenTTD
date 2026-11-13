# G2a: a split or merge with more than two keys becomes a cascade of two-way nodes

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree: given in your launch prompt, on a branch `wt-g2a` created there at the head given in your launch prompt. Builds: `build` (Debug) and `build-rel` (Release). Ignore docs/junction-tool/wip/. Never use git stash.
- The ruling is DESIGN-G2-general-graph.md §G2.1 and D228 (DECISIONS.md, last rows). Read both.
- Today `JunctionTreeBuilder::Partition` in src/junction/topology/junction_topology.cpp (around lines 267-293) refuses more than two keys with "more than two legs at one split/merge are not ruled yet". `Keys()` returns the keys already ordered (least counter-clockwise distance from slot s), and that is the lane order the cascade must keep.

## Read first
src/junction/CLAUDE.md, topology/CLAUDE.md, src/tests/junction/CLAUDE.md, CODINGSTYLE.md (C++ part), DESIGN-G2 §G2.1, junction_topology.cpp lines 233-380, and the existing graph tests in src/tests/junction/topology/junction_topology.cpp. Find them by `BuildJunctionTrackGraph`, and copy how they build a topology and paths.

## Tests (red first; in src/tests/junction/topology/junction_topology.cpp, tags `[junction][topology]`)
1. "a split to three lanes is a cascade of two-way nodes": the hub topology LLL_RRR main × L_R branch, with pairing any, built the way the existing tests build LL_RR × L_R. Assert:
   - (a) BuildJunctionTrackGraph returns no error;
   - (b) every node has exactly two child edges for a split, or two parent edges for a merge;
   - (c) along every path, all In edges come before all Out edges in graph.path_edges (D24);
   - (d) for the cascade at the split with 3 keys, the first node's two children carry key sets in lane order {k0} and {k1, k2}, i.e. contiguous halves with the smaller half first for an odd count (1 + 2).
   - Red: before the change, (a) fails with "more than two legs at one split are not ruled yet". Record that message.
2. "the cascade of four keys is two halves": a unit test of the same Partition behaviour with 4 keys, if a topology with 4 keys at one node is buildable from the notations (e.g. LLLL_RRRR × L_R). Assert children {k0, k1} and {k2, k3}. If no topology yields 4 keys at one node, skip this test and say so.

## Change
In `Partition`, replace the `keys.size() > 2` refusal with a cascade:
- When `keys.size() > 2`, the node's two children are the halves of the ordered key list: first `keys[0 .. h)`, then `keys[h .. n)`, with `h = n / 2` (so 3 gives 1 + 2 and 4 gives 2 + 2).
- A half holding one key is handled exactly like a key today: a leaf when it holds one path, else `Partition(key, by_slot, node, s)`.
- A half holding several keys becomes its own two-way node at the same `by_slot` level and the same slot s. Its edge carries the union of the half's paths. That node splits its half the same way, recursively.
- Write this as a private member function of JunctionTreeBuilder, `bool Cascade(const std::vector<std::vector<uint16_t>> &keys, bool by_slot, JunctionTrackEnd parent, uint16_t s)`, which `Partition` calls after computing keys. It holds the node and edge creation that is in Partition today. The node and edge creation code itself stays as it is.
- The edge's path list order: the union in key order.
- Merge trees use the same code (the `split` flag already mirrors edge kinds).

## Downstream
Order, structure, placement and so on may refuse the new graph later with their own errors. That is expected and out of scope. Test 1 asserts the graph only. If an EXISTING test changes result, stop.

## CLAUDE.md (same commit)
In topology/CLAUDE.md, replace "A switch has two legs; more are refused as not ruled (G1 §7)." with: "A switch has two legs; a lane bound for more than two keys splits as a cascade of two-way nodes over contiguous halves of the ordered key list (1 + 2, 2 + 2, ...); merges mirror it (D228, DESIGN-G2 §G2.1)."

## Do not
Change Keys(), the order or structure stages, or any refusal outside Partition.

## Stop conditions
- Any identity hash or existing test result changes.
- The topology LLL_RRR × L_R cannot be built by the existing test helpers.

## Gates
- Before the change, run `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` from the worktree root to get a baseline.
- After the change:
  - Debug and Release build, with no new warnings.
  - gate passes, with the new cases added.
  - identity is unchanged.
  - `tools/junction/jt fast build '[topology]'` passes within its budget.
- One commit: `Add: Junction graph splits and merges of more than two keys as cascades of two-way nodes`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- References read.
- Commit hash and subject.
- Red evidence.
- Members added.
- Baseline and after counts.
- What the LLL_RRR × L_R pipeline does next: which stage refuses it, and the error text. Run it through order and structure once and report; do not fix anything.
- Stops.
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
