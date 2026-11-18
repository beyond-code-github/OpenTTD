## Run 3 (S2c)
Commits on probe-s2c: A 64ce475b60, B e81145c17a. Baseline = wt-planar 20f98229e0 (pv-*.baseline, tag lists match 32/32/12).
- T2 and T3, A and B (W=3, W=10): all 32 variants of each fail with "base infeasible" (the composer's first solve), 0 built; "slot change inside one piece" is gone. With PLANAR_S2LOG on T2 o0t0f0s0 the composer's piece diffs remain (channel 7 core post lane 1 old 1 slot 3; channel 10 core pre/x/post lane 1 old 1 slot 2).
- LL2 (12 variants, all built in A, B3, B10): best 2230.25 vs baseline 2354.25; 12 better, 0 worse, 0 equal; median change -135.0 in all three runs; median solve 4.87 s (A), 4.93 s (W=3), 4.98 s (W=10) vs baseline 4.07 s. The width weight changes nothing measurable.

# PROBE-S2 (throwaway) measurements

## Run 2 (S1b, D351; probe-s2b = wt-planar 20f98229e0 + cherry-pick 37dc49d25d of a7e98b6ba1, clean)
- Fresh baselines at 20f98229e0: T2 28 ok of 32 (4 infeasible), T3 28 of 32, LL2 12 of 12 (best 2354.25).
- STOP CONDITION HIT: T2 and T3 all 32 variants fail with "slot change inside one piece" (no other reason; no "base infeasible", so the IIS step did not apply). Nothing reaches the solver.
- The failing case is the same in every variant (S2SPLIT log, checked on 6 T2 variants spanning o0/o1, t0-t3): channel 7, intervals x0 and mid0 share one segment; slots of channel 7's lanes (lines in Lanes order): x0 [1 2], mid0 [0 3]. Lane-1-to-lane-0... offsets differ: x0 gives 1 slot, mid0 gives 3. So the five "first cases" are one case repeated.
- LL2 (12 tags): 12 built and solved, but all 12 worse: best 2354.25 -> 3067.25 (o0t1f0s1); change +713 to +837, median +771.5; 0 better, 12 worse, 0 equal. Median solve 4074.5 ms baseline vs 4546.5 ms probe. 12 S2DIFF lines per variant (slot offsets differ from old constants on LL2, unlike run 1, where S1b's slots had agreed).
- jt fast not re-run (stopped at the stop condition).


Branch probe-s2 on a8b146d1ff (ng1 worktree). Baselines: pv run/save on wt-planar before the change (T2 28 ok of 32, T3 28 ok of 32, LL2 12 ok of 12 tags).

## Result
- T2: 0 of 32 variants solve (28 solved before). T3: 0 of 32 (28 before). All 32 build (PLANAR_DRY "built") and are reported "base infeasible" by the rough solve. No "slot change inside one piece" and no "a slot step on a diagonal piece is not composed" occurred anywhere.
- LL2 (12 default tags): 12 ok, areas and track identical to baseline in all 12 (best 2354.25 both; 0 better, 0 worse, 12 equal, median change 0); median solve 4099 ms baseline, 4204 ms probe (noise). No slot differed from the old constants in these tags (no S2DIFF lines).
- Best area: T2 baseline 862.25 vs none; T3 baseline 1933.25 vs none.
- `jt fast build-rel '[planar]'`: 23 cases, all pass (0.07 s). No failures to list (the T2/T3 area-pinning tests are not in that filter).

## Where slots differ from the old constants (every variant, both shapes)
| hub | channel | interval | lane | old | slot |
|---|---|---|---|---|---|
| T2, T3 | 10 (E core, prio join straight) | pre, x, post | 1 | 1 | 2 |
| T3 | 10 | pre, x, post | 2 | 3 | 4 |
| T2 | 7 | post | 1 | 1 | 2 |
| T3 | 7 | post | 2 | 3 | 4 |
Join out gap and fray gap: no difference (join out gap 1 slot; fray gap 2 slots exact).

## Cause (from PLANAR_IIS on T2 o1t3f1s0)
IIS rows: fray gap (4 half-tiles, now Eq), the fray's S-leg segments E1#0, E2#0..#4, and the core's `flex` row (offset 4). The slot layer keeps the line stationary with a gap of 2 slots from the prio fray's last interval into the core's `pre`. The composer's prio fray (`Fray`, R11, left unchanged) takes each near S lane through `Double(S[j], h, -sigma)`: the lane ends on the copy's line, 1 slot toward the far lane, with no back step. So after the fray the lanes are 4 - 2 = 2 half-tiles (1 slot) apart, and the core wants 4. The slot layer sees the copy as a track beside a stationary line; the fray leg moves the lane onto it. This is the question in DESIGN-tracks 15.4 (does the lane end on the copy's line, or the copy return to the lane): it matters, and S2 as briefed cannot compose it because a step across a site (here fray to core) is not in scope. The same applies at the join entry (JoinEntry offsets 0,1,3 vs slots 0,2,4 on channel 7 post in T3/T2).

## Failure cases
Slot-change-inside-one-piece: 0. Diagonal slot step: 0. Other new reason: none. Reason for all T2/T3 failures: "base infeasible" (old reason "infeasible" on the 4 baseline failures per hub).

## Probe code notes (deviations)
`SlotOf`, `IvName`, `Offs`, `LogDiff`, `SlotBend` and the `slots` member are methods of MappedComposer, not locals, because Bundle/Fray/Join need them. `Fray` gained a `gaps` parameter. `JoinEntry` is only used for the diff log now. The join out gap uses `2 * (slot(O[k+1]) - slot(O[k]))` in the out channel's pre (equal to 2*sigma*diff when lines are in sigma order). Lane order: `Lanes(c)` is left to right, the slots are left to right across travel, so the mapping is the identity. Bundle: m=0 channels use pre for pieces before the last and post for the last; core with no decks puts the pre-to-post bend at the end of its lead. pv and docs are untracked in ng1's tree; pv run from /home/pete/OpenTTD/tools/junction/pv.

## Run 3 IIS (probe-s2c e81145c17a, T2, PLANAR_ONLY=<tag> PLANAR_IIS=1 PLANAR_S2LOG=1, W unset)
Each variant solves two shapes (default, then program); each prints an IIS. S2DIFF lines are the same in every solve: channel 7 core post lane 1 old 1 slot 3; channel 10 core pre/x/post lane 1 old 1 slot 2.
- o0t0f0s0 (both shapes, same IIS, no fray in this variant): flex (E1#2.y vs E2#5.y = 4), seg E1#3, seg D1#5, tie@L1063 (E1#3 end = D1#5 end, the join's M.Tie), seg E2#6, seg E2''#0, cross E2''#1>D1#5 (x2 rows, v>=1). Class (c): the prio Join (planar_components.cpp:1046-1076; Double "''" at :1068/:1073, tie :1063, cross from the Double/Join) at E/D, against channel 10's core flex (Flex :1124, called from build_bundle :1424, offset 4 half-tiles = slot 2). D1#4 appears only as a free end y, no row of its own; no channel 7 SlotBend or deck row. Not fray, not channel 7 post.
- o1t3f1s0, default shape: fray gap (Eq 4 half-tiles, :1024, gaps from slots at :1356), seg E1#0, E2#0..#4, E1#1, flex (E1#1.y vs E2#4.y = 4). Class (a): prio Fray feeding channel 10's core, as in Run 1 (Fray's Double(S, h, -sigma, "'") :1033 ends the lane 1 slot toward the far lane, gap 4 then flex 4 leave nothing for the 2 half-tile move).
- o1t3f1s0, program shape: identical to the o0t0f0s0 IIS (flex, E1#3, D1#6, tie@L1063, E2#6, E2''#0, cross E2''#1>D1#6 x2): class (c), same as above.
So T2 is infeasible for two separate reasons: the Fray (default shape, f1 variants) and the prio Join with channel 10's core flex (program shape, and both shapes in o0). Both include channel 10's offset-4 flex row.
