# RW3: edge roles from the graph and tokens, not per-row arrays (DESIGN-G1 §16.4 RW3; RECON-per-row-code.md R1-R3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-rw3 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes: never touch it. Use absolute paths.
- RW1 (48edcd0adb: FindRowItems, node_item) and RW2 (5f6b386eed: S-bends sized by item rows on any edge) merged. This step is an identity refactor.
- Today: the program builder (R1 ~:725-768) counts exactly one in, one leg, one out edge per row and refuses otherwise ("not ruled yet"), marking `row_doubled` only on the leg; placement (R2 ~:983, ~:1012-1030) rebuilds `in_edge[r]`/`leg_edge[r]`/`out_edge[r]` arrays and a `doubled_row(r)` lambda (a bridge on the leg edge); R3 (~:1357-1362) `fan_item`/`edge_port` find the Fan by token position (first of an in edge, last of an out edge). Line numbers at 7f5bc41e06; re-locate them.

## Read first (once each)
CODINGSTYLE.md (C++ part); RECON-per-row-code.md §1 (R1-R12) and §4; src/junction_topology.h JunctionTrackPrograms (edges, copies, roles) and the graph edge kinds; src/junction_topology.cpp at R1-R3 and every reader of in_edge/leg_edge/out_edge/doubled_row.

## Changes (src/junction_topology.cpp)
1. **Per-edge role predicates** (file-local, in the anonymous namespace or as lambdas where the data lives; name them `IsInEdge`, `IsOutEdge`, `IsLegEdge`, `IsDoubledEdge`): in edge = its program holds a Fan token that is its first positioned token and the edge starts at a port; out edge = a Fan that is its last positioned token and the edge ends at a port; leg = a row edge between two row nodes; doubled = `!programs.copies[e].empty()`. Derive them from the graph and the program only (not from a row index). If one of them cannot be decided that way for an existing seed, stop and report.
2. **Readers use the predicates per edge.** Every reader of `doubled_row(r)` reads `IsDoubledEdge` of the row's leg; `fan_item`/`edge_port` find the Fan token by kind (the in edge's first Fan, the out edge's last), not by position. The `in_edge[r]`/`leg_edge[r]`/`out_edge[r]` arrays may remain only as a lookup filled from the predicates (one place), and the "one in, one leg, one out per row" refusal stays (multi-leg rows are not ruled: D-pending), now phrased from the predicates. If a reader needs the row arrays for something the predicates cannot answer, list it in the report (it is RW4's).
3. Comments move with the code.

## Tests
1. **Outer (identity):** build-rel outputs of `"[topologysweep]"` (incl. EDITSOUND), `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` equal to launch HEAD's with times stripped; LL_RR circuit byte-identical; `[topology]`, `[topoproj]` unchanged.
2. **`[topology]` "topology: edge roles come from the graph":** on T and LL_RR, for every edge, the predicates agree with the roles the old arrays give (write the old derivation in the test as the oracle: which row array held the edge). Red: the predicates do not exist (compile) — not a red; instead record the test's pass at the end and say it is a characterisation.

## Do not
- No chain-path unification (RW4), no row_pts change, no stack change (RW5), no prio change (RW6).

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; the identity diffs; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Codechange: Junction edge roles from the graph and programs, not per-row arrays`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the readers changed (file:line), what still needs the row arrays (for RW4), and the identity result. When you have reported, stop.
