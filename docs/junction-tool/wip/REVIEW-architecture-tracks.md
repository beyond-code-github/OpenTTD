# REVIEW-architecture-tracks: review of the planar solver (at wt-planar bc47f12f73, probe-pr dc79e8c83f)

**Verdict.** The failures keep coming back because no single structure owns the hub's tracks. "Which track lies where relative to which" is worked out four times, at four levels of detail, and no level is derived from another:
- the line-level planar map: lines and every topology crossing; no copies, no rows;
- the slot layer: channel intervals holding lines and copies; sites reduced to links between straight legs;
- the composer: every strand, built by hand in each component;
- the signal pass and the checker: tiles.

Each stage sees only the tracks of its own level. So any track one stage adds (a row, a copy, a fray's or join's own legs, a touch point) has to be restated to the other stages as a hand rule, a reservation, or a geometric re-guess. The missing piece is a **track-level planar map**: today's line-level map refined per variant so that every physical track is an edge. Slots, clash sides, ladder walls and the identities the export carries would all be read from it.

DESIGN-tracks 15.5 (sites as slot intervals) and STATUS:43 ("rows as lines of the planar map") are its two halves, and they should be designed as one artefact. Neither covers the half that runs from the export to the signal pass, or a rule for tracks whose length is metric (the prio row).

The riskiest assumption is that one order, fixed before lengths, holds every track and stays feasible with today's fixed component shapes. A test of about a day on probe-pr's T2 layouts can settle it.

Separately: the product path still sends T2 and LL² to the hand compositions by name, which D326 forbids.

Marks: [V] verified in code (wt = wt-planar, pr = probe-pr; pc = planar_components.cpp), [I] inferred.

## Q1. The missing piece

**Today's four models of order [V]**
1. **Planar map.** `PlanarLineSides` builds it from topology lines and all topology crossings, a fray's and a join's own crossings included (planar_regions.cpp:243-295). Copies borrow their line's sides because `LineOfStrand` strips the copy mark from the name; `prio:B1` maps to no line at all (planar_regions.cpp:369-375).
2. **Slot layer.**
   - It cuts channels only at channel crossings (planar_slots.cpp:47-56) and lists lines and copies (:58-76).
   - It links only straight legs across a site (:163-187, `node_legs[n].straight`). A turning leg has no link.
   - The site's own crossings are computed (`group_crossings`, planar_channels.h:67) and read nowhere. `git grep` on both branches finds only planar_channels.cpp:123 and :133.
3. **Composer.**
   - `Fray` and `Join` rebuild the site's insides by index rule: "V_k crosses every nearer S_j" (pc:988-1027, :1031-1100).
   - Several facts are stated twice. The copy side is in pc:1172 and pc:1451-1456, and again in planar_slots.cpp:58-72. The prio fray's straight-leg move is in pc:1016-1019 and again in planar_slots.cpp:173-181.
4. **Export.** It is per route path (`PlanarRouteProgram`, planar_components.h:26-41), so a track with no route (a row) has no place in it.
   - pr adds a separate list of rows (pr planar_components.h:52).
   - The signal pass then re-matches row pieces to main-line pieces by tile adjacency (pr junction_signals.cpp:379-397). The composer already knows that pairing: row segment i is tied to lane segment row_first+i (pr pc:1116-1121).

**Each failure traced to the stage that lacked the track or the constraint**

| Failure | Missing where | Cite |
|---|---|---|
| fray's turning vs straight legs' cross-section | slot layer: site internals absent | slots:47-56, :163-187 [V] |
| joiners spread before the join | same; patched by a hand rule | pr slots:152-180 [V] |
| joiner turn-in fixed at 4 (the order gives 2) | composer constant standing in for a slot difference | wt pc:1044 [V] |
| row walk through a multi-piece diagonal | signal pass re-derives the row from tiles; `corner` = one half piece between axis pieces | wt signals:370-393 [V] |
| checker rejects a row on the opposite diagonal half | the checker's own straight model (ruled later, D353) | wt rules:1331-1364; pr :1391-1396 [V] |
| touch points only on axis pieces | signal pass | wt signals:393; pr :444-445 [V] |
| prio signal spacing on diagonals | a metric need of the pass, absent from the length model | pr signals:516-521 [V] |
| loose fringes (111 tiles) | row not in the map: no side, no rungs, so greedy trial | regions:369-375; pc:2216-2217, :2237-2259; ladder walls only from line faces, pc:1770-1782 [V] |

**The piece: a track-level planar map** (the name is mine)

- **Inputs:**
  - the topology;
  - the variant: flipped channels, every over/under including D347b's inside joins, and the ties;
  - the resolved sites;
  - the line shares (which crossings double: D312, D351, D352);
  - the prio set and its form;
  - the shape: each interval's heading, which says where a slot change may go (15.5).
