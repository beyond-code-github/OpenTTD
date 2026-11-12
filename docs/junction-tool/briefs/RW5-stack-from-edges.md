# RW5: the stack built from per-edge needs, not a per-row if-chain (DESIGN-G1 §16.4 RW5; RECON-per-row-code.md K1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-rw5 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes (src/junction_levels.cpp): never touch it. Use absolute paths.
- RW1-RW4b merged (row items from tokens, roles from predicates, one chain builder, points in chain order). K1 (StructureJunctionTrackGraph, ~:585-637 at 7f5bc41e06; re-locate): per row, joiners (>1 refused) and "crossed" decide prio/approach/copy per side, then a Kind by an if-chain (Lane, Copy, Approach, Prio, PrioAndCopy...). That is the last per-row-kind site in structure building.

## Read first (once each)
CODINGSTYLE.md (C++ part); RECON-per-row-code.md §1 K1 and §4 step 6; src/junction_topology.h JunctionTrackStackItem and its Kind; src/junction_topology.cpp StructureJunctionTrackGraph.

## Changes (src/junction_topology.cpp)
1. Each stack item is emitted from a per-edge need, named in a comment: a joiner edge into a row -> an Approach item on the joiner's side (and a Prio item where a joiner merge needs the prio, per the existing rule); a crossing that doubles a leg -> a Copy item on the side the copy goes; the row's own lane -> a Lane item. Items that today merge into one kind on one row side (PrioAndCopy) come from the two needs landing on the same side, by the same rule as today (keep it, say where).
2. The order of items within a row and across rows stays exactly as today; the "more than one joiner per row" refusal stays (phrased from the needs).
3. Comments move with the code.

## Tests
1. **Outer (identity):** `tools/junction/jt identity build-rel`; build-rel outputs of `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` equal to launch HEAD's with times stripped; LL_RR circuit byte-identical (`[.circuitllrr]`, JUNCTION_CIRCUIT, cmp); `[topology]`, `[topoproj]` unchanged.
2. A temporary assert that the new stack equals the old (kinds, rows, sides, order) for every seed in the sweeps; report it held; remove it with the old code.

## Stop conditions
- Any stack differs; a kind that no per-edge need explains (report the row and kind); anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Codechange: Junction stack items come from per-edge needs`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the need -> item table and the identity result. When you have reported, stop.
