# LL_RR: decision cards for the engineer (2026-11-11)
Source: RECON-llrr.md (rules survey). Each card is one question the rules leave open. Recommendations are the orchestrator's.

## Card 1: first scope
Context: D60 allows an LL_RR main line with an L_R branch, or an LL_RR branch too (H36). No source has an SLH T design for LL_RR (RECON Q1). Example: Pete's LL_RR main line, an L_R sideline joining it.
Options: (a) LL_RR main line, L_R branch first, then LL_RR branch; (b) both at once.
Consequence: (a) needs one new pattern (H13 1-to-2 fan); (b) adds multi-to-multi balancing (H24) at the same time.
Recommendation: (a).
Prior art checked: H1, H12, H13 (hubs_1to2.png), H24, H36.
Reversible: yes.

## Card 2: which main-line lanes may exit to the branch
Context: R11 says "exits (splits) need no balancing", but D126(1) names "which lanes a splitting train can leave from" and D176 says each ML train has a line choice. Example: an S-bound train on the inner A lane: can it exit, or must it already be on the outer lane?
Options: (a) the exit splits from both lanes of the direction (two splits, each a whole-train leg to the branch merge, D176); (b) from the outer lane only (trains bound for the branch are expected on it, e.g. after a shifter before the hub); (c) from the outer lane, with a shifter inside the hub (MSH 01, H32).
Consequence: (a) more track, any lane works; (b) compact but trains on the inner lane miss the exit; (c) compact for the hub, a shifter's extra block upstream.
Recommendation: (a) for the first step; it needs no shifter and keeps every lane equal (R46).
Prior art checked: R11, R46, D126, D176, H32, hubs_split.png.
Reversible: yes.

## Card 3: doubling a main-line bridge over two lanes
Context: D80/D174 double every main-line bridge, written for one lane per direction. With LL_RR, B's two lanes cross over A->S and N->A. Example: does B get two bridges (one per lane), or four (each lane doubled)?
Options: (a) the two lanes are the doubling: one bridge per lane, two side by side; (b) each lane doubled: four bridges.
Consequence: (a) compact; a bridge is one long block per lane, so a lane's throughput drops on the bridge as D80 feared; (b) D80's reason holds per lane, but the crossing grows a lot.
Recommendation: ask; the orchestrator leans (a) only if D80's reason ("a bridge can't carry signals, so it is one long block") is weaker with two lanes sharing the load.
Prior art checked: D19, D80, D174, R48-R53, H27 ("doubled therefore have half value").
Reversible: yes (a map choice).

## Card 4: D113 against D176 after a line-choice split
Context: D113 wants a signal between a split and the next bridge head; D176 says that on multi-line the leftover after a line-choice split carries no signal unless it is whole bays. D176: "D113 vs this on multi-line: to settle with LL_RR".
Options: (a) on multi-line legs, no bridge head before the first whole bay ends (the leg starts with whole bays, then the bridge; D113's signal is the bay boundary); (b) D113 wins: the signal stays, and that leg's leftover must then be a whole bay too.
Consequence: (a) and (b) both end with whole bays before any bridge; (b) states it as a length rule on the stretch split -> head.
Recommendation: (b), as one length rule: on a multi-line leg, every signalled stretch after a line-choice split is whole bays.
Prior art checked: D83, D88, D113, D176.
Reversible: yes.

## Card 5: shifters
Context: D126(3) puts shifters in scope; no rule makes them mandatory for an LL_RR SLH (H9, H19 only for shift mainlines; MSH 01 "could be used").
Options: (a) no shifters in the first LL_RR step; (b) a shifter inside the hub.
Recommendation: (a); revisit with throughput (traffic model).
Prior art checked: H9, H19, H32, H38.
Reversible: yes.

## Card 6 (not LL_RR; from PERF2): searches that cannot succeed because of a rule, not a tile
Context: a house right beside the hub on the main line makes A detour so N->A can never merge; no tile blocks it, so the reachability test (GS1b) cannot see it, and the box grows to the snapshot's edge at every nearby spot: 39.8 s in Release before the tool gives up (PERF2 row e). The worker thread is busy that long; the ghost shows "working out".
Options: (a) a time budget per hover search (for example 300 ms): when it runs out, the tool says "no junction fits here: move the cursor"; honest, but a slow valid placement could be missed; (b) an exact rule-level stop: detect that an edge fails the same way at consecutive heights for reasons above the moving rows (needs a design and measurement; days); (c) both: the budget now, the exact stop later, the budget only as a backstop.
Recommendation: (c).
Prior art checked: D170a, GS1b (tile-level stop), PERF2.
Reversible: yes.