- **Construction:** today's line-level map, plus one local rewrite per atomic primitive (D330):
  - **Doubled crossing:** a copy edge beside the line, on the D312 side, from a copy-split vertex to a rejoin vertex after the last consecutive doubled crossing (D352). It crosses everything the line crosses there.
  - **Prio join:** per lane, a row edge on the side away from the joiners (D349). It runs from a row-start vertex on the lane to the merge-tile join (D79/D81). It crosses every track that crosses the lane in between (form B: the joiner decks). In form A it has a gap beside the doubled stretch (§13.1).
  - **Fray and join legs:** already edges; their crossings only need over/under labels.
- **Holds:**
  - vertices: port ends, nodes, crossings (with the over track and the deck group), copy split and rejoin, row start, merge join;
  - edges: each a track, with its kind (Line, Copy or Row), its owning line and its lane;
  - the rotation system, and from it the faces (as `Faces()` today);
  - for each corridor interval, its tracks left to right;
  - `Side` for every pair of tracks that bound a common face;
  - **role tags** carrying the constraints components need:
    - a copy sits exactly one slot from its line, and the line ends on the copy's slot (D351);
    - row coverage is at least 2P+2, summed over the row's edge range;
    - there are at least ⌈coverage/SD⌉ touch points ("one touch point per signal space", STATUS:42).
- **Read by:**
  - **Slot layer:** intervals and orders from the map, generic rules only: neighbours at least 1 apart, a copy exactly 1 from its line, a node's legs on its slot, steps only where the heading allows.
  - **Composer:** segments per (track, interval), as `Bundle` already builds them. Component code places only its fixed coop internals, on role-tagged tracks (DESIGN-tracks §2.3).
  - **Clash loop and ladders:** a strand is a map edge, `Side` is looked up by id, and rows and copies are walls.
  - **Export:** every piece carries its track id and kind. A row carries its lane, and each row piece carries the lane piece beside it.
  - **Signal pass:** places touch points and signals on known row/lane pairs. It searches positions (D345), not geometry.
  - **Checker:** stays an independent judge on tiles (D193), but has to catch up with D342 and D353.
- **Replaces:** every item listed in Q2.
- **The one new rule it needs (extent):** a row's start is a vertex on a chosen lane edge, which is topology. Its position along that edge is free in the length solve, which is metric.
  - Today pr ties the row start to the start of the lane's last run (pr pc:1057, :1113-1115), so the row is as long as the whole run, not 2P+2 [V].
  - Whether that over-length costs area has not been measured [I].
- **What the rulings already cover:** D349 (the row is a line of the bundle) and D350 (an ordered track list per interval) already rule the contents. 15.5 is the slot half and STATUS:43 the sides half. Still missing: one artefact feeding both, the export half, and the extent rule.

## Q2. Local patches for that piece (wt unless marked pr)

- **Slot facts stated by hand:**
  - prio fray gap of at least 2 (planar_slots.cpp:34-41, :115-116);
  - prio join entry gaps 0, 1, 3, 5: the stagger written as a slot fact (:143-155);
  - prio fray's straight leg fixed one slot over (:173-181);
  - copy sides (:58-72);
  - pr: rows only in the straight channel's last interval (pr slots:74-84);
  - pr: joiner spread of at least 3 (:152-180);
  - pr: a plain fray's straight legs keep its cross-section (:183-205).
- **Composer offsets and constants:**
  - fray gap (pc:1007-1010);
  - coverage as one straight 2P+2 piece (:1040);
  - turn-in constants (:1044-1045);
  - one doubled stretch per crossed joiner (D322, :1049-1059);
  - close-up of O[j] by 2j (:1076-1079);
  - exit to the left refused (:1034-1036);
  - exit steps from planar_gaps rules (:1083-1094, :1499-1503, :1536);
  - the Flex jog, i.e. the candidate-step bit (:1103-1124, :1383-1387);
  - copy sides (:1172, :1451-1456).
- **Reservation:** `prio_rows`, a fixed 2P strand beside the merge that clashes with everything (pc:1075, :1859-1875).
- **Sides by hand:**
  - fan rule (pc:1659-1677);
  - side rule (:1678-1707);
  - copies joined to ladder walls by a side test (`LadderCopy`, :1761-1765);
  - strand to line by name (regions:369-375);
  - greedy trial for every unsided clash (pc:2237-2259).
- **Probe-only:**
  - full-share joiner refused (pr pc:1043);
  - the crossed row piece picked by index arithmetic, where §2.3 says by role (pr :1129);
  - an optional 0-or-at-least-2 segment, a yes/no bend that D340 and D350 rule out (pr :1134).
- **Signal pass and checker:**
  - tile re-derivation of the row (wt signals:370-421: `corner`, `row_of`, `plain`, `row_free`);
  - pr: tile re-matching (`row_index`, :379-397);
  - pr: touch points beside diagonals (:444-445);
  - pr: `CheckPrio` bypassed for given rows (:514);
  - pr: a spacing judge (:516-521);
  - pr: an opposite-half whitelist in the checker (pr rules:1391-1396).

