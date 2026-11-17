# REVIEW-channels: where T3's jogs come from (code wt-planar 1abf2b91b1, docs 0b483956ca)

1. **Cause:** the model knows each channel's own width but nothing about the space between neighbouring channels. Ports on one side are pinned one tile apart, yet inside the hub a third channel's lanes run between them (a joiner landing, a fray leg leaving, the crossings between the main lines). That space must be wider inside than at the edge, only optional steps can open it, and the solver has to find out which exist.
2. **T3's 7 jog groups:** 2 required steps where a leg's gap closes (S and E exits: the joiner lands from the in port's side), 1 redundant (W exit: EF's fan already opens that gap), 1 genuine choice (B steps away from the median before its core), 3 on the wrong side or in the wrong direction (both of E's sides, B's north side).
3. **Proposed channel model:** extend D327's width profile from channels to the gaps between them. Each channel carries, per side and per stretch, its neighbour and the channels lying between; a step exists exactly where a gap pinned at the edge must hold such a band inside (required, at least a tile, on the channel whose site the band belongs to). A straight channel gets one candidate step before its core (B's jog), solved as a variant.
4. **Fit:** DESIGN-pieces §2 and §4 stand; §1, §3 and §6 must change (exit jogs are not join widths, fans carry a gap part, step directions come from where the neighbouring port lies).
5. **Confidence:** high for the three exits (the code's own condition; both bounds are tight in the dump), medium for why B's jog pays and for the side convention, low for LL² (read, not run).

**Terms.** *Leg*: the two ports on one side of the box (the E leg is ED out and AB in). *Gap*: the space across travel between two neighbouring channels. *Band*: lanes of a third channel lying in a gap. *Median*: the gap between the main lines E and B. *Close-up*: an out port's lanes stepping sideways after their join (the code's exit jog, `joinNj`). Positions are half-tiles across travel (south or east positive) from the dump's strand sums, EF1's port point = 0. Bare line numbers are planar_components.cpp.

## Q1. What makes jogs necessary

What the model has (verified):
- **Exact port rows:** a side's lines are 2 apart within a port, 4 between ports, as equalities (1552-1569, D313). E side: ED1, ED2, ED3, AB3 at b, b+2, b+4, b+8.
- **Sideways freedom** lives only in turning channels' diagonal pieces (`Bundle`, 1014-1050: two headings with free lengths absorb any offset) and in optional pieces: fans only to the left of travel (`Fray`, 892/897), close-ups only to the right (`Join`, 978), straight-channel jogs either way (`Flex`, 991-1008).
- **Straight channels have nothing else:** one heading makes a core (1198); the path step builds headings from route turns and crossing detours only (planar_paths.cpp:141-168, 247-279).
- **Each channel is placed on its own:** lanes at fixed offsets from its own first lane (1046-1049, 1003-1006, `JoinEntry` 1084-1089); between channels only clash separations (1477-1537).

**Missing:** what lies between two neighbouring channels, stretch by stretch. **Rigid:** the port rows and a straight channel's single heading. Where a band runs between two channels pinned close at the edge, their gap must open inside and close at the edge, and only a step can do that.

Bit order is creation order (planar_model.h:354-360): B SW, B NW, E NE, E SE, S exit, W exit, E exit; best 1000101.

**S exit (join3j): required.**
- A runs straight into AF; F lands from the west (planar_channels.cpp:240, side +1), the side of the neighbouring in port CD. After this non-prio join the out lanes stay on A's lines (986).
- F's landings are SE pieces of at least 6 (938; dump 3:6 on F1-F3), so F3 approaches at A1−10. The up-front side rule keeps F's last two pieces 2 east of every CD segment (1510-1537); CD cannot fan (plain fray, '-' fixed off: 1188, planar_model.h:353).
- With the row AF1 = CD3+8: AF1 ≤ A1−4, a close-up of at least 4. Dump AF1 `4:18 5:4 4:4`: exactly 4.
- Correction to the brief: the 96 combinations with the S or E exit off fail in 83-149 ms; the other 8 failures are slower (E SE below).

