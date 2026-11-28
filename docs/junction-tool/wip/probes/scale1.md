# probe-scale1: does negotiation build large hubs? (HEAD 850fdf36fe, 2026-11-28)

Probe test appended temporarily to planar_length_build.cpp (reverted). Release build, budget_seconds 90, one run per row.

| hub | variant | negotiate | built | wall | attempts / rip-ups / soft | why |
|---|---|---|---|---|---|---|
| T1 | rigt1 | false | no | 92.5 s | 921 / 0 / 0, deepest level 4 of 7 | search budget |
| T1 | rigt1 | true | no | 97.0 s | 52 / 18 / 16 (soft routing 7.65 s of it, 37 calls) | 18 rounds, 18 rip-ups; queue left: stretch L9, stretch L3 L5 L13 L15, stretch L4, L12 L8, L6, L11 |
| T2 | 0-3 (4 variants listed; 0 run) | both | no | 0 s | 0 | map has copy or row tracks (28 lines, 4 rows, 0 copies) |
| T3 | 0 (variants 0-3: 2,2,1,2 copies, 6 rows each) | both | no | 0 s | 0 | same |
| LL2 | 0 (all 4: 5 copies, 0 rows) | both | no | 0 s | 0 | same |
| PSG 197 | hand structure, rows=false (101 line tracks, 0 copies, 0 rows) | both | no | 0 s | 0 | "chain D1 D12 D12b ends at a fork or a node without a site ... (version 1 has no fork)" |

Rows 2-4 are refusals at FindChains, not searches. T2/T3/LL2 variants 1-3 were not run (the refusal is per map, and all four of each carry copies or rows).

Answer key for PSG 197 (V's drawing, tools/junction/corpus/out/psg197): total travel 2207 half tiles over 101 lines (lengths.txt, sum of piece lengths); box from the pins' end and cross points 50.5 x 62 tiles (approximate: end vertices only, not every bend). No built hub to compare.

## Blockers
1. Copies and rows: `FindChains` (planar_build.cpp:307) refuses any non-Line track. The only no-rows option is `LoadPlanarStructureVariant(..., rows=false)` (D473). It exists for written-down structures only. `BuildPlanarStructure` (T2, T3, LL2) has no such parameter: it calls `BuildPlanarTrackMap` with the default rows=true (planar_structure.cpp:347), and copies come from the run's `copy` flag (a channel's line_share >= 1.0), so no text or spec change removes them. Dropping them needs rebuilding the map from runs, which BuildPlanarStructure does not hand out; the expanded topology also holds the copy/row nodes, so stripping tracks from a finished map is surgery on `src/junction` types. I did not try it, nor strip prio from the T2 text (that would be the workaround the brief forbids). Smallest unblock: a `rows`/`copies` flag on `BuildPlanarStructure` passed to `BuildPlanarTrackMap` (rows=false, run.copy=false).
2. PSG 197 with rows=false has no copies, but the builder has no forks ("straight X: none" / a node with no site; planar_build.cpp:442). Smallest unblock: builder support for a fork node (two legs, no straight).
3. T1 itself (the baseline) does not build in 90 s with either search; the existing negotiate test is `[!shouldfail]`.

## Last step lines
T1 default (last placed): chain 2 (L12 L8) placed at 85 half tiles; then budget: "search budget: 92.5 s, 921 attempts, 0 backtracks, deepest level 4 of 7".
T1 negotiate, the last six steps alternate (a rip-up ping-pong that history did not break in 18 rounds):
```
rip: stretch L2 of chain L2 L3 L5 L13 L15 for chain L9
rip: stretch L9 of chain L9 for chain L2 L3 L5 L13 L15
(repeated 3 times)
```
why: negotiation: 18 rounds, 18 rip-ups, 97.0 s; soft: route 7.65 s (37 calls), judge 0.00 s, other 0.00 s.

## Does negotiation scale?
Not on this evidence, and the test cannot reach the question yet. Of the five hubs only T1 runs the builder at all, and negotiation does not build it: it ping-pongs L9 against the long chain L2 L3 L5 L13 L15 for 18 rounds. The time is also off: 97 s for 18 rounds (past the 90 s budget), of which only 7.65 s is soft routing, so about 5 s a round goes into the hard stretch reroutes (the "prove no route" cost of FACTS M29). On a 15-track hub that is already the limit; PSG 197 has 101 tracks. The first things in the way, in order: copies and rows (T2, T3, LL2 and PSG 197 never reach the search; BuildPlanarStructure has no rows-off path), forks (PSG 197), and then the negotiation's own convergence and per-round cost on T1.