## Q3. Riskiest assumption and the quickest test

**Assumption:** every track the components build has an order fixed by topology and variant alone, so separating tracks by the map's sides is feasible with the fixed coop shapes. Put another way, no good layout needs a side that depends on lengths.

**Why it is the riskiest:**
- Slots, sides and ladders all rest on it.
- The one direct test so far (rows given their lane's sides) was reported infeasible.
- One order today already depends on length: the stagger, where lane k's row and lane k+1's copy share one track over different stretches (planar_slots.cpp:143-155; REVIEW-sideways Q1.3). D349 removes it by giving each its own track, at a cost in area.

**Rule out a confound first (30 minutes)** [I]. The infeasible result may come from a wrong direction rather than a wrong side:
- The sided round takes, for each clash, the first admissible direction from `SepOptions`, sorted by current violation (pc:2219-2224).
- It writes that as an endpoint box: all four end pairs at least 2 apart (:2075-2077).
- One failure ends the variant (:2229-2233). The other admissible directions are never tried.
- A row against its own lane gets lp == lq and falls through to greedy trial.

Re-run that experiment with `PLANAR_IIS=1`. A diagonal `sep … dir(±1,±1)` row in the IIS points to the direction, not the side.

**Test (one day at most, test-only, on probe-pr):**
- **A (hours, no solver change).** In the 932.25 layout of T2 o1t3f1s0 (rows clashing, feasible):
  - for every row and every strand within clash distance, read which side the row lies on;
  - compare it with the side a row edge inserted into the line-level map would give (beside its lane on -sigma, from its start to the merge, crossing exactly what crosses the lane in that range);
  - also sum each row's length minus 2P+2.
- **B (one day at most).** Put the rows into the map:
  - `LineOfStrand` maps `prio:L` to a row entry carrying the inserted edge's sides;
  - rows join the walls as copies do in `LadderCopy`, so rungs separate them, not endpoint boxes;
  - the row start is free along its first edge.

  Run T2 o1t3f1s0, then the T2 and T3 sets.
- **Pass:**
  - every variant the greedy run builds is still feasible;
  - area below 932.25 on o1t3f1s0;
  - every prio fits end to end;
  - what remains above 821.25 (no rows) equals the row tiles.

  If it passes, build the map as one artefact (15.5 plus sides).
- **Kill:** either of these:
  - A shows a row on both sides of a track it never crosses;
  - B is infeasible with an IIS that pits a component's fixed rows (`jdeck`, row offset, coverage) against the map's separations.

  Either one means the components contradict the map. They would then have to be rebuilt from it site by site (15.5's composer step) before the map can supply sides.

## Q4. Other conflicts with the goal (not covered by Q1)

1. **The product path still uses the hand compositions (D326, D330) [V].**
   - The window calls `PlanarSeedFor` (junction_gui.cpp:1203), which calls `SolvePlanarComponents` (planar_plan.cpp:92). That routes "T2" and "LL2" by name to the hand `Composer` (pc:2928-2930).
   - `PlanarHubText` returns only T1, T2 and LL² (planar_plan.cpp:430-436). T3 and any other hub get no topology.
   - The rig's mapped builds must come from another harness [I].
2. **Separation still decides, the ladders only help.** Even with sides, the direction is greedy and a failure is terminal (as in Q3). The separations found on the first, track-length solve are carried into the area pass (pc:2301-2307), so early choices limit the area. D339 says ladders replace the loop (ROADMAP item 3).
3. **Acceptance runs through a checker that lags the rulings (D345 vs D193, still open).** `CheckPrio` still assumes one axis (rules:1331-1364; AUDIT A1-A12), and pr had to bypass it (pr signals:514). A bent prio is now judged by reach alone.
4. **The signal pass's metric needs are judged only after the length solve.** Its needs are touch points per signal space and spacing along diagonals (pr signals:516-521). No row of the length model carries them (pr `Join` [V]), so coverage that is too short can only fail late [I].
5. **Fixed-width refusals stay in the components:** pc:1349-1351, :1401-1403, :1034-1036, :1515-1517, and `SlotBend` on a diagonal (:900-902). The map is n-lane by construction; these components are not.
6. **Some choices remain search bits.** The s-bit probe showed candidate steps stay bits (STATUS:35), against D350's "retire". That is fine while they are searched, but it is where the cost of scale goes (§14).

## Suggested tests
1. **Map invariants on T2 o1t3f1s0:**
   - V - E + F = 2;
   - each row crosses exactly the joiner decks of its join;
   - Side(X, row(L)) = Side(X, L) for every X that does not cross L within the row's range.
2. **Slots from the map alone:** the T3 joiner channel gives J0, row, S1, J1 at 3 apart and a turn-in of 2, with no pr slots:152-180 rule.
3. **Export:** every row piece names its lane piece, and the signal pass places touch points with no tile matching.
4. **pv regression:** T2, T3 and LL² against probe-pr, with LL² unchanged (it has no n-lane prio join).
