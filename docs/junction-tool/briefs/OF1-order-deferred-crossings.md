# OF1: the order sweep places crossings between non-adjacent edges (RECON-nonadjacent-crossing.md rules A and B)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-of1 <HEAD>` (given at launch). Never use git stash. Other agents may work in /home/pete/OpenTTD/.claude/nodes2 and .claude/ng1: never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- OC1 (docs/junction-tool/RECON-nonadjacent-crossing.md, read it whole; experiment patch /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/oc1.patch, which has diagnostic fprintf lines and a hidden dump test you must not commit): OrderJunctionTrackGraph's sweep (src/junction_topology.cpp ~:562-614) expands the leftmost item first and keeps `pending` per band; LL_RR x LL_RR (pairing any) fails "a crossing between edges that are not adjacent is not ruled yet" because an edge of a pending crossing is expanded into its children before its partner appears; keep fails likewise and also needs pending crossings carried across bands. Measured fix: (A) defer expanding an item while a pending crossing names it with a partner that has not yet appeared as an item (fall back to leftmost when every candidate is deferred); (B) carry unswept crossings to later bands. With both, any and keep pass the order stage; `[topology]`/`[topologysweep]` passed. OC1 did not check A for deadlock on other topologies.

## Changes (src/junction_topology.cpp, OrderJunctionTrackGraph only)
1. Rule A and rule B as OC1 measured them, without the diagnostics; each with a comment citing OC1 and saying why it is general (the order of expansion follows the pending crossings, not the leftmost item).
2. A termination guard: if a full pass defers every item and nothing is placed, fail "the crossings cannot be ordered" (no infinite loop).

## Tests (record each red)
1. **Outer identity:** `tools/junction/jt identity build-rel` and build-rel outputs of the five sweep tags equal to launch HEAD's (times stripped): the T and LL_RR + sideline orders must not change (if any order changes, STOP and report which seed and how).
2. **`[topologysweep]`** (extend LP1's "LL_RR x LL_RR under each lane pairing seeds or names a reason"): under pairing any and keep, OrderJunctionTrackGraph succeeds (red at HEAD: the non-adjacent refusal); report the next stage's refusal for each (OC1 predicts any: "a row edge to a node the row has not made"; keep: "a branch crossing without one joiner").
3. **NM1 rerun (deadlock check):** apply nm1.patch (do NOT commit; extend locally with LP1's `NM1_PAIRING` env as LP1 did, or reuse LP1's local change if you can find it in the scratchpad lp1/ folder) and run all 243 pairs x 3 pairings one by one (shell loop with `timeout 60`): 0 crashes, 0 timeouts; report the stage/first-failure group counts per pairing vs LP1's (scratchpad lp1/nm1-{0,1,2}.tsv).
4. TESTS.md row.

## Stop conditions
- An existing order changes; a crash or timeout in the NM1 rerun; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5` (report the time; it was already 5.1-5.2 s: report, do not trim here); `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Change: Junction order sweep places crossings between edges that are not yet adjacent`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the NM1 counts per pairing before/after and the next refusal for LL_RR x LL_RR under each pairing. When you have reported, stop.
