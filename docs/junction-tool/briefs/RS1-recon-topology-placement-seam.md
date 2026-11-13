# RS1 (read-only recon): the seam between topology and placement inside junction_topology.cpp

Read-only. Write only the output doc. Keep your context small: use LSP or grep and read with offset/limit. Never read the 4,500-line file whole. Never touch worktrees under /home/pete/OpenTTD/.claude, game clients or the rig servers.

## State
- Repo /home/pete/OpenTTD, branch feature/junction-tool.
- src/junction/topology/junction_topology.cpp is about 4,500 lines. `PlaceTrackGraph` alone is about 2,200 lines.
- Horizontal placement (difference constraints, relation families, node positions, facts) currently lives in that file. placement/ holds the constraint solver (junction_positions) and the level solve (junction_levels).
- There is a folder cycle: topology.cpp calls levels.h and positions.h, and levels.h includes topology.h.
- The plan is to split the file so that topology/ ends at "graph + order + structure + programs", and placement/ owns horizontal placement. The orchestrator designs the interface; this recon supplies the facts it needs.
- Read src/junction/CLAUDE.md, topology/CLAUDE.md and placement/CLAUDE.md, plus docs/junction-tool/RECON-folder-layout.md §3, which names split points.

## Questions
1. A map of junction_topology.cpp: each top-level function and type, with its line range, size, the stage it belongs to (topology, graph, order, structure, programs, bridge cut, placement, render, signals, hover glue), and its callers. Do this as a table. For `PlaceTrackGraph`, break it into its internal phases (by comment headers or blocks), each with its line range and what it reads and writes.
2. The data that flows between the structure/programs stages and placement: the types, with each field listed and the stage that WRITES it and the stages that READ it, e.g. JunctionTrackGraph, programs, items, stack, bridge cut. Which fields does placement read? Which fields does placement write back into graph-side types? Those write-backs are what makes the cut hard.
3. What levels.h needs from topology.h: the exact types and fields. Could it be satisfied by a smaller header that both folders include, such as a placement-input type?
4. The render and signal stages: do they read placement output, graph types, or both? Where should they live after the split?
5. Two or three candidate cut lines, each with:
   - the files that would result and their approximate sizes;
   - the types that would move to a shared header, and where that header would live;
   - the remaining cross-folder includes;
   - the risks to the identity hashes. An identity step must leave LEVELHASH unchanged, so only code moves.

   Do not recommend one. List the facts only.

## Output
/home/pete/OpenTTD/docs/junction-tool/RECON-topology-placement-seam.md (untracked, about 150 lines).

## Report
10 lines: the function map in summary, the main write-backs, what levels.h needs, and the candidate cuts with their sizes. When you have reported, stop.
