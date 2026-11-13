# RECON P4: an incremental exact_cost for the level solve's descent

Recon by experiment at 819ea7fa36 (Release, `build-rel`, loaded machine, runs vary +-5%). Patch: `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p4.patch` (src/junction_levels.cpp only; `P4_CHECK` constexpr, true = assert every call against the full computation). The profiling instrumentation is `.../p4-profile.patch` (rdtsc ticks; applies to the clean launch file).

## 1. Profile of exact_cost after P3b (LL_RR hover sweeps, 79 676 calls, 253 level solves; measured, rdtsc)
Per call ~98 k cycles; 315 calls per solve; 223 group evaluations per solve (about 1.4 trials each). The descent is 81% of a solve, exact_cost 79% of it.
| Part (src/junction_levels.cpp, original lines) | share of exact_cost |
|---|---|
| raised-against-lowered pair loop (`q_raised` x `q_lowered`, ~800-830) | 41.3% (294 raised x 147 lowered = ~40 k pairs per call; the `max_raised - min_lowered >= 2` prefilter never skips) |
| cone propagation, both passes (~780-800) | 17.2% |
| touched-corner loop (h, violations, moves, dirty marking, ~830-865) | 13.4% |
| target collection incl. conflicts (~716-730) | 11.2% |
| foundation / cost loop over the n tiles (~870-905) | 6.3% |
| choices lookup n tiles (~706) | 5.3% |
| reset of the last evaluation (~690) | 4.5% |
| span bounds, span violation, ramp rule | 0.8% |
Per call: 657 touched corners (of 3321 cone corners), 774 target corners, 847 dirty tiles, 657 moved corners. A group change moves ~4 tiles.

## 2. The incremental cost (exact; measured)
State = exact_cost(I_lv), updated when one group's level changes (`inc_apply`): the dependants' choices (members, partner heads, tiles that exit into a member) are re-looked up; corners they touch get their effective target and conflict violation recomputed from their (at most four) contributors in tile order; the raised/lowered pair violation is updated by difference at the corners whose target changed (a Manhattan-radius scan, radius = max raised - min lowered - 1 from a histogram, or the opposite list if smaller); the L and U bounds are recomputed only inside the reach of the changed seeds; corner terms, moves, dirty counts, clear costs and the foundation tiles' cost/violation are updated by difference at the corners and tiles that changed. Spans and the ramp rule (0.8%) are recomputed in full.
Invariant that makes it exact: L (U) is the fixed point of max-propagation (seed - path length, every path corner above the ground), so it is the max over seeds of single-source values; a corner outside the multi-source reach of max(old, new) at the changed seeds has the same unchanged-seed contribution before and after; inside the reach it is recomputed from the new seeds and the unchanged bounds of the outside neighbours. Every other stored term is a function of inputs that are visited when they change.
Checked: with the assert on (full exact_cost at I_lv after every inc_apply: cost, violation) no mismatch over 131 669 applies (LL_RR hover sweeps), 29 655 (T), and the whole `[identity]` tag (7344 + 7344 offsets x 2 topologies). The state is built by the same code from empty (all tiles changed), so the start is checked too. If a tile has no choice at the start levels the descent falls back to full evaluation (did not occur: 240 of 240 descents incremental in LL_RR, 108 of 108 in T).

## 3. Numbers (Release)
- Hashes unchanged: `jt identity`: T LEVELHASH planned 2919 hash 16256945156918441187; LL_RR planned 1180 hash 11818222337964832513. Identity tag 60.7 s (was 73 s); LL_RR identity sweep 19.8 s (31.3 s). Wide gate green, 75 s, 483 cases.
- Descent: 8 081 -> 1 105 Mcyc (7.3x). exact_cost per applied trial ~13 k cycles instead of ~98 k. Whole level solve 9 935 -> 3 100 Mcyc (3.2x): per solve LL_RR planned ~9.5 -> ~3 ms, failing ~11.5 -> ~3.6 ms (INFERRED from the cycle ratio; HOVERPLAN_LLRR mean 4.4 -> 2.6 ms, max 25-29 -> 9 ms measured).
- HOVERPLAN T mean 1.60 -> 1.32 ms (max 4.2 -> 3.0). HOVERNEAR T mean ~17-19 ms, max 74-81 ms: unchanged (placement dominates; T has 108 level solves in the sweep).
- HOVERNEAR_LLRR: mean 131 -> 91-96 ms, max 1463 -> 560-630 ms (planned 20, moved 13 unchanged).
- Fixtures over 100 ms in LL_RR near (measured per fixture, 20 of 48): 16 have 0 level solves and fail in the placement stage (100-120 ms to try every offset of the near search, none succeeds); 4 have level solves: temperate-mountainous-5 575 ms (80 solves), -3 416 ms (68), arctic-hilly-13 154 ms (24), arctic-hilly-2 148 ms (11). Level solves are ~7 ms each there, placement included.

## 4. What remains and the next levers
- A solve is now 57% prelude, not descent (phase ticks, % of solve): the difference constraints and positions (`diff`/`AddDifference` plus corner_vars and the SLOPE_RANGE neighbour pass, ~385-585 of the original file) 24%; the constraint build before it 8%; tile maps 7%; start levels 7%; descent 35%; incremental init 3%. The next lever for solve time is the constraint build (a `std::map<MapCorner,...>` and per-pair `AddDifference` propagation): INFERRED 2-3x on the prelude by arrays instead of maps, not tried.
- For HOVERNEAR_LLRR the level solve is not the main cost: 16 of the 20 slow fixtures never reach it, and the 4 that do need 11-80 solves. A cap on level solves per near search (D197-style time budget or "at most N solves, then report no plan") would bound mnt-5/-3 at about 100 ms but can lose a plan that a later offset would have given: a decision for the engineer (card: cap vs budget; LL_RR has no 100 ms requirement per LR1). Failing-placement time (16 fixtures, ~100-120 ms) needs a placement-stage lever (P2-style sharing), outside this recon.
