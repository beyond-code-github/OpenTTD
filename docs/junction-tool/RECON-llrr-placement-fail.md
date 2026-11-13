# RECON P5: LL_RR near search, offsets that fail in the facts placement

Launch HEAD 7941320eb6 (branch wt2-p5, worktree .claude/nodes2, nothing committed). Release, 48 fixtures x 153 offsets (7344), the hover path (PlaceJunctionTrackGraph heights=true, terraform=true: so the facts block runs with classes_only, heights off), shared stages reused. Patch: /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p5.patch (library changes + hidden harness `[.p5]` "p5 experiment LL_RR placement failures" + env `P5_NOREJ=1` switches the reject off). All numbers below MEASURED unless marked INFERRED.

## 1. Where the failures are
- 7344 offsets: 1180 plan, 571 pass the X solve and fail later (502 "the corners the tracks fix cannot be joined by one terraform", 62 "tile", 7 "cannot reach the ground the terraform must keep"; 3-6 ms each), 5593 fail IN the X solve of the facts placement. Never in build() (0), never in the Y part (shared).
- The X-solve failures: the first reason is "blocked tile" in 5551 of the 5593 (the rest S-bend size / Corner size / chain run: blocked tile pushes the columns until a size rule contradicts). So: class facts only (no height facts, heights are off in this path), water/houses under a group tile or a row run.
- Cost per such failing offset (us, mean): setup 17, refresh 6, build 370-435 (of which facts block 195; the other ~245 is the offset-independent relations: 446 constraints, 121 points), X solve 230 (of which the offset-independent relation part ~145, the fact restrictions ~85, Explain text 3). ~0.65-0.70 ms total.
- Certain only at the solve: no fact is empty on its own (flat bound vs each group rejects 781 only); the contradiction needs the relations (longest paths) plus domain holes plus the row runs.

## 2. The early reject (soundness, rate, cost)
JunctionPlacementNearReject(shared, facts, offset), before PlanJunctionHoverAt for the near MOVES (the cursor spot is placed in full: its text is the plan's why). Shape recorded once in MakeJunctionPlacementShared from the same facts block over an all-buildable dummy (251 tile groups (point, column offset, row), 49 row runs, 446 relations, flat least columns).
Per offset, bounds propagation to a fixpoint: lo/hi per point; relations x_v - x_u >= k (lo_v >= lo_u+k, hi_u <= hi_v-k); each group: lo moves to the next buildable column, hi to the previous (c in 0..MaxX-offset.x, empty = reject); each run (west point w, east point e) with a blocked column c: lo_e >= c forces lo_w >= c+1, hi_w <= c forces hi_e <= c-1 (the AddAvoid rule not(x_w <= c <= x_e)); lo > hi anywhere = reject.
Sound: every step is implied by the constraints the solve adds (same relations, same domains, same avoids); the relations are offset-independent because build() reads neither facts nor offset before the facts block (grep: no use between the first line of build and the facts block); the box-bound relations added after the block are dropped (fewer constraints, still sound). Checked: over all 7344 offsets 0 rejects among the 1751 offsets that pass the X solve (1180 planned + 571 failed later), 5298 of the 5593 X-solve failures rejected (94.7%; flat bound alone 781, lower bounds with holes 4033). Cost 16 us per offset (prototype; 4 us without runs and upper bounds). INFERRED: the 295 left are contradictions that need the solver's deeper propagation; not chased.
Identity: PlanJunctionHover never calls it (so the hashed why texts are untouched); the near result is unchanged: same 20 planned / 13 moved lines for LL_RR and 32 / 13 for T (diff of the moved lines with and without the reject: identical).

## 3. Identity-preserving speed-ups (both in the patch)
- Group "blocked tile" notes formatted lazily (first four columns kept as ints, text built in the Explain loop): facts block 195 -> ~148 us (was 47 us of fmt).
- holds_on_row keyed by row only when classes_only (kind and axis are irrelevant there): ~17 us.
- Together: failing offset build 435 -> 370 us. `jt identity build-rel`: T hash 16256945156918441187 planned 2919 and LL_RR hash 11818222337964832513 planned 1180, both unchanged (59 s).
- Not done, MEASURED potential: caching the offset-independent X system (the relations: ~245 us of build, ~145 us of solve = ~390 of ~650 us) needs the reasons' indices and the Explain order kept (origin reason is pushed after the facts' reasons); INFERRED worth ~2-3 ms of the mean per fixture after the reject, small next to what remains.

## 4. Numbers (Release, best combination: reject + 2 speed-ups)
- HOVERNEAR_LLRR mean 92-96 -> 34.5-34.8 ms, max 569-574 -> 535-540 ms; fixtures over 100 ms 21 -> 3 (arctic-hilly-13 156, temperate-mountainous-3 426, -5 535; arctic-hilly-2 82 ms).
- T HOVERNEAR mean 18.9 -> 9.9 ms, max 74 -> 56 ms (same reject code). HOVERPLAN/_LLRR unchanged (2.7 / max 9 ms).
- Identity hashes unchanged (above).

## 5. What remains
The 3 fixtures over 100 ms do 23 / 70 / 79 full placements, every one past the X solve and failing in the level stage ("corners ... cannot be joined by one terraform" 502 offsets overall, ~6 ms each, 3-6 ms for the 62 "tile"), plus the 3.3-6 ms level solve of the plan found. An early reject for the corners-join failure (it names tracks fixed by the placement, so likely checkable on the X result before the render and the level solve) is the next P-step; the 295 unrejected X failures cost ~0.2 s over the sweep.
