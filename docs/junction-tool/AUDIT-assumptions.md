# Audit: hand-coded assumptions in the generator and checker

Brief: `briefs/AUDIT-assumptions.md`. Head e5cbb311ae, read as the working tree stood on 2026-11-08, while another agent was editing `junction_rules.cpp` and `junction_generator.cpp`, so line numbers may be off by a few. Read-only; nothing was built.

**Provenance is graded strictly.**
- **GAME** means an OpenTTD mechanic.
- **COOP** means a sourced R-rule, or a D-ruling that quotes the engineer.
- **OURS** is everything else: orchestrator calls (even recorded ones such as D85, D88, D90, D99a and D103(1)), plan-document designs (PLAN-1b §6) and unexplained constants.
- Where one line mixes provenances, the row says which part is which.

Abbreviations: gen = `src/junction_generator.cpp`, rt = `src/junction_router.cpp`, route = `src/junction_route.cpp`, sp = `src/junction_signal_pass.cpp`, rules = `src/junction_rules.cpp`.

## 1. Generator skeleton

| # | Location | What it fixes or forbids | Provenance | Effect on footprint or throughput | Proposed replacement |
|---|---|---|---|---|---|
| S1 | gen:214 | Rejects H unless `yb ≥ g + 4` | OURS | None on its own; it sits below the real minimum that STUB sets | keep: performance only |
| S2 | gen:215 | W track on row `g+1` (ML gap from the notation) | COOP (R41, D22) | — | — |
| S3 | gen:216, 479-480 | Lane B sits on the row right outside the W track (`b_row = g+2`): the outer side, with no gap between lanes | OURS (D85, orchestrator; D105 says "the shape is free") | Fixes the doubled section's shape and side; rules out the inner-side and spaced variants | replace with a checker rule (G1 equal length plus balance) plus a sweep of lane side and offset |
| S4 | gen:221 | Search box `min = (1, 0)`: no route goes north of the E track or reaches x = 0 | OURS | Probably none for a T, but it is unproven | keep: performance only. Note: a north loop is never tried |
| S5 | gen:223 | `max_expansions = 50000·width/64` | OURS | A route that runs out of expansions drops the candidate without a word | keep: performance only. Count and log the drops |
| S6 | gen:224, gen:80-90 | `curve_spacing = DeriveCurveSpacing` (s from 1 to 12, capped at 2·TL units); 1 when `curve_limit` is false | GAME (`train_cmd.cpp:321` `GetCurveSpeedLimit`, D67) | — | — |
| S7 | gen:225 | `s_bend_window = 2·TL` units | GAME (the `curvecount 1/1` exemption; D67, engineer: "one of them per TL") | — | — |
| S8 | gen:227 | `max_bridge_middle = 10` | OURS | Caps the span on top of `bridge_span_ok` | remove; `bridge_span_ok` (GAME: bridge type and `max_bridge_length`) covers it |
| S9 | gen:230 | `turn_cost = rail_piece/2` on every L or R | OURS (D3b tie-break; not a game price) | Biases routes against curves, so compact curvy routes can lose to longer straight ones | keep only as a tie-break after real cost, or remove; the score already picks among clean candidates |
| S10 | gen:278, 702 | Pass-1 crossing cost ∈ {0, `bridge(3)`} | OURS (`bridge(3)` is arbitrary) | Decides where branches cross the W row, and so how long the doubled section is | replace with the doubled section's real priced estimate, or a wider sweep. The checker and score decide |
| S11 | gen:235 | `STUB = 3`: straight 3-tile stubs on both branch legs | OURS (D90(4)) | Forces at least 3 straight rows at the branch edge; D58 only needs 1 tile beyond the outermost switch or signal. **About 2 rows of height** | replace with checker rule G10 (D58) plus a stub sweep {1, 2, 3} |
| S12 | gen:245-249, 290, 306 | W→S may merge, and N→W split, only on stub side pieces | OURS (D90(4)) | Pins the branch switches into the bottom rows | remove with S11. Goals become any plain piece of the S or N branch track; G4, G8 and G10 judge |
| S13 | gen:251-310 | Two passes: the W row is crossable straight track in pass 1; doubled lanes are fixed afterwards and everything is re-routed | OURS (D87) | Pass 1 chooses crossings without knowing what the doubled section costs | keep: performance for now. Later, search jointly or iterate pass 2 |
| S14 | gen:262, 266-309 | Greedy fixed order E→S, W→S, N→E, N→W | OURS (D90(1)) | A cheap early route can block a smaller overall layout | replace with a checker rule plus a sweep over the planar-valid orders |
| S15 | gen:270 | Column fence: E→S may not touch column xI on rows 0..g+3 | OURS (D90(3)) | Fixes the crossover: E→S always passes low and N→E always bridges it, which adds rows | remove. Sweep the crossover topology (PLAN-1b X1/X2); G2 and G5 judge |
| S16 | gen:272 | Fence on the tile above the N stub (`ys−1`) | OURS (4989ade9a9 patch) | A reserved tile | remove with S11 and S15 |
| S17 | gen:274 | Branch fence: the ML gap rows `1..g` are closed to W→S and N→W | OURS (D90(3)); contradicts D109 ("router stays free to run a branch through the ML gap") | Forbids using the gap, so routes go around it | remove; the checker already covers it via G2, G5 and G9 |
| S18 | gen:273, 477-481 | Under-span fence: W→S and N→W may not pass under ML bridges | OURS (D90(3)); D61 (engineer: "Allowed") lets connections pass under bridges | Forbids the most compact crossings for those two movements | remove; the checker already covers it via G2 and G5 |
| S19 | gen:279 | E→S leaves the E row only at x ∈ [2, xI−2] | OURS (D90(2)) | Holds the E→S split at least 2 tiles west of the N column | remove; the checker already covers it via G4, G8 and G9c |
| S20 | gen:237, 282 | E→S's goal is fixed: the top of the S stub `(xO, ys)` | OURS (D90(4)) | Fixes where E→S joins | remove with S11 and S12 |
| S21 | gen:288 | W→S leaves the W row only at x ∈ [xO+2, width−3] | OURS (D90(2)) | Holds the W split at least 2 tiles east of xO | remove; the checker already covers it via G4, G7 and G9c |
| S22 | gen:289 | W→S may also start on lane B (the D85 arrangement) | OURS (D85); **the engineer ruled against it in D110(3)** ("Yes the SL splits first") | Lets adjacent or duplicate splits through | remove (queued as D110(3)); add the missing checker rule for adjacent splits |
| S23 | gen:486-493 | `lane_b_max_x = lane B's first axis x − TL` ("clear of the S-bend's turns, G3") | OURS (a G3 hedge) | Pushes W→S at least a TL downstream | remove; the checker already covers it via G3 |
| S24 | gen:298-300 | The N→E merge window starts at `x_s + TL·prio_tl + 1 + slack` | The coverage part is COOP (D108, "The split is inside the prio!"); the `+1` is OURS (left over from D108's `+sd+1`) | 1 tile of width | remove the `+1`; G9c covers it. Keep the coverage bound: performance only |
| S25 | gen:300 | The N→E window also starts at xO or later | OURS (D90(2)) | N→E can't join between the branch columns | remove; G2, G4 and G9 judge |
| S26 | gen:288, 300 | Goal and start windows end at `width−3` | OURS (frame margin) | None inside the 64-wide frame | keep: performance only |
| S27 | gen:300, 325, 703 | `joiner_slack` sweep 0..3 | OURS | Adds tiles between the joiner and the switch upstream of it, to make up for the windows | remove once the windows go; router cost plus the checker choose |
| S28 | gen:310 | Pass-1 N→W window on the W row: [2, xI−2] | OURS (D90(2)) | Holds N→W at least 2 tiles west of xI | remove; G4 and G9 judge |
| S29 | gen:312-314 | One crossing zone: a single doubled span from min(C) − margin to max(C) + margin | OURS (D109 clusters not yet built) | One long span over every crossing, so long bridges. D103: a long bridge is a long block | replace: D109 clusters plus a min-gap sweep (queued) |
| S30 | gen:704 | `bridge_margin` ∈ {1, 2, 3} | OURS (D103(1), orchestrator) | Already a sweep. The top value of 3 is arbitrary | keep (a sweep); D103(2) headway should bound it later |
| S31 | gen:316-317 | Span accepted by `bridge_span_ok`; heads inside the frame | GAME / frame | — | — |
| S32 | gen:324 | In style 1, N→W joins the B row | COOP (D105 parallelogram) | — | — |
| S33 | gen:325 | `nw_max = min(xI−2, merge − TL·prio_tl − slack)` | The coverage part is COOP (D108); `xI−2` is OURS (D90(2)) | `xI−2` holds the N→W merge 2 tiles west of the N column | remove `xI−2`; keep the coverage bound: performance only |
| S34 | gen:333-352 | Lane A in style 1 is `F^k B_n [F] RFL`, gap ∈ {0, 1}, **first one found wins** | The shape is OURS (D105: "the shape is free"); the loop order is OURS | If gap 0 rasterises but fails the checker, gap 1 is never tried, so the candidate dies | make both gaps separate candidates; the checker decides |
| S35 | gen:356 | `x_sb = nw_max_x − 6` (W shift-back S-bend position) | OURS (D110(4) says replace it; queued) | About 6 tiles of width in style 1 | replace with a tightest-first sweep; the checker decides (G3, G9b, with D111) |
| S36 | gen:357 | Rejects when `x_sb < 2` | OURS (frame) | — | keep |
| S37 | gen:362-375 | Lane B in style 0 tries `want_axis` ∈ {0, 1}; first one found wins; **`q` starts at 1** | OURS. **Suspected defect:** after `RFL`, L leaves an axis piece and each of the q ≥ 1 F's adds a plain axis piece, so `want_axis = 0` can never match. D110(2)'s "zero first" may be dead for lane B (verify with a unit test) | 1 tile per lane at the bridge entry | start q at 0, and make both variants candidates |
| S38 | gen:396 | `r = 1`: exactly one plain tile after the far head before LFR (style 0) | OURS (the G5 straight-approach rule that D110 relaxed in the checker) | 1 tile per lane at the bridge exit | sweep r ∈ {0, 1}; G5 (relaxed) and G3 judge |
| S39 | gen:365, 379, 385 | Fixed lane programs: one-row RFL/LFR S-bends only | OURS (D100/D105 give balance by construction; the shape is ours) | Rules out wider shifts and other balanced shapes | keep for now (balance by construction). Later: routed lanes under an equal-cost rule (D105 "Later") |
| S40 | gen:416-426 | Style-0 lane A is the mirror of lane B | COOP (D100: "the symmetry is the thing that balances") | — | — |
| S41 | gen:439, 464 | Equal raster units; both lanes merge on one tile | COOP (R33, R53, D100) | — | — |
| S42 | gen:495 | In style 1, `nw_min_x = x_sb + 3` | OURS | Holds N→W 3 tiles past the shift-back | remove; G3, G9 and G9b judge |
| S43 | gen:498-517 | The ML rows are cropped to content ±2, before signals and prios exist | OURS; D58 (engineer) says one tile beyond the outermost split, merge or prio signal | Up to 2 columns of width | crop after the signal pass, to D58's rule; G10 checks it |
| S44 | gen:682 | `ml_gap == 0` → NeedsMlGap | OURS (follows from straight prio rows and the style-0 gap row) | Refuses L R outright | ask the engineer; revisit after D111 (prios may bend) |
| S45 | gen:695-696, 755-756 | H swept from `g+5` to `g+20` | OURS | The lower bound is tied to STUB; the upper bound caps the search | keep: performance only. Re-derive once S11 changes |
| S46 | gen:742 | Early stop 2 heights after the first clean height | OURS | A cheaper, taller candidate could be missed (the score is cost first) | keep: performance only. Note it in the score docs |
| S47 | gen:705 | `lane_style` ∈ {0, 1} | COOP (D105: "nothing in the rules says bridges have to be in this arrangement") | — | — |
| S48 | gen:641-643 | Terrain planes from a hard-coded box x 26..34, y −4..4; at most 3 planes | OURS. **Defect:** the box assumes `hub_x = 30`, but `MakeJunctionFrame` moves `hub_x` by `TL·(prio_tl−1)` | At prio_tl > 1 the planes are sampled off-hub | derive the box from `frame.hub_x`; keep "top 3": performance only |
| S49 | gen:629-636, 735 | Selection order: cost, then −(largest open rectangle), then area | Cost first is COOP (D76: "Cost is the primary driver"); the open-rectangle measure (largest empty rectangle, maximised) is OURS; area only breaks ties | Footprint is the third key, so the smallest layout wins only on a tie | ask the engineer (see §5) |
| S50 | gen:633-634 | Cost = rail_piece per track bit + signal + `bridge(n)` (+ terrain) | GAME (unit prices) | — | — |

## 2. Router

| # | Location | What it fixes or forbids | Provenance | Effect on footprint or throughput | Proposed replacement |
|---|---|---|---|---|---|
| R1 | rt:224 | Any turn is allowed once `u ≥ curve_spacing` | GAME (D67) | — | — |
| R2 | rt:225-227 | A tight opposite-hand pair (S-bend) is allowed only if the previous turn was at least `s_bend_window` back | GAME (`curvecount 1/1`) | — | — |
| R3 | rt:223, 225 | After a tight pair, no turn until `u ≥ 2·TL`; any other turn within `curve_spacing` is forbidden | OURS: stricter than the game, whose penalty uses the average spacing over the train (a third bend can pass) | Rules out some compact bend sequences that G3 would pass | replace with checker rule G3 plus a sweep: track the last 2·TL units of bends in the node and apply the real formula, or relax and let G3 reject |
| R4 | rt:261, 345 | Units: axis piece 2, half piece 1, bridge 2 per tile | GAME (D36) | — | — |
| R5 | rt:291-292 | Only F after a far head | OURS (the old G5; D110 now lets a curve meet a head) | 1 tile at every bridge exit | remove; the checker already covers it via G5 |
| R6 | rt:324; route:103 | A bridge starts only from an axis piece, so the approach tile before the near head is always straight | The head axis is GAME; insisting on a straight approach piece is OURS since D110 | 1 tile at every bridge entry | allow B from a half piece whose exit edge is the head's edge; G5 covers it |
| R7 | rt:329-339 | Heads on free, usable tiles that aren't crossable; middle tiles not under another bridge | GAME (`tunnelbridge_cmd.cpp:507-509`, "Disallow crossing bridges") plus OURS (no head on a crossable W-row tile) | — | keep |
| R8 | rt:326-327 | B<n> for n = 1..`max_bridge_middle`, filtered by `bridge_span_ok` | GAME (see S8 for the cap) | — | — |
| R9 | rt:297-300 | Step cost = rail_piece + terrain + crossable cost + turn cost | Prices are GAME; crossable cost and turn cost are OURS (S9, S10) | — | see S9, S10 |
| R10 | rt:306-321 | A piece that clashes with existing track is allowed only on a goal tile, followed at once by a goal piece (the merge grammar) | OURS (structural) | Merges happen only where the generator's goal windows allow | keep (structural). It loosens when the windows go |
| R11 | rt:206-211 | Only parallel half-piece pairs share a tile; no X+Y crossing | GAME (track combinations) plus COOP (no level crossing, R58) | — | — |
| R12 | rt:420 | A self-crossing best route returns `nullopt` instead of searching on | OURS (defect) | Drops routes the search could still find | continue the search (skip that goal node) |
| R13 | rt:284 | Heuristic: Manhattan distance × rail_piece | OURS. Admissible only if `bridge(n) ≥ (n+2)·rail_piece` | With cheap bridges the "cheapest route" may not be the cheapest | keep: performance only. Assert admissibility from the prices |
| R14 | rt:95-98, 424 | Grid padding of 12; the expansion cap returns `nullopt` | OURS | See S5 | keep: performance only |

## 3. Signal pass

| # | Location | What it fixes or forbids | Provenance | Effect on footprint or throughput | Proposed replacement |
|---|---|---|---|---|---|
| P1 | sp:38-48 | Paths walked mainline first | OURS | Decides which shared signal gets placed first | keep |
| P2 | sp:81-84 | A signal on the **nearest** legal piece before every split and merge, and after every merge | OURS (PLAN-1b §6 "switch isolation"; the D71 rule only needs no block holding both a split and a merge, which is G8) | Packs signals at switches. It caused the D107/D108 leaks and limits where joins can go | ask the engineer |
| P3 | sp:86-93 | A signal before the near head and after the far head of every bridge | COOP (D82: "there's no signal before the bridge on the ML") | — | — |
| P4 | sp:96-100 | The joiner's signal before an ML merge is Entry | COOP (D74) | — | — |
| P5 | sp:107 | Bay = 2·TL units | COOP (D83: "If the waiting bays aren't whole trains then this will block") | — | — |
| P6 | sp:131-148 | A bay boundary is the first legal piece at least a bay back, never a bridge, head or switch | OURS (D88, orchestrator) | Bays may come out longer than a TL | keep (it serves D83's purpose) |
| P7 | sp:162, internal.h:21 | Fill density SD·192; weights 192 (axis) and 128 (half) | GAME (`rail_cmd.cpp:1277` `CmdSignalTrackHelper`; D97, engineer) | — | — |
| P8 | sp:166-200 | Fill is "dragged" upstream from each downstream anchor | OURS (D97, orchestrator's part) | The short block falls at the upstream end | keep |
| P9 | sp:170 | Never a signal on a path's first or last piece | COOP (D58 boundary, via G10) | — | — |
| P10 | sp:523-560 | Inside the coverage, adjacent ML signals lose the fill signal (the upstream one if both are fill) | The rule is COOP (D107(2)); the tie-break is OURS | — | keep |
| P11 | sp:492-517 | A one-tile block on one lane only, inside the coverage, gets an extra signal | OURS (D2 ruling, "to show the engineer") | Three signals in a row on lane B | remove if G9 and G9b pass without it; otherwise ask the engineer |
| P12 | sp:445-486 | A prio join is reserved in the lane-merge block before the required signals go in | OURS (mechanism) | — | keep |
| P13 | sp:266-291 | The prio goes on the side opposite the joiner's approach | COOP (D79 puts the first join on the merge tile, which forces it; D111: "offset to the side opposite the joiner"). GENERATOR-1b step 6's "both prio sides (D70)" is stale | — | — (fix the doc) |
| P14 | sp:331, 464, 503, 542 | Coverage = 2·TL·prio_tl units, where prio_tl is a whole number | OURS (a stopgap). D29 (engineer): "take the shortest mathematical answer"; D31 and D102 want the train model (M4) | Prios come in whole-TL steps, so they are often too long: width and ML spacing | replace with D102's L (M4) |
| P15 | sp:336-337 | Coverage reaches one segment further, where a held train stands | COOP (D107, D108) | — | — |
| P16 | sp:375-379 | Every join on the merge's own row (a straight prio) | OURS; **D111 overturns it** (engineer: "nothing says that a prio can't have a curve or an s-bend in it") | Forces every ML bend outside the coverage; that is what drives S35 and S42 | replace with D111's path-following prio (queued) |
| P17 | sp:404-407 | Two adjacent joins abandon the prio | GAME (the prio signal needs a plain row tile between joins; join tiles hold overlapping tracks, which `rail_cmd.cpp:1089` rejects) | — | — |
| P18 | sp:346-357 | Each block's join goes on the first suitable piece from the downstream end | OURS | Minor | keep |
| P19 | sp:414-418 | Prio signals: Exit farthest, Combo elsewhere | COOP (D74) | — | — |
| P20 | sp:397-398 | A row tile must be empty or already hold exactly the wanted bits | COOP (D70: "touch other track" is forbidden) | — | — |
| P21 | sp:392-395, 409 | The first join is always on the merge tile | OURS: D79's quote is a permission ("you are ok to have the merge and the prio connection on the same point"), and the code makes it mandatory | Little on footprint; it removes the option of starting the prio upstream | ask the engineer |
| P22 | sp:585-597 | One pass, no retry; a prio that fails kills the candidate | OURS | Candidates are lost | keep: performance only |

## 4. Checker

| # | Location | What it fixes or forbids | Provenance | Effect on footprint or throughput | Proposed replacement |
|---|---|---|---|---|---|
| C1 | rules:154-179 | Exactly two port tiles per edge, each holding only the axis track | OURS (structural; one lane per direction, D72) | — | keep until multi-lane |
| C2 | rules:28 | `MAX_PATH_PIECES = 4096` | OURS | — | keep: performance only |
| C3 | rules:346-353, 399-412 | G1: exactly the six movements, one path each or a doubled set | COOP (D22, D80) | — | — |
| C4 | rules:366-390 | Doubled lanes share a prefix and a suffix, with disjoint middles | OURS (structural form of D19/D80) | Can't express a branch movement taking two different routes | keep |
| C5 | rules:388, 393 | Doubled lanes have equal length | COOP (R33, R53, D100) | — | — |
| C6 | rules:417-425 | G2: no level crossing | COOP (R58: "trains should not have to slow down") | — | — |
| C7 | rules:532-611 | G3: a train of 2·TL parts never drops below `reference_speed` in a curve | GAME (`train_cmd.cpp:321` port) plus COOP (R2, D29). It assumes every vehicle is half a tile long and there is no tilt | — | — |
| C8 | rules:587, 596 | G3 pads both ends of a path with a train length of straight track | OURS | A curve near the edge may be penalised once it is joined to the line | ask the engineer |
| C9 | rules:428-443 | G4: no split after a merge | COOP (R8, D24) | — | — |
| C10 | rules:472-477 | No track or signal on a bridge head; heads aligned | GAME | — | — |
| C11 | rules:483-489 | Approach track reaches the head's shared edge | GAME (D110) | — | — |
| C12 | rules:491-492 | Span accepted by `bridge_span_ok` | GAME (D35). D94 (orchestrator) also lets bridges slower than design speed through | A slow bridge slows ML trains and nothing scores it | see the objective gaps in §5 |
| C13 | rules:495-511 | Every ML bridge is doubled | COOP (D80: "you haven't doubled the bridges on the mainline so this will slow trains down") | — | — |
| C14 | rules:513-521 | No tile shared by two bridges | GAME (`tunnelbridge_cmd.cpp:507-509`) | — | — |
| C15 | rules:681-689 | Signal legality: not on a head, track present, no overlapping tracks | GAME (`rail_cmd.cpp:1078-1089`) | — | — |
| C16 | rules:691-694 | Signals face the traffic | COOP (one-way signals, D20/D68) | — | — |
| C17 | rules:716-719 | The nearest legal piece before each split and merge, and after each merge, is signalled | OURS (same as P2) | Same as P2 | ask the engineer |
| C18 | rules:721-735 | Signals before and after every bridge | COOP (D82) | — | — |
| C19 | rules:738-752 | No gap over SD·192 where a legal piece exists | COOP (D97) plus GAME weights | — | — |
| C20 | rules:754-760 | Signal type follows the setting | COOP (D68) | — | — |
| C21 | rules:769-800 | G7: at least 2·TL units between the ML split and the last signal before the merge (E→S, W→S) | COOP (D71(a)) | — | — |
| C22 | rules:802-857 | Whole-train bays before every queue merge, at least one | COOP (D83); the boundary rule is OURS (D88) | — | keep |
| C23 | rules:936-953 | G8: no block holds both a split and a merge | COOP intent (D71(b): "Deadlocks are a bit no-no"); the exact test is OURS (a sufficient condition) | — | keep |
| C24 | rules:1039-1042 | Every non-lane ML merge has a joiner and a prio | COOP (R18, D27) | — | — |
| C25 | rules:1043-1058 | The joiner's signal is Entry | COOP (D74) | — | — |
| C26 | rules:1079-1082 | The ML merge is on an axis piece | COOP-derived (the D79 join on the merge tile needs axis + h1 + the joiner's piece) | — | — (goes with P21) |
| C27 | rules:1124-1160 | Streamlined joins; no stub on the merge-tile join | COOP (D75, D81) | — | — |
| C28 | rules:1161-1167 | The prio is one straight row beside the ML | OURS; **D111 overturns it** | Same as P16 | replace with D111's generalised G9 shape (queued) |
| C29 | rules:1170-1187 | The prio touches no used track | COOP (D70) | — | — |
| C30 | rules:1189-1192 | The first join must be on the merge tile | OURS (D79 gave permission, not a rule) | Same as P21 | ask the engineer |
| C31 | rules:1203 | Coverage = 2·TL·prio_tl | OURS (stopgap; same as P14) | Same as P14 | replace with D102's L (M4) |
| C32 | rules:1207-1225 | One join per covered block, and one per row block; blocks with no joinable tile are exempt | COOP (R25, D25); the exemption is OURS (D99a, orchestrator) | — | keep |
| C33 | rules:1227-1249 | G9c: no split of the ML lane in the coverage plus one segment | COOP (D108) | — | — |
| C34 | rules:1251-1277 | Prio signals face upstream: one between segments, Exit farthest | COOP (D74, R25) | — | — |
| C35 | rules:1281-1286 | No stray unused track | OURS | — | keep |
| C36 | rules:1317-1557 | G9b: the joiner's Entry signal settles red for a one-tile train anywhere in the coverage | GAME (`signal.cpp:274` `ExploreSegment`, `:418` `UpdateSignalsAroundSegment`) plus COOP (D107). The one-tile train is OURS, and conservative | — | keep |
| C37 | rules:1589-1608 | West, east and bottom edges free of signals, switches, heads and unused track | COOP (D58) | — | — |
| C38 | rules:1630 | G3 is skipped when `curve_limit` is false (original acceleration) | GAME (D67 says the original model has no curve limit; an open item, unverified) | — | verify in 1c |

**Missing checker rules** (no row, since nothing in the code assumes them; they are gaps):
- adjacent splits on an ML path (D110(3), engineer: "Two adjacent splits ... are wrong");
- bridge occupancy against headway (D103(2));
- bridge speed against design speed (D94);
- the joiner's acceleration run after the merge (R27/D102).

## 5. The objective today

**What runs.**
- `GenerateJunction` sweeps H, then crossing cost × slack × margin × lane style.
- It keeps candidates that pass `CheckJunctionRules`.
- It ranks them lexicographically by:
  1. unit-price cost: track bits, signals, bridges, terrain;
  2. −(largest empty rectangle in the bounding box);
  3. bounding-box area.
- It stops 2 heights after the first clean height.
- The library tier uses the same key without the early stop.

**Against "the smallest footprint that works and never slows an ML train":**
1. **Footprint isn't the objective.** It is the third key. Cost tracks track length, not the bounding box, and the open-rectangle term *rewards* a big empty hole. D30 ("fewest tiles, then cost") was revised to cost first by D76.
2. **The search can't reach the smallest layouts.** The skeleton holds the gains: stubs, fences, windows, the fixed order, first-found lane programs, one bridge zone, straight prios and the ±2 crop (rows S11–S43, R3, R5, R6, P16). The checker never sees most of the space.
3. **Slowdown is only measured as curve speed (G3).** Nothing simulates trains during generation:
   - A bridge slower than design speed passes (D94).
   - Bridge headway isn't checked (D103(2)).
   - Block length against the D84 headway brackets isn't checked.
   - The joiner's acceleration in front of an ML train isn't checked (R27).
   - YAPF balance is assumed from the mirror construction, not measured.
4. **Prio length isn't from the train model.** It is a whole multiple of TL (prio_tl), not D102's L, so M4 is still open.
5. **Throughput isn't scored.** Waiting room is "at least one bay" and nothing more; traffic presets (D84) aren't wired in.
6. **Defects that silently shrink the search:** S34/S37 (first-found lanes; a dead `want_axis = 0` branch), S48 (planes off-hub at prio_tl > 1), R12 (a self-crossing route gives up the search) and S5/R14 (silent expansion-cap drops).

## 6. Ranked removal plan

Ranked by expected footprint gain against risk. Each step ends with the full suite, the `[.gensweep]` count and a rig look.

**Step 1: rulings already given (low risk).**
1. S22 + the missing adjacent-split checker rule: D110(3).
2. S35 + S42: tightest-first shift-back (D110(4)).
3. S24 `+1` and S33 `xI−2`.
4. S23 (lane B G3 hedge).
5. S43: crop after signals, to D58.

**Step 2: bridge-end tiles (medium gain, low risk).**
1. R5 and R6: no straight tile forced at heads.
2. S38: r ∈ {0, 1}.
3. S37: fix q, so the zero gap really gets tried.
4. S34: both gaps as candidates.

**Step 3: prio shape (large gain in style 1; medium risk; G9b is the safety net).**
1. P16 + C28: D111 path-following prio.
2. P21/C30 if the engineer agrees.
3. Revisit S44 (ml_gap 0).

**Step 4: skeleton fences and stubs (largest height gain; higher risk of a search blow-up, so measure time).**
1. S11 + S12 + S16 + S20: stub sweep, branch switches anywhere.
2. S15: crossover topology sweep.
3. S17 and S18: open the gap rows and the ground under the spans.

**Step 5: bridges.**
1. S29: D109 clusters, queued.
2. S10: real crossing price.
3. S8: drop the cap.
4. R12: keep searching after a self-crossing route.

**Step 6: windows and order.**
1. S19, S21, S25, S28: windows out.
2. S27: drop the slack sweep.
3. S14: order sweep.
4. R3: exact curve-grammar window.
5. S9: turn cost as a tie-break only.

**Step 7: objective.**
1. S49: score key (after the engineer's answer).
2. P14/C31: D102 prio length (M4).
3. Checker rules for D103(2) bridge headway and D94 bridge speed.
4. S48 planes fix (do it earlier if terrain work resumes).

## 7. Questions for the engineer

1. **Score order (S49).** Should the generator pick the smallest bounding box first and cost second, as D30 first said, instead of cost first (D76)? Yes/no.
2. **First prio join (P21, C30).** You said the merge and the first prio join *may* share a tile (D79). Is it also fine for the first join to sit one or more tiles upstream of the merge, when that fits better? Yes/no.
   ```
   prio row   . = = = = /        (first join upstream)
   ML         = = = = = = X = =  (X = merge tile, joiner arrives below)
   ```
3. **Switch isolation (P2, C17).** Is it enough that no signal block holds both a split and a merge (with normal SD spacing), or must there be a signal on the *nearest* possible tile before every split and merge and after every merge? Yes = nearest is required.
4. **Edges in the curve check (C8).** May the checker assume the track runs straight for at least a train length beyond each footprint edge? The endpoint solver would then have to keep its first curve that far out. Yes/no.
5. **No ML gap (S44).** Should an L R mainline with no gap be supported (for example by spreading the ML locally inside the junction, R17), instead of refusing? Yes/no.
6. **Three signals in a row on one lane (P11).** On one lane of a doubled section, inside a prio's coverage, is "signal, one plain tile, signal" acceptable, if G9b proves the prio still holds? Yes/no.
7. **Slow bridges (C12, D94).** When no bridge type reaches design speed, should the generator prefer a layout that avoids an ML bridge (or report no layout), rather than accept the slowdown? Yes/no.
8. **Branch stubs (S11).** Is there any coop reason for straight track on the branch legs between the edge and the first split or merge, beyond D58's one tile? Yes/no.