**E exit (join5j): required.**
- E runs straight; D lands from the south, AB's side (same condition). After the prio join the out lanes close up onto E1's line (968-972, 986); E1 = −14 (EF1's fan 14).
- D3 lands with 6 onto the merge at E1+8 (dump D3 `… 2:74 1:6`), approaching at E1+14 = 0. The side rule wants every AB segment, the stub on the edge included, 2 further south: AB3 ≥ 2, ED1 = AB3−8 ≥ −6, close-up ≥ 8.
- Dump ED1 `2:82 3:8 2:4`: exactly 8 (ED1 = −6, AB3 = +2). From the join's constants alone (landing ≥ 4) the bound is 6.

**W exit (join4j): never helps** (+57/+58 in all 12 measured pairs).
- C lands on B from the south, away from EF: no joiner band between CB and EF.
- The W leg's gap does open (F leaves EF's fray southward and crosses B), but EF's fan already opens it: it fans 14/12/10 where the spread alone needs 6/4/2 (907), so 8 of each fan is gap. A CB close-up is a second step for the same gap: two more bends per lane for nothing.

**The deciding condition is already in the code:** the side rule's test "a joiner arriving from the side of the neighbouring in port" (1518-1527) holds for S and E, fails for W. It is used only to forbid clashes, never to build the step it implies.

**B's jog (flex7j h5): a genuine choice** (−19 to −36).
- The main lines close a loop through two rigid rows: from EF1, E's fan −14, E's close-up +8, E row +8 to AB3, AB3's fan +2, B's jog +4 (core doubles net 0), W row −8 back to EF1. Sum 0 (verified: ED1 = −6, CB3 = AB3+6).
- Every other term is set by something else, so B's jog is the closing term. Off, the loop closes by a longer AB fan (moving A too), a shorter EF fan (narrower median at the W end) or a longer E close-up.
- Why it pays (INFERRED from the dump): in the core D crosses B northward and turns NE then E; D3's N piece ends 1 past B3's copy line (+6) and its NE piece is 5, so D's E run sits at 0. B3 is at +4 at the E end, +8 in the core. Without the jog, D's E run, and with it E and the E close-up, must move north. The jog moves B after A has left, which no other piece can do.

**The other three straight-channel sides: wrong side or direction.**
- B NW (+48..+55) narrows the median at the core; E NE (+19..+40) widens it at the E end, so the E close-up must come back further.
- E SE (+24/+25) makes the median shape of B's jog, but on E, whose last stretch is the straight prio run into the merge (934) beside D's band.
- Both E sides on fail in all 8 combinations after 0.86-1.10 s, not at the first solve. INFERRED: a clash the lazy loop cannot separate, as the lanes jog independently (995-1006).

**Not a width change:** E's and B's lanes leave their frays at 0, +2, +6 (minimum spread, then a doubled step back), exactly `JoinEntry` {0,1,3} lines, because each fray's far lane and its join's far lane share an edge. `Flex`'s per-lane freedom does no re-spacing on the T; its jogs only move gaps.

