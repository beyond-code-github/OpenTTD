# RW1: row item lookups in one helper; tokens carry their items (DESIGN-G1 §16.4 RW1; RECON-per-row-code.md K2-K7, R10, R11)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-rw1 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes: never touch it. Use absolute paths.
- This is a pure refactor: every seed, sweep number and the circuit file stay identical.

## Read first (once each)
CODINGSTYLE.md (C++ part); docs/junction-tool/RECON-per-row-code.md (whole, 49 lines; sites K2-K7, R10, R11 with line numbers at 7f5bc41e06); DESIGN-G1.md §16.4; src/junction_topology.cpp at those sites; src/junction_topology.h the token struct (its `index` member) and JunctionTrackStackItem.

## Changes (src/junction_topology.cpp only)
1. **One helper.** The lane/approach/north item lookups duplicated at K2 (~:709-718, programs) and K3 (~:997-1008, placement) become one file-local function in the anonymous namespace: `RowItems FindRowItems(const std::vector<JunctionTrackStackItem> &stack, int row)` with a file-local `struct RowItems { size_t lane; size_t approach; size_t north; };` (NO_ROW or the existing sentinel for "none"; keep the existing semantics exactly: north = the item before Lane unless that is Approach, as the code does today). Both sites call it.
2. **Tokens carry their items.** Every token whose row the placement derives from a row role gets its item in `index` when the program is built: the doubled leg's HeadsPair/Over tokens get the item of their copy's row (copy 0 = north item, copy 1 = lane item, K6 ~:1475); the in-edge Fan already gets north/lane (K4); LaneSplit/LaneMerge S-bends the lane item (K4); the joiner Corner the approach item and its arrival S-bend the lane item (K5) — confirm each already does, add only what is missing.
3. **Readers use the token's index.** K7 (~:2183 exit_item, ~:2232-2241 bend1/bend0 pins), R10 (~:2181-2187 node_pt Y relations) and R11's item pins (~:2227-2263) read the item from the relevant token's `index` instead of recomputing north_item/lane_item from the row. If a reader has no token to read from (the relation is about a node, not a token), use the token whose element starts or ends at that node, and say which in a comment; if none exists, stop and report.
- Comments: every existing comment at a moved site moves with it.

## Tests
1. **Outer (identity):** before any change, capture build-rel outputs of `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` to files; after, the same outputs differ only in timings (diff with times stripped); the LL_RR circuit file regenerates byte-identical; `[topology]` and `[topoproj]` pass unchanged.
2. No new test (refactor). If you find a site where the token's index and the recomputed item differ on any seed (add a temporary assert, run the sweeps, remove it), report the site and stop: that is a hidden behaviour.

## Do not
- No change to approach S-bend sizing (RW2), roles (RW3), row_pts (RW4), stack building (RW5), the prio (RW6).

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; the identity diffs above; `tools/junction/jt gate build-rel` (count and time).
- One commit: `Codechange: Junction placement reads row items from tokens through one lookup`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the sites changed (file:line before -> after) and the identity diff result. When you have reported, stop.

## Amendment 1 (orchestrator, after the stop)
1. **A node's row is the item of the token whose element ends (in edge) or starts (out edge) at that node**, not the edge's first token. For an in edge that is its last row-positioned token (Fan, SBend, Corner: the last one carrying an item); for an out edge its first. So R10's exit pin = the in edge's last positioned token's index; the lane pin on out.from = the out edge's first positioned token's index. On every seed that is the Fan (identity holds); on the test "a fan edge may hold more than its fan" it is the S-bend back to lane-1, which is the north row the old code pinned: the test must pass unchanged. Do not edit that test.
2. **North for the doubled leg** (K6 heads copy 0, K7 north pins) = the exit split node's row from rule 1 (the in edge's last positioned token's index); lane (copy 1) = bend1.index as in your patch. No new Step member; Over tokens keep the crossed edge in `index`.
3. Resume from rw1-attempt.patch; all identity checks again; gates; the one commit.
