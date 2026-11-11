# RN0c2: the flat T's paths as turn programs, and which lengths the rules fix (read-only recon for N0c2)

You do read-only research and write one file. Do not change code, build, or commit anything except that file. Keep your context small: read with offset/limit, report only conclusions. Mark anything not read directly from code, a decision or a dump as INFERRED.

## Goal
N0c2 will generate the L_R node graph from the rules instead of reading today's routed flat T (DESIGN-N3 §5, D189: topology = ports + movements; node order, sides, doubling, lengths, depth derived by rule; crossing assignment chosen, main line over first). To design it, the orchestrator needs today's T written as per-movement turn programs, with each length classified.

## Read (once each)
`docs/junction-tool/DESIGN-N3.md` §5; `docs/junction-tool/DESIGN-nodes.md` §4, §4a, §4b, §7; `docs/junction-tool/FLAT-T-CHAINS.md`; `docs/junction-tool/RECON-edge-rules.md` (§1-§3); `docs/junction-tool/DIAG-flat-speed.md` Q1; the flat T dump `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n3a/t2-dump.txt` section (a) (two characters per tile; same coordinates as FLAT-T-CHAINS); `src/junction_rules.h` / `.cpp` for JunctionCurveGaps and DeriveCurveSpacing (grep, then offset/limit); `src/junction_frame.h` or wherever MakeJunctionFrame is (port and hub positions); DECISIONS.md rows D80, D113, D115a, D117a, D148, D148a, D156, D172, D174 (grep by id).

## Write `docs/junction-tool/RECON-turn-programs.md`
1. Per path (the 7 of FLAT-T-CHAINS), its program as runs: direction (one of 8), length in pieces, kind (X run, Y run, diagonal, bridge span, port stub), and the nodes on it (split, merge, prio merge, bridge heads, crossings with which path). Tile coordinates of each run's ends.
2. Per diagonal: the hands of its two curves, its length in units, the rule minimum at the test spec (JunctionCurveGaps / DESIGN-nodes §4a: same-hand pair min(2 TL - 1, CL), opposite pair 2 when alone in a window), and whether it is at the minimum; if longer, what else fixes it (a column or row it must reach, a crossing, a bridge window), INFERRED where reasoned.
3. Per X/Y run: what bounds it from below (curve gaps, prio coverage 2p / 2p+1, bridge span limits, D172 window, D113 signal before a bridge head, switch-tile rules) with the decision or code line.
4. The geometric facts that fix the arrangement: the order of the vertical legs across x and why (port columns from the frame, which leg merges into which, which crossing is forced by port order), which crossings exist and who is over (bridge) in today's T, and which of these follow from ports + movements + node order (D189) versus a choice.
5. A table: for curve spacing CL at 100, 120, 140, 160, 180, 196 km/h (DIAG-flat-speed Q1: 2,3,4,5,6,7; TL 4), each diagonal's required length, and how many rows/columns each path's diagonals then add relative to the test spec.
6. Open questions for the orchestrator (numbered, one line each).
Keep it under 200 lines.

## Report
The file path, its line count, and the numbered open questions. When you have reported, stop.
