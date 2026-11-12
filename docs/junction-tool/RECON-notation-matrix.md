# RECON: notation matrix (NM1), which notation pairs the pipeline builds (2026-11-13)

Reconstructed by the orchestrator from NM1's report (the agent's own copy was deleted by mistake before it was committed); raw per-pair results: /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/nm1.tsv; experiment patch: same folder, nm1.patch (hidden `[.nm1]` test).

Setup: wt-nm1 at 068a0b7b2b, Release, MakeTestJunctionSpec, flat ground. Main L, R in 1..3, gap 1..3; branch L, R in 1..3, gap 1: 243 pairs. Each pair: MakeJunctionHubTopology, the seed stages one by one, render, signals, CheckJunctionRules, then PlanJunctionHover on 5 real fixtures. Gaps 1..3 give identical results: 81 distinct cells.

## Clean: 6 pairs (2 cells x 3 gaps)
- L_R x L_R: 41x14, 1 ms. LL_RR x L_R: 62x25, 12-16 ms. All render, signal and pass the checker.

## Failures: 237 pairs
- **E (192 pairs, 79%):** "multi-to-multi movements are not ruled yet" (src/junction_topology.cpp:83). Lane pairing when both groups have >= 2 lanes (D186 deferred it). Hides everything downstream for those pairs. Needs a design rule (likely an engineer ruling).
- **G (30 pairs):** "more than two legs at one split" (24) / "at one merge" (6) (junction_topology.cpp:254): 3-lane mains and branches. Needs a rule for 3-way splits, tied to the doubled-leg model (D202).
- **C (6 pairs):** "placement rules contradict each other" (junction_topology.cpp:1389): unequal-group mains L_RR and LL_R with an L_R branch. Needs diagnosis (which rule, or a missing token).
- **Crash (9 pairs):** SIGSEGV in OrderJunctionTrackGraph (junction_topology.cpp:623, `order.rows[row]` out of range): main L_R with branch L_RR, LL_R or LL_RR. A bug: guard it (fail "not ruled yet"), then the lane-pairing rule.
- No failures at the order-later, structure, programs, placement, render, signal or checker stages: every seed that places also renders, signals and passes the checker.

## Not reached
Tunnels (D220): flat ground needs only bridges. Seed time <= 16 ms.

## Hover signal (weak)
Every 10th fixture: L_R plans 3/5 (max 14 ms), LL_RR 1/5 (max 94 ms). With the first 5 fixtures both 0/5.

## Suggested order
Guard the crash; design multi-to-multi lane pairing; 3-leg splits; the unequal-group contradiction.
