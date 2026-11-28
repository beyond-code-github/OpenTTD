# probe-llrr3b: why are the 3-way LL_RR's two chains refused with no conflicts? (HEAD 5d9f97ca94, 2026-11-28)

Hidden test `[llrr3bprobe]` (reverted), Release, `-j 4`, variant 0, PathFinder, `pf_iterations` 0 (iteration 0 only, 7.6 s) and 1 for the iteration-1 lines. Edits as in `probe-llrr3` (lines only, fork reading, crossings-authored skip) plus prints; all reverted. Scratchpad: `llrr3/probe-src.diff`, `b/probe-b.diff` (the prints and the fix-b toggle), `b/base.err`, `b/it1.err`, `b/*.out`.

## 1. The fork reading's signs
Evidence: `b/base.err` lines `Q1 site ...` and `Q1 chain ...`.
- Every one of the 36 nodes has `site.straight` = none, in all four variants (variants 1-3 printed too). The topology has no prio and the tie bits that give a site a straight leg are not set by these variants, so there is no real straight-leg site in this topology to compare against (the brief's `ELFM1 -> EL1, FM1` is also a fork here).
- The reading takes out[0] of a split (in[0] of a merge) as straight and gives side +1 to the other leg. The legs are listed left to right (`ELFM1 -> EL1, FM1`: EL1 straight east, FM1 south, to the right), so out[1] / in[1] on side +1 is the same side the existing code gives a right-hand leg of a real site (`leg = straight == first ? -side : side`, +1 right of the straight leg's travel).
- Chain ends printed: leg +1 for every chain that starts at out[1] of a split (`FM1 F1 FjS`, `G1 ...`, `DK1 ...`, `L2`, `M1` ...) or ends at in[1] of a merge; -1 where the routed track is the in[0]/out[0] leg (`E1`: start -1 end -1; `B1`, `AG1 A1 AjS SO1` start -1; `CJDK1 CJ1 C1 CjW` end -1 as C1 is in[0] of `C1, C2 -> CjW`). Signs are consistent. Fix (a) is not needed.

## 2. Chain `AGBH1 BH1 H1 HjW WO2`
It is a port-to-port chain: one start point (the E side line, (160,71) in iteration 0, (240,71) in iteration 1), no node candidates, one attempt per iteration (`Q23 chain [AGBH1 ...] attempt 1 ... REFUSED`). Nothing on its trunk is a node: none of its own nodes (BH1, HjW ...) is located yet.
- The refusal: the dropped bridge is on the anchor chain `AGBH2 BH2 B2 BjW WO1`, pieces 76..79 (iteration 1: 116..119) of a chain whose real polyline is (120,69)..(0,69), 60 pieces. It lies beyond the anchor's last piece, on the anchor's port-stub ray (the ray the router sees east of x = 120 out to the box edge), tiles x 76..79 half-tile x 152..158 on row y 69 (heads 76 and 79, middles 77 and 78).
- Whose node: none. `ToHub` finds no run holding pieces 76..79 because they are not in the real chain, so `dropped` counts it, and `TryRoute` reports it with the "spans a node point" note. It is the crossing of another track (the anchor's ray), not a lift at the leg's first piece.
- Why the crossing is there: AGBH1 is left of AGBH2 at the E in port (south, y 71) and WO2 is right of WO1 at the W out port, so AGBH1 -> WO2 must cross AGBH2 -> WO1. The new chain starts at x 160, east of the anchor's end (x 120), and the cheapest place to cross is at once, over the ray. The ray is never kept (`Finish` sets the real stub), so a bridge on it refuses the candidate; there is no other attempt for a port-to-port chain and nothing to price.

## 3. Chain `CJDK1 CJ1 C1 CjW`
- Iteration 0: attempt 1 (goal (60,69), trunk AGBH2, arc 60 of 120, no other node on that trunk, -1) judges clean and places it. Not refused.
- Iteration 1: all 20 attempts (goals (60,69) .. (80,69), arc 40..78, no other located node on the trunk) route with 0 conflicts and every one is refused by the judge with the same line `domain: connection tracks ELFM1,EjE tile 33,31` (`b/it1.err`, 20 `Q23   judge:` lines).
- The two tracks that touch are ELFM1 (chain `ELFM1 EL1 L1 LjE EO2`) and EjE (chain `ELFM2 EL2 E2 EjE EO1`), neither is the leg's and neither is the trunk of its goal; they touch at tile (33,31), which is one of iteration 0's three overused tiles ((23,30) (32,31) (33,31)). The leg's node is on y 69, the touching tracks on y 61..63: the leg's node is not near either node involved (no node distance applies, 0 half tiles on the leg's trunk against another node: none exists).
- So the line is not about the leg at all: the contact was left in the state by two chains placed softly in iteration 0 (they cross at (33,31), the LL_RR's E out port swaps the lines ELFM1 -> EO2 and ELFM2 -> EO1). `PlaceSoft` judges the whole state with only the attempt's own blockers ripped, so a foreign standing contact fails every attempt of every chain placed while it stands, with 0 conflicts of the chain's own. The same holds for the iteration-0 unplaced K1 ("connection tracks EO1,EO2 tile 33,31") and L2 ("ELFM1,EO1 tile 33,31"): the line names the same (33,31).

## 4. The fixes, measured (iteration 0 only, variant 0)
| fix | unplaced chains | overused tiles | count reported (unplaced + conflicts + tiles) | time |
|---|---|---|---|---|
| none | 3 (AGBH1 BH1 H1 HjW WO2, K1 KjE, L2) | 3 ((23,30) (32,31) (33,31)) | 141 | 7.57 s |
| a. fork signs corrected | not run: question 1 found the signs right | | | |
| b. candidates at least `rules.node_leg` + 2 half tiles (4: node_leg 2 + one tile) from every other node point on the same trunk | 3 (same three; L2 now with 0 conflicts) | 3 (same tiles) | 101 | 7.63 s |
| c. both | same as b (a not applied) | | | |

Fix b does not place either named chain: it changes only L2's conflict count (40 -> 0) and the total (141 -> 101). The cause of the two named refusals is not node spacing: AGBH1 has no node candidates, and CJDK1's refusal names two tracks that are not its own.

## Cause and the smallest fix
Two separate causes, both in the builder, neither a price: (1) a port-to-port chain that must cross the anchor and starts on a side line beyond the anchor's end crosses the anchor's stub ray, and a bridge on a ray lies in no real piece, so the one attempt it has is refused ("a bridge ... spans a node point" is the wrong note for it); the smallest fix is to give such a chain a start (or the ray an end) that puts the crossing over the anchor's real polyline, e.g. clip the ray to the real end plus the stub the router needs, or let a bridge on a ray be moved onto the real piece by `Finish`. (2) `PlaceSoft` admits an attempt only when the whole state judges clean after ripping its own blockers, so a standing contact between two other chains (ELFM1 and EjE at (33,31), already counted as an overused tile) refuses every later chain with 0 own conflicts; the smallest fix is to judge the attempt against the state's own judge lines (accept when the attempt adds none that the state without it did not have), or to rip the chains named by the standing line. Neither was applied or measured here (the brief measured a and b only); fix b alone leaves both chains unplaced.
