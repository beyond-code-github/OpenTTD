# RECON P6: "corners ... cannot be joined by one terraform" in the LL_RR near search

Launch HEAD 8cff8a6fc6, worktree .claude/nodes2 (branch wt2-p6, nothing committed, clean). Release, 48 fixtures x 153 offsets (7344), PlanJunctionHover at every offset (no near reject). Patch: /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p6.patch (diag counters g_p6 in junction_levels.cpp/.h, hidden harness `[.p6]` "p6 experiment corner join", env `P6_REACH=1` = the fix below, `P6_VALIDATE=1`, `P6_DUMP=<prefix>`). All numbers MEASURED unless marked INFERRED.

## 1. Where the failure is certain: nowhere, it is the descent
- LL_RR: 502 offsets end "cannot be joined" (7.0 ms mean each); T: 157 (2.2 ms). The violation is already > 0 at the start levels in all of them (before any descent move), and the descent never clears it.
- Which corners (violation category at the end, LL_RR 502): a corner pushed outside the cone of the ground nothing can move ("base") in 502 of 502 (alone in 379); a corner whose clamp is overridden by another track corner's cone ("clamp") in 109; raised vs lowered pair rule in 15; span bound 0, tunnel 0, ramp rule 0, foundation/head 9. T 157: "base" alone in all 157. So: track-fixed corner against immovable ground, never a bridge span or tunnel bound.
- Why the solve gets there: the cone filter (LS1) computes, per tile, the interval `reach` of levels that keep its corners inside the base cone, tests that the intervals meet (RestrictRange), then `pos.Undo(cone_mark)` throws the intervals away. The start levels (cheapest flat level in the constraint-only range) and the descent range (lo..up from the difference constraints only) never see them, so the descent walks outside what the cone already excluded and ends in violation. The failure is the heuristic's, not a proof of infeasibility.

## 2. The fix, and a sound early check
- Fix (patch, 2 lines): keep the reach restrictions (no Undo of cone_mark) and clamp each group's descent range to pos.Lo/pos.Up of its members. Sound by construction: it only removes levels the cone test showed have no surviving option.
- Result over the 7344 offsets per topology: T planned 2919 -> 3064 (+145), corner failures 157 -> 1; LL_RR 1180 -> 1601 (+421), corner failures 502 -> 43. 0 plans lost; of the old plans 17 change cost (T 6: 3 cheaper, 3 dearer; LL_RR 11: 10 cheaper, 1 dearer). Every plan of both sweeps (T 3064, LL_RR 1601) re-placed, rendered, JunctionRoutesHeightsOk and CheckJunctionRules clean: 0 invalid.
- Early check as the brief asks (pair rule on the fixed-corner set: tiles the propagated level ranges force to one level, not rampable, not a head, whose best choice is not a foundation fix their four corners; two fixed corners differ by at most their Manhattan distance; sound because any valid terraform is 1-Lipschitz on the corner grid): rejects 4 of the 502 LL_RR failures (1 of 157 T), 0 of the offsets that plan, 0 under the fix too. Cost small (pair loop over a few hundred corners, not timed). Not worth it: forced single levels are rare before the descent.
- Nothing else sound and cheap exists before the descent: an early reject must keep the failures that a better solve would plan, and 459 of the 502 (91%) are exactly those (421 of them plan under the fix; the other 81 still fail: 43 as corner failures, 38 INFERRED with another reason).

## 3. Numbers (HOVERNEAR sweeps, same machine)
| | baseline | with the fix |
|---|---|---|
| HOVERNEAR_LLRR | planned 20 moved 13, mean 34.9, max 548 ms | planned 20 moved 11, mean 11.0, max 37.1 ms, 0 fixtures over 100 ms |
| HOVERPLAN_LLRR | planned 7, mean 2.6, max 8.7 ms | planned 9, mean 2.6, max 8.7 ms |
| HOVERNEAR (T) | planned 32 moved 13, mean 9.8, max 57.3 ms | planned 32 moved 13, mean 9.5, max 56.4 ms |
| HOVERPLAN (T) | planned 19, max 2.9 ms | planned 19, max 3.0 ms |
The near search stops at the first plan, and the fix makes plans appear at nearer offsets, so the three slow fixtures (arctic-hilly-13, temperate-mountainous-3, -5) stop doing 23-79 full placements.

## 4. Identity
The fix moves both identity hashes by rule (T planned 2919 -> 3064, LL_RR 1180 -> 1601; 0 lost, 17 cost changes, all valid), as P-steps that make the solve find more do. The pinned counts and hashes in the two LEVELHASH tests would be re-pinned, and the floors (HOVERPLAN_LLRR 7, HOVERNEAR_LLRR 20, T 19/32) still hold. INFERRED: the T identity-sweep cone_rejects (99) are unchanged (the cone test is untouched); not run. The early check was never switched on in a plan path, so no hash depends on it.

## 5. What remains
- The 43 LL_RR (1 T) failures that survive the fix (23 base, 14 clamp, 4 pair, 2 clamp+found): 6.8 ms each, 0.3 s over the sweep; INFERRED some are truly infeasible, not proven. No time-budget card is needed for LL_RR any more (max 37 ms); the near search of the engineer's GUI is within the 100 ms hover target on all 48 fixtures.
- Decision for the engineer: adopt the fix as a step (re-pin the two LEVELHASH identities by rule; validation above is done), rather than an early reject.