**The hunch, answered:** channels do change width along their length, and the code handles that (spreads, doubled copies, the join's close-up to its far lane: required or fixed '+'). What never changes size is the gap between neighbours: pinned at the edge, unstated inside.

## Q2. What a channel should be

The plainest extension of D327: one width profile across the hub, lanes and gaps, instead of one per channel.

**Data a channel carries**, beside lanes, crossings and path: per side, its stretches along the path, split at its own events (sites, crossings, port end). Each stretch records the neighbour (a channel, or the edge between two ports of one side), the bands between, and whether it is pinned (ends at the edge at the row spacing, D313). All of it comes from what planar_channels and planar_paths already hold: ring order, site sides, node leg sides, event order, over channel. No geometry.

**Where sideways steps come from**, these four only:
1. **Lane width changes**, as today (D327, D333): a prio fray's spread, a doubled stretch's copy line, a lane split, a join closing to its far lane. Required, displacements known per lane.
2. **Leg steps.** A band lies in a pinned gap when the in port's fray sends a leg toward the out port, or the out port's join takes a joiner from the in port's side. Then one required step of at least a tile (minimum from rule constants: S 4, E 6 on T3), on the channel of the site the band belongs to: a fray's band on its in port's fan (length = spread + gap, one piece), a join's band on its out port's close-up straight after the merge (D313, D317). T3: fan at W, close-ups at E and S, none at W: the measured state.
3. **Crossing detours**, as the path step makes them.
4. **Candidate steps:** a straight channel over crossings (a core) gets one candidate step before its core, away from the median. T3: B only (E crosses no channel; the north direction is not generated). Each candidate is a variant: two heading sequences, both solved (about 0.5 s together, DESIGN-pieces §8).

**B's jog:** the path step emits B as [W] in one variant and [W, SW, W] then the core in the other. The SW piece is required, at least 2 half-tiles (one tile across, even between parallel axis pieces, D334). The lanes step together, offset-parallel on every piece as in `Bundle` (1046-1049), so the lane-crossing clash that sank the switchless-diagonal experiment cannot happen. No switch, no zero-length piece; curve rows stay `PieceCurves` gap sums (planar_model.h:415) because every bend exists.

**Directions:** a leg step moves toward the neighbouring port (close-up) or away from it (fan), read from the ring, not from left or right of travel. `Fray` fans left (892) and `Join` closes up right (978): correct on both fixtures, where every out port's right faces its neighbouring in port and every in port's left faces away. A hub of the other hand gets both backwards (INFERRED); D313's "fans to its left" means "away from the neighbouring port" in that arrangement.

**In the code:** stretches in planar_channels beside the sites; planar_paths inserts steps before crossing pieces settle, so crossings see them; `Flex`, `Join`'s exit step, the fan's optional flag and `OPTMAX` go.

## Q3. Fit with DESIGN-pieces

- **§2** (required pieces, grid in the geometry layer) and **§4** (curve gap sums) fit unchanged.
- **§1** is contradicted twice. "5 exit jogs whose on/off follows from the joins' widths (a prio join is wider than its port)": the W join is prio and its close-up never helps; the S join is not prio and needs one; the prio join already closes up to its far lane inside itself (968-972). And the dump's 13 groups are 6 profile-fixed ('+'/'-') and 7 'j', not 7/5/1.
- **§3:** add a fourth source, gaps between neighbouring channels (Q2 source 2); "a join closing up" in source 2 should mean only the join's own close-up to its far lane.
- **§5:** B's jog stays explicit but generated by source 4, not listed per hub; the watch item ("always jog") becomes "is candidate 4 always taken".
- **§6:** "fans and close-ups are width changes" must split: a fan is spread plus gap, a close-up is gap only and exists only when source 2 says so. The '-' rule (a plain fray never fans) conflicts with source 2 when a plain fray's leg is the band: in T2 o0t2f1 C runs straight and D leaves the CD fray east toward AF, and the dump has AF's close-up instead. That needs a convention (D327's "which edge absorbs", still open). The up-front side rule (1510-1537) becomes redundant.
- **§7:** step 2 (heading-sequence builder) takes the stretches as input.

## Q4. LL²

Qualitatively yes, with one extension: LL²'s exits are one-lane joins per node (1360-1381, `merge{n}j`), so the neighbour across a pinned gap can be the other lane of the same out port. Reading `PlanarLL2Text` (planar_plan.cpp:438-475):
- **E leg:** EO2 = EDj2 + D2 with D2 prio, so D2 runs straight into EO2 from the south and, turning, is free across; AB's fray sends A south, away. Nothing between EO2 and AB: no leg step, unlike T3's E exit. Same rule, different topology.
- **EO1-EO2** (pinned adjacent lanes): EDj1 lands on E1 from the south and EDj2 on D2 from the north, both in that gap: steps required. D317 already describes this case in these terms ("where the other bundle's joiner still holds the slot, it steps beside the joiner"), its two-step form a variant by area.
- **W leg:** EF's fray sends F south toward WO; EF's fan takes the gap, as on T3.
- **S leg:** SO2 = AFj2 + F2 (F2 prio, straight): no joiner band. Whether the CD fray sends D toward SO depends on its tie bit, so the leg step appears in one variant only.
- **Lane splits:** which leg diverges is a tie bit (D330 rule 5); its step becomes a required width change, replacing `Fork`'s two free optional diagonals (813-823; own binaries, no group set, planar_model.h:367-372).

No per-hub rule; LL²'s variant count is set by its tie bits (up to 9), not by steps. All of Q4 is INFERRED.

## Checks to run first

1. Unit test for source 2: T3 o0t1f1 → W fan, E and S close-ups; T2 o0t2f1 → S leg from the CD fray; LL² per node as in Q4.
2. T3 o0t1f1 with S and E close-ups required, no W close-up, no E flex, B's jog as two variants: expect 1941.25 and 1960.25, each near the fixed-switch 0.27 s.
3. T2 o0t2f1 with the S leg's step on CD's fan instead of AF's close-up, against 906.25: settles the side convention.

**Verified against inferred.** Read from code and dumps: the exit condition, both tight bounds, the loop sum, the fray-exit/`JoinEntry` match, every cost range. Inferred: why B's jog pays, the E-both-on failure, the direction risk, all of Q4.
