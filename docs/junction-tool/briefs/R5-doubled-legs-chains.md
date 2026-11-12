# R5: doubled legs placed by their chains (lane pair, HeadsPair); the row blocks go (DESIGN-G1 §16 R5; RECON-row-chain §4 step 5)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-r5 <HEAD>` (given at launch). Never use git stash. Another agent may work in .claude/nodes2: never touch it.
- R2-R4 merged: in/out edges and undoubled legs are placed by their chains (their chain elements use the row_pts points as aliases, so point ids are unchanged); doubled legs still use the row blocks (lane split/merge, heads nh0/fh0/nh1/fh1 per bridge, the hug, the LaneMerge-PrioNode floor) and are skipped by refresh and the chain loop; their chains exist (copy 0: [Tile, HeadsPair x g, SBend(sb0_first, lane_merge), Tile]; copy 1: [Tile, SBend(lane_split, sb1_last), HeadsPair x g, Tile]) tied by `eq` (R2 check).
- This step: doubled legs' relations come from their chains (HeadsPair shape: copy 1 = copy 0 + u, the stagger; the lane pair's joins; series heads spacing from the floor table; the hug from the bridge's crossers in flat-solve order as T1c-2 does; the LaneMerge-PrioNode floor), the row blocks are removed, and the consumers that still read row_pts (occupancy enumeration, facts, sym_routes' doubled branch, the post-solve row setting, spans) read the chains. Point ids stay: the chain elements keep using the rp points as their points (aliases); row_pts stays only as that point storage.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; DESIGN-G1.md §16; RECON-row-chain.md §1 item 7, §4.
- src/junction_topology.cpp: the doubled-row block (lane split/merge, heads, hug, prio floor), the R2 chains and check, refresh/chain loop skips, occupancy/facts/sym_routes/spans/post-solve readers of row_pts, the two test members `row_chain_mismatches` and `row_change_keys`.

## Tests (record each red)
1. **Outer**: every existing test and sweep with identical numbers; circuit byte-identical. Red: removing the doubled row block before the chain relations are live breaks placement (record).
2. The R2 check and its two test members are removed (the chains are now the only geometry); the R2 tests that used them are replaced by "topology: doubled legs place by their chains" asserting, for each doubled edge on LL_RR at the test spec and across the sweep: copy 1 heads = copy 0 heads + u per bridge, lane split exactly 2 before the first near head (D215, already a test: keep it), every crosser tile between its bridge's heads.

## Changes
1. Doubled legs no longer skipped: their chains carry the relations named above; the doubled row block is removed entirely (and `doubled_row`'s special cases in collect_bridges/hugs where the chain now does it; say what remains and why).
2. Consumers read chains (occupancy enumeration, facts, sym_routes' doubled branch, spans, post-solve row setting).
3. Remove `row_chain_mismatches`, `row_change_keys` and the R2 check.
4. TESTS.md rows.

## Do not
- No program, render, signal or family rule change; no point renumbering.

## Stop conditions
- Any seed or sweep number changes; a doubled-leg feature the chain relations cannot express (report it); anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test` with `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` (diff against a launch-HEAD run: identical except timings); circuit byte-identical; `tools/junction/jt gate build-rel` (report before/after counts).
- One commit: `Codechange: Junction doubled legs placed by their chains; row blocks removed (R5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus what remains of row_pts and why, timing before/after (placeoracle micros). When you have reported, stop.
