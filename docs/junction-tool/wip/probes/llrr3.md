# probe-llrr3: can PathFinder build a fresh 3-way LL_RR? (HEAD 5d9f97ca94, 2026-11-28)

Hidden test `[llrr3probe]` (reverted), Release, `-j 4`, `budget_seconds` 240, present 0.5 x 1.5, one run per variant. Hub text: scratchpad `llrr3/llrr3.txt` (three LL_RR lines W, E, S, lane splits by direction then by exit lane, merges by line first, no prio).

## The temporary edits (all reverted)

1. Lines only (`planar_structure.cpp`): `bool g_probe_lines_only`; when set, every run's `copy` is cleared and `BuildPlanarTrackMap(..., rows = false)`.
2. Forks (`planar_build.cpp`, `FindChains`): a local helper `straight_of(si, node)` reads a site with `straight == NONE` as a split with `node.out[0]` straight (a merge with `node.in[0]`), and `side_of(si)` gives +1 for such a site. Used at the six places that read `v.sites[si].straight` (succ, pred, the two chain-end checks and the two `leg` lines). Structure and the sites are unchanged. It held: no fork refusal appeared in any run; chains were found for all 60 tracks.
3. Not in the brief, needed to get past the first refusal: `FindChains` refuses a map whose `expanded.crossings_authored` is false ("the map has no arrival side for crossing 0 (not authored)") before any chain is built; the topology has no `cross` statements, so the crossings are derived. The PathFinder never reads the crossing plan (it sets `cross_any` and breaks out of the plan loop), so the check was skipped under `options.pathfinder` (`if (!options.pathfinder && !crossings_authored)`). Nothing else read `b_from_left` on this path.

## Counts

- Parse: no error; 60 lines, 24 routes, 36 nodes, 24 crossings, 6 ports.
- Structure (lines only, keep 4): 4 variants, cost 21 each, 60 tracks, 24 crossings, 15 structures each; evaluated 40, not exhaustive, no skips. Tags o0t0f0m0, o0t1024f0m0, o0t128f0m0, o0t16f0m0 (they differ only in ties).

## Build: variants 0-3, PathFinder

| variant | wall | iterations | outcome |
|---|---|---|---|
| 0 | 240.2 s | 47 | not built: search budget (chain L2) |
| 1 | 240.5 s | 50 | not built: search budget (chain E1) |
| 2 | 238.1 s | 50 | not built: no convergence after 50 iterations, 37 overused tiles |
| 3 | 231.2 s | 50 | not built: no convergence after 50 iterations, 37 overused tiles |

About 4.5 s per iteration (variants 2 and 3 report 4.67 and 4.53 mean; 0 and 1 stop on budget so the mean is not set). The overused count per iteration is identical in all four variants (iteration 0 first):

141, then 21 for iterations 1-33, then 37 35 37 37 35 35 37 35 35 37 35 35 37 37 35 37.

It never falls: it is flat at 21 and then at 35-37, so the history does not separate anything.

## What the iterations show (variant 0 log, `llrr3/run-v0.txt`)

- Chain `AGBH1 BH1 H1 HjW WO2` is unplaced in every iteration (48 of 48): "a bridge of the route spans a node point (the router lifts the trunk over the node tile it shares with the leg's first piece)", 0 conflicts in its attempts. Its attempts are all refused by the router's own node rule, not by another track, so no price can move it.
- Chain `CJDK1 CJ1 C1 CjW` is unplaced in most iterations: "domain: connection tracks ELFM1,L2 tile ..." (a node tile shared with the straight leg's neighbour), with 0 conflicts in its attempts.
- Chains E1 and L2 (the first two lane-split legs on W's entry) are unplaced in the late iterations: "domain: head tracks E1,L1" / "E2,L2" at tiles x about 674-717, or "no route from (1436, 63) to (1416, 61) within 80 half tiles".
- The overused tiles drift east along y 30 one iteration at a time ((23,30), (54,30), (67,30), (79,30), (91,30) ... (448,30), then (1391,31) ...): a node point or stub that is pushed farther along the trunk each iteration. The box is growing to x 1400+ half tiles, i.e. the hub is not converging to a place, it is running away along the straight chain.
- Iteration 0 already leaves 141: 2 unplaced chains plus the conflicts of the others.

## Not reached

No hub was built, so there are no judge lines, box, travel or D530 measure, no `built.json`, and the port pull-in (brief step 5) was not run.

## What blocks, and what is left

The blocker is not the negotiation's price but the builder's placement of a fork-shaped (lane-split) chain: with no straight leg, the first leg is read as straight and the other leg leaves on side +1 at a node point placed on an axis piece of the trunk, and the router refuses a bridge that spans the node tile ("a bridge of the route spans a node point") and a leg's first piece that shares the node tile with a neighbouring chain's head. These refusals come with 0 conflicts, so soft pricing and history have nothing to price; two chains (AGBH1 ... WO2 and CJDK1 ... CjW) stay unplaced for the whole run, and the chains that do place keep pushing their node candidates along the trunk until the box runs away. The fork edit is a reading, not a fork in the builder: the builder has no node form with two sibling legs leaving at a tie (a fork leg is +1 side with a turn of one eighth, where a fork's two legs should split the turn), and the lane-split nodes here sit in series (a split of a split of a split, three deep on each entry). What is left: (1) a real fork node in the builder (both legs turned one eighth, either side of the trunk heading, as the drag's shape 2) and the candidates for a split whose trunk is itself a leg of a split one tile before it; (2) the router's "bridge spans a node point" and the shared-node-tile cases made soft or avoided at the leg's start (D529 touched the first head only); (3) nothing in the negotiation to try until the zero-conflict refusals are gone.

Files: scratchpad `llrr3/run-v0.txt` .. `run-v3.txt` (full logs), `llrr3/probe-src.diff` (the src edits, without the test).
