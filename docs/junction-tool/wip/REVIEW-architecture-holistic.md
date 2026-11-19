# REVIEW-architecture-holistic: review of the planar solver architecture (at 9460806ea5; probe-g3e f40d43c61c; wt-g3b cfdd91a517)

**Verdict.** The foundations are right and mostly valid by construction: the topology and its planar map, the channel graph, the track map (TM1) and the slot layer as a min-cost tension LP. Above the length program, though, no hand-off carries an invariant that makes the next layer feasible.

The code is therefore a generate-and-test loop:
- heuristic layers propose;
- the length MILP, with its lazy clash loop, is the only validator;
- every IIS becomes a local rule.

Each new design has landed as a new composer (hand Composer, MappedComposer, TrackComposer) that re-makes the decisions instead of consuming them.

D361's stack is reachable, but not by patching a composer. It needs three changes:
1. **The lateral layer owns every sideways move,** in frames that cover every cycle closed by a pin. Today it has one frame per channel; 15.5 adds node groups. Port-side gaps between channels, bands between single-line channels and deck spans are still outside every frame.
2. **The shape layer owns deck capacity.**
3. **Separation comes from a fixed order per stretch,** decided once, not from lazy rounds.

The orchestrator's reading ("15.5 plus G2 channel-path headings only") is correct as far as it goes. On its own it brings back G3b's T2 failure, the W-side gap that D359 was covering, and leaves the f0 deck-max class untouched.

The cheapest decisive experiment is a composer that reads only slots and channel-path headings, run with clash rows off, counting base-infeasible variants. Each IIS then names the missing frame (§7).

Marks: [V] read at the cited place; [I] inferred. `pc` = `src/junction/planar/planar_components.cpp` at 9460806ea5; `g3e` = the same file on probe-g3e.

## 0. The problem, from first principles

**Inputs.**
- A topology (planar CLAUDE, "Topology"):
  - ports on the box sides (side, in or out, heading, lines in reading order);
  - routes, each a lane path from an entry line to one or more exit lines;
  - 1→2 splits and 2→1 merges in authored cascades;
  - the prio routes.
- A spec: train length TL, curve spacing CL derived from speed, prio length P, the longest bridge middle (`MakeParams` pc:2909-2918).
- Terrain, later.

**Output.** Track pieces on the tile grid, with bridges and signals, as route programs. It is priced and built with ordinary commands (planar CLAUDE, "Back end").

**Hard constraints.**
- **Geometry (game):**
  - eight headings, at most 45° between pieces, no 90° (R1);
  - curve spacing between same-hand bends along every route path (R2, CLAUDE "Curve speed");
  - tile parity;
  - split and merge geometry: one leg keeps the heading, the other leaves by 45°;
  - no shared tile except the allowed pairs.
- **Crossings (game):**
  - grade-separated;
  - a bridge is straight and axis-aligned, with heads on whole tiles and its middle at most the maximum ("deck max", pc:1361-1365);
  - no bridge over a bridge;
  - the under track is not parallel to the deck.
- **Coop rulings:**
  - split before merge (D24);
  - every main-line merge has a prio, with coverage of at least 2P+2, no main-line split inside it (D108), and real pre-signal reach (D107);
  - full-share lines are doubled at crossings (D331, D332), as equal parallelograms (D351, D312, D322);
  - whole-train bays of 2TL+4 (D88);
  - ports pinned at the notation spacing (D313, pc:1294-1314).
- **Checker:** meant to be a test oracle, not an acceptance step (D193, still OPEN).

**Objective, scale and speed.**
- Area inside the outline, linearised, with track length as the tie-break (D315, D329, D336).
- The scorecard: empty tiles, wasted sideways travel, track (D357).
- Each variant as compact as its own decisions allow (D358).
- Any topology, lane count and pairing (D326), up to LLLL_RRRR and beyond (D126).
- A seed under 5 s and a hover under 100 ms (D297).

**Combinatorial and continuous decisions.**
- **Combinatorial:**
  - which line pairs cross (forced by interleaving on the ring, picked greedily);
  - crossing order (triangle flips, D344);
  - over or under (D328, D347b);
  - the straight leg and side at each split and merge (ties);
  - doubling (by rule, D331);
  - the integer sideways position of every track per stretch (S-bends);
  - whether a bend exists (headings, D340);
  - which breakpoints of two facing walls face each other (the corridor "phase" of §7, read today from a rough solve, D339);
  - where the prio joins and signals go.
- **Continuous** (integer, but ordered): piece lengths, and from them bend positions, deck spans, coverage extent and area.
- **Growth:**
  - Variant bits live at channel level and grow with channels and pairing, not with lanes. T2 and T3 have 5 bits (32 tags); LL² has 13 (8192, §14); lane-paired LL3a has 11 open crossings (§14). T2 and T3 rank variants alike (Spearman 0.945, §14).
  - Tracks, crossings and potential clash pairs grow with lanes, at least quadratically for any-to-any pairing, because fork and merge cascades and the crossings of choice legs multiply. LL² already has six forks and 15 sites (test planar_channels.cpp:160-172) [the exponent is I].
- **Measured:** with the discrete choices fixed, the continuous solve is fast: T3 0.22-0.38 s per variant against 12-36 s with the transition binaries free; LL² 15 ms (DESIGN-components §8, D327).

**What a good architecture for this needs.**
1. Each hard constraint is owned by one layer: the one that has the information to decide it.
2. Discrete decisions are made at channel size; continuous ones at track size.
3. Every hand-off has an invariant that guarantees the next model is feasible. Then variants are ranked on cheap scores and never found infeasible by an expensive solve. With a 15 s budget per variant (pc:52), D297's 5 s seed leaves room for at most one MILP.
4. Every physical track (copies, rows) exists in every layer's model. No space is reserved by hand.
5. The expensive solve runs a bounded, small number of times.

## 1. The current architecture as a decomposition

**Shipped path** [V]:
- The window maps only T1, T2 and LL² (`PlanarHubText`, planar_plan.cpp:430-436), then calls `SolvePlanarComponents` (planar_plan.cpp:92).
- T1 is refused and goes to `planar_layout`, whose seed takes 616 s (ROADMAP, "Solve speed").
- T2 and LL² go by name to the hand `Composer`, including `Block227` (pc:3042-3044, :241).
- `MappedComposer` runs only for other names, so in practice only in tests and on the rig.

This breaches D326 and D330. REVIEW-architecture-tracks Q4.1 flagged it on 2026-11-19, and it is unchanged.

**Mapped path per variant** (`SolvePlanarMapped` pc:3098):
1. **Topology** (planar_topology): lines, nodes, crossings (chosen greedily), event order (chord drawing); a validated line-level planar map.
2. **Channels** (planar_channels): channels, sites (straight channel, side, tie), `node_legs`, `line_share`, `group_crossings`.
3. **Variant:** bits o/t/f/s. Up to 6 bits every variant is enumerated; above that, a local search runs whole solves (pc:3105-3198).
4. **Shape:**
   - `BuildPlanarPaths` gives the default shape: headings, crossing pieces and over choices, with retry-capped detours (planar_paths.cpp:335, :347) and a fork-leg jog (:357).
   - `SolvePlanarShape` is a per-channel ILP: crossing rules and least turning; no faces, widths or decks (whole of planar_shape.cpp).
   - Both shapes are solved in full and the smaller area wins (pc:2885-2901, D341).
5. **Lateral:**
   - `BuildPlanarSlots`: one frame per channel with a free offset (planar_slots.cpp:102); links only for straight legs (:165, :169); hand facts (:35-42, :143-155, :173-180).
   - `BuildPlanarGapSteps`: ring rules (planar_gaps.cpp:88-137).
   - `PlanarCandidateSteps`: variant bits.
6. **Composer** (`MappedComposer::Build` pc:1423-1979):
   - Fray, Join, Fork, Flex, Bundle and core recipes, with Double and PairDecks;
   - the `prio_rows` reservation (pc:1103, :1344-1360);
   - the fan rule and side rule as up-front separations (pc:1852-1900);
   - under-deck rows; ladder faces;
   - `FinishPlanarModel`: ports, anchor, pins, box, decks, bays, curve rows.
7. **Solve:**
   - `SolveLazy`: the base solve, then lazy clash rounds, then one area linearisation (pc:2191-2385).
   - Ladders: a rebuild with rungs, at most two passes, with the lazy loop inside each pass (pc:2761-2790).
8. **After the solve:** `PickBest` by area, export, render. The signal pass places each prio by search: every leaf is judged by `CheckPrio` and `CheckPrioReach`, up to 20000 leaves (junction_signals.cpp:356, :466-468, :509).

**Who owns each decision** [V unless marked]:

| Decision | Owners in code | Owned once, where the information is? |
|---|---|---|
| Crossing set and order | topology chords; triangle bits | yes |
| Over/under | planar_paths at channel crossings; the Fray/Join recipes inside sites; G2 rule 4 on the probe | two owners; D347b form B is unreachable in MappedComposer |
| Sides | `PlanarLineSides` per line; fan and side rules (pc:1852-1900); the order `SepOptions` tries directions in; greedy trial for unsided clashes (pc:2304-2326); rows have none | four owners; the per-stretch track map is built but read only by the probe (g3e:3483) |
| Copy side | planar_slots.cpp:58-78; pc:1644-1648; pc:1200; Fray/Join's own `Double`; the track map | stated five times |
| Sideways moves | see §4 | at least seven owners, against D350 |
| Bend existence | shape headings, plus recipes, plus G2 diagonals (probe) | split |
| Bend placement, lengths | MILP | yes |
| Separation | lazy loop, up-front rules and ladders | three mechanisms, none complete |
| Deck span against deck max | crossing pieces are chosen by the shape, which knows no widths; the limit is an MILP row (pc:1364) | decided where the widths are unknown |
| Prio row | composer reservation; the signal pass re-derives it from tiles; checker | three owners |
| Signals | signal pass, by search | one owner, but by generate-and-check |

**Hand-off invariants:**

| Hand-off | Stated invariant | Guaranteed or checked later |
|---|---|---|
| topology → channels | the map validates; each must-cross pair crossed once | guaranteed (refusal otherwise) |
| channels → shape | only `CheckPlanarShape`'s heading and crossing rules | realizability not stated. TM3p: 18 of 27 crossing-angle shapes of T2 o1t3f1s0 are base infeasible (STATUS, TM3p) |
| shape + sites → slots | consecutive tracks at least 1 apart | guaranteed by the LP, but slots never see headings. The composer then refuses "a slot step on a diagonal piece" (pc:947) and "slot change inside one piece" (pc:908) |
| slots → composer | offsets as lower bounds (`MinGap`); the run into a join exact | the composer may `Fail` (pc:908, :947, :1063, :1543, :1595, :1709, :1754): it discovers that an earlier decision cannot be built |
| composer → MILP | none | the base solve is the feasibility test ("base infeasible") |
| MILP → separation | per-line sides | lazy and greedy; one failure ends the variant (pc:2297-2299, :2317-2320) |
| layout → signals | none: rows are reserved, not built | "the prio element does not fit", found after the solve (junction_signals.cpp:509; STATUS ST4, TM5p) |
| signals → acceptance | the checker | in acceptance (D193 OPEN) |

**Every generate-and-check site:**
1. Variants are enumerated or searched by whole solves; failures are found by solving (pc:3105-3198).
2. Both shapes are solved in full to pick one (pc:2885-2901).
3. Path detours retry, with caps (planar_paths.cpp:335, :347).
4. The composer refuses what earlier layers decided (the `Fail` list above).
5. The base solve is the oracle, and IIS-driven fixes followed (the whole G3 chain, STATUS).
6. The lazy clash loop (pc:2191-2330).
7. The ladder re-read passes (pc:2764-2790).
8. Forced local rules that can make the MILP infeasible:
   - the prio fan (pc:1025);
   - the fray gap (pc:1035-1038);
   - join `lo` constants (pc:1072);
   - the close-up `2j` (pc:1106);
   - hand slot facts (planar_slots.cpp:35-42, :143-155, :173-180);
   - ring-rule refusals (planar_gaps.cpp:113);
   - on the probe, D359 and G2 rules 2 and 3 (planar_paths.cpp:473-502).
9. The prio search in the signal pass, judged by checker functions.
10. The checker in acceptance (D193).
11. A flat seed, then terrain facts added lazily (planar CLAUDE, "Back end").

The area linearisation (D329) is approximate optimisation, not a feasibility check, so it is acceptable.

## 2. Prior art

**Topology-shape-metrics** (Batini, Nardelli and Tamassia 1986; Tamassia 1987).
- The structure: planarise (fix the embedding), then shape (an orthogonal representation by min-cost flow), then compact.
- The guarantee: any valid orthogonal representation compacts. Lengths are bounded only from below and the box is free.
- This project is TSM: the topology layer, headings as the shape (D340), and the length MILP as the metrics.
- The guarantee fails here for a reason that can be listed. The metric layer holds equalities and upper bounds that the shape cannot see:
  - port pins (pc:1313);
  - deck max (pc:1364);
  - parallelogram and equal-bridge equalities (pc:992-995, :1012-1013);
  - fixed S-bend lengths;
  - slot offsets.
- **Borrow:** enumerate exactly these and make each the invariant of a discrete layer.

**Turn-regularity and the complexity of compaction** (Bridgeman, Di Battista, Didimo, Liotta, Tamassia and Vismara 2000; Patrignani 2001; Klau and Mutzel 1999).
- Orthogonal compaction is NP-hard in general.
- It is polynomial when every face is turn-regular, i.e. the relative position of facing reflex corners is determined.
- Klau and Mutzel make the undetermined pairs binary variables.
- That is exactly the ladders' open question, which parts of two walls face each other (§7: matching corridors by heading runs fails). D339 reads the answer from a rough solve and re-reads it.
- **Borrow:**
  - make the phase an explicit decision of one named layer, refined once;
  - shrink the set of faces that need it, by putting parallel walls into a shared frame.

**VLSI routing.**
- The flow: global routing (capacity per region), track assignment (Batterywala et al. 2002), channel routing (left-edge, Hashimoto and Stevens 1971; dogleg, Deutsch 1976; Yoshimura and Kuh 1982), then compaction.
- The invariant from global to detailed routing: demand at most capacity in every region.
- Channel routing is valid by construction only because the channel's height is free. A switchbox (VLSI's term: a region fixed on all four sides) is not, and needs rip-up and reroute.
- A hub is a channel along its free extent, and a switchbox wherever ports are pinned.
- **Borrow:**
  - capacity rows decided above the lengths: deck span against the crossed width, and corridor demand against the pinned width;
  - the dogleg (an S-bend) is owned by track assignment, i.e. the slot layer;
  - honesty: where two dimensions are fixed, a bounded iteration is the known price.

**Layered-drawing coordinate assignment** (Gansner, Koutsofios, North and Vo 1993; Brandes and Köpf 2001).
- Coordinates within a layer of fixed order, solved by network simplex, come out integral. That is today's slot LP (REVIEW-sideways-modelling Q1).
- The difference that matters: in a layered drawing every node of a layer lies in **one** frame. The slot layer gives each channel its own free offset (planar_slots.cpp:102), so it cannot state any relation between two channels. This is the frame problem of §4.

**Octilinear metro maps by MILP** (Nöllenburg and Wolff 2011).
- One MILP: direction binaries, order constraints, and pairwise planarity rows added lazily. This is the same lazy clash loop, and it is their known weak point at scale.
- Bast, Brosi and Storandt (2020) route on an octilinear grid graph, where tile occupancy gives non-overlap by construction. That is §11's windowed grid layer.
- **Borrow:** the warning, not the method.

**Rail layout synthesis.** I recall SAT and MILP work on station track layouts over a model of splits and segments (Norwegian, around 2019-2021, Luteberg and others). This is UNVERIFIED here: I could not check titles. I know of nothing on compact grade-separated hubs with train-length rules, so graph drawing and VLSI are the closer guides.

**What it resembles.** TSM, with a track-assignment layer between shape and metrics, and a Nöllenburg-Wolff metric layer. Each of those gives a per-layer guarantee this project lacks:
- TSM: a valid shape always compacts;
- VLSI: capacity met means channels are routable;
- layered drawing: one frame per layer.

## 3. The history as evidence

**The pattern: the MILP is the only validator.** Every design question is answered by an IIS, and every answer becomes a local rule, a loosening or a new composer. Evidence:

1. **Correct by construction, asked for twice.** D193 (2026-11-11) and D361 (2026-11-19) state the same goal, eight days apart. D193 is still OPEN.

2. **Sideways ownership went round a full circle in four days:**
   - D327, width profiles (11-16);
   - D335, gap rules (11-17), built as ring rules;
   - D338, every free stretch may bend;
   - D340, no optional bends (11-18);
   - D350, the slot layer owns sideways position;
   - D359, port steps from ring rules, outside the slot layer (11-19);
   - D360, back to the slot layer.

   Three reviews reached "one owner of sideways position or order" independently:
   - REVIEW-channels (the gaps between channels);
   - REVIEW-sideways-modelling (a track-assignment layer);
   - REVIEW-architecture-tracks (a track-level planar map).

   Each finding was partly built, then consumed by a composer that kept its own rules.

3. **Rules added, then loosened:**
   - G2's drop rule was loosened twice in hours (98ef7c11c0, 587ab90166);
   - D321's optional bends were removed by D340;
   - the ST strip of unruled constraints was "about neutral" (STATUS);
   - D355 and D356 loosen composer constructions that had no ruling behind them.

4. **Hand-reserved space:**
   - the `prio_rows` strand (pc:1344-1360);
   - the slot facts: fray gap, entry gaps 0, 1, 3, 5, fixed fray leg (planar_slots.cpp:35-42, :143-155, :173-180);
   - `lo` 4/6, `STUB`, `FANW`.

   §15.5's own finding lists five such reservations.

5. **Composer churn.** Hand Composer, then MappedComposer, then TrackComposer, each re-making every decision. MappedComposer carries seven experiment toggles that stayed: NORULES, NOTIE, NOUNDER, FACESEPS, ALIGN, NOLADDER, SHAPE (planar CLAUDE).

6. **The probes measure themselves against the oracle.** probe-g3e has 17 commits in a day, each moving the IIS by one step (git log).

7. **Agreed but not done.** Retiring the lazy loop was agreed (D339, §8.2, ROADMAP item 3) and the loop still runs inside every solve (pc:2782). The D326 breach (§1) was flagged and stays.

8. **The docs drift.**
   - STATUS's table of bests (T3 1933.25, LL² 1347.25) disagrees with its PLAN line (T3 1907.25, LL² 2162.25).
   - planar CLAUDE still says `SolvePlanarShape` is called by nothing, which contradicts its own third bullet. REVIEW-sideways-modelling Q5 noted this already.

## 4. Sideways moves

**Owners per kind of move:**

| Kind | Agreed owner | In code (shipped / probe) | Departs? |
|---|---|---|---|
| Lane spacing change in a channel | slots (§15, D350) | `SlotBend` from slots (pc:943-953); also the Fray prio fan (pc:1025) and the Join close-up (pc:1104-1107) | yes: the composer still makes fixed steps |
| Doubled bridge's move | the bridge (D351); slots charge only the step back | `Double` (pc:986-998); slot rows (planar_slots.cpp:118-133) | no |
| Fork leg divergence | slots through 15.5 (D360) | `Fork` makes both diagonals optional (pc:963-964), against D340; the probe's G2 rule 2 forces both (planar_paths.cpp:473-487) | yes |
| Joiner landing | slots through 15.5 | Join diagonals with `lo` constants (pc:1072); the probe's G2 rule 3 (:489-502) | yes |
| Close-up or fan at a pinned port gap | D335's width profile, then slots (§15) | ring rules (planar_gaps.cpp:88-137); per-node merge exits (:184-211, used at pc:1729); on the probe D359 = ring rules plus candidates only (g3e:2934-2941) | yes, outside slots. On the probe, the per-node merge close-ups at EO, WO and SO have no owner at all |
| Jog of a straight channel | variant bit (§3a); §15.3 said retire | Flex jog (pc:1576-1580) | open: the s-bit probe showed slots cannot choose it |
| Shape jogs and detours | D340 "jogs stay with construction rules"; §16.1 "jogs are not shapes" | planar_paths.cpp:335-357 | yes |
| Prio row beside its lane | D349: the bundle widens | fixed reservation (pc:1344-1360) | yes |

**Is §15.5 enough for LL²'s forks and merges? Necessary, not sufficient.**

What it covers:
- **The exits.** EO1 and EO2 form one channel (test planar_channels.cpp:107-108). Their site is a Join of two nodes (:169-170).
  - Its in-lines, left to right, are E1, EDj1, EDj2 and D2 (planar_plan.cpp:460-461). Its out lines are EO1 and EO2, pinned one slot apart.
  - A 15.5 frame holding all six tracks sees four tracks become two. It must charge two slot units of close-up: on EO1 and EO2 after the merges, or on E1 and D2 before the band arrives.
- **The G3l/G3o binding class** is the same shape at WO: C1, CBj1, CBj2, B2 → WO1, WO2. That class was a fork diagonal plus the CBj1 and CBj2 merge diagonals, against WO pinned 2 apart.
  - MappedComposer covers it today with the `PlanarMergeExitStep` rule (pc:1729).
  - The probe has no owner for it. D359 lists only ring rules (g3e:2934-2941), and `BuildPlanarGapSteps` skips ring neighbours that share a channel (planar_gaps.cpp:128) [V]. That this caused the LL² IIS is [I].
- **The forks.** E2's fork-site frame (E1, E2 → E1, E2a, E2b) decides which leg stays straight.

The frame problem. Frames are joined only by charged links, and every single-line channel has a free offset, so a link costs nothing (BRIEF-G3m: "charging both legs' steps alone costs 0"). Any requirement that spans frames is invisible to the solve. Five gaps follow:

1. **15.5 is written for two-channel sites** ("the straight channel's lines, the joiners and the out lines"). Every LL² exit site has four in-channels and is composed per node (`others.size() != 2`, pc:742-745).
   - **Fix:** define the site over a node group with any number of in-channels, ordered by the map.
2. **Bands between single-line channels.** D1a crosses E2b (a channel crossing between two single-line channels), then lands on E2a at EDj1 (planar_plan.cpp:456-459).
   - So the face between E2a and E2b must hold D1a's approach, two slots wide.
   - The fork frame says one slot. The EO frame keeps EDj1 and EDj2 adjacent.
   - No frame holds E2a, D1a and E2b together [I]. Today the MILP and the lazy loop find the room.
3. **Port-side gaps between two channels.** On T2's W side, EF (in) and CB (out) are pinned two tiles apart, while F leaves E and crosses B between them. This is G3b's IIS (BRIEF-G3b2, Why 1).
   - EF and CB share no site, so no 15.5 frame holds both. D359 exists for this case.
   - Withdraw D359 with only 15.5 in place, and the IIS comes back [I].
4. **Deck spans.** The f0 class: B's deck pair has to span both D lanes and both F lanes under deck max 24 (G3k, STATUS).
   - This depends on how many tracks of other channels cross under one over piece, i.e. on the crossing pieces the shape chose.
   - It is a capacity fact for shape or skeleton, not a sideways move.
5. **"Not available on a diagonal piece of one heading"** (15.5) makes slots depend on headings.
   - The shape cannot see room, so a needed step can land where it cannot be built.
   - Better: make every step buildable. On a diagonal stretch, a step is an axis excursion (§3a; §15.2 "Diagonal intervals"; S5 not built). Then the slot layer needs no headings.

So the lateral frame must be the region between neighbouring tracks wherever a pin closes a cycle:
- channels;
- node groups;
- port-side corridors.

The last are D335's model, computed from the track map's faces instead of ring rules. Faces that only open as wedges from a node or a crossing need no frame: the node's or crossing's own geometry separates their walls.

## 5. The layer stack I would build

Each layer below is given as: its decisions; its model; the invariant it hands on; why that makes the next layer feasible. Variants are enumerated in L1 only.

**L0 Topology** (planar_topology; keep).
- *Decides:* lines, nodes, the crossing set, event order.
- *Model:* rules.
- *Invariant:*
  - the line map validates (V-E+F=2, simple faces);
  - every must-cross pair is crossed once;
  - events are totally ordered per line.
- *Why L1 is feasible:* L1 expands a valid map by local operations.
- *Refusal:* only when the topology is authored (D343).

**L1 Skeleton and track map** (planar_channels plus TM1 in planar_regions; keep, and make it the single source).
- *Decides:*
  - over or under at every crossing; inside a join, one form bit per site (D347b), not one per crossing;
  - the straight leg and side at every split and merge;
  - triangle orders;
  - doubling (D331/D332), copies (D351/D322, one per crossed joiner);
  - rows (D349).
- *Model:* rules, plus the variant search, which lives here alone.
- *Invariant:*
  - one validated track-level planar map;
  - sides per stretch, with no conflict (today's tests, planar_tracks.cpp:79);
  - **new:** for every over track, the crossers it must span, and for every corridor, its band count.
- *Why L2 is feasible:* L2 needs only node and crossing kinds and these counts.

**L2 Shape** (`SolvePlanarShape`, extended from channels to track stretches: TM3).
- *Decides:*
  - every stretch's headings;
  - the piece each crossing sits on, and from that the deck groups;
  - the crossing angles.
- *Model:* a small ILP:
  - absolute headings (so face turn sums close by themselves);
  - crossing rules; node rules (one leg keeps the heading, the other moves by one eighth); ports; no U-turn;
  - **new capacity rows:** a deck group's crossers fit within the longest middle at the chosen angle. The width per crosser comes from the existing `Cross`/`Deck` head rows (planar_model.h:498); no new constant.
  - It makes no jogs: no sideways moves (D340, D360).
- *Invariant:* every deck spans its crossers within the bridge limit, and headings change only for net turning and crossing rules.
- *Why the next layers are feasible:* L3 never fails (below). L5 cannot hit deck max, because the span was checked here at the angle L5 will build. This kills the f0 class before any solve.

**L3 Sideways position** (planar_slots; rework).
- *Decides:*
  - each track's integer slot in every interval of its frames (channels, node groups, port-side corridors);
  - hence every S-bend: its size, its sign, the interval it lies in;
  - which fork leg and which merge leg stays straight;
  - every close-up and fan at a pin.
- *Model:* min-cost tension LP, a network matrix, so integral, in milliseconds.
  - Hard rows: neighbours at least 1 apart; a copy exactly 1 from its line; a node's legs on its slot at the node; port pins (rank inside a port; the inter-port gap on a side, today outside slots: RECON-sideways-sites row 23).
  - Soft: charged steps.
  - It is always feasible.
  - The hand facts retire, because they fall out of track order (15.5's claim; test it).
- *Invariant:*
  - positions are consistent around every cycle a pin closes;
  - every step lies in an interval where L5 can build it at fixed length: an axis S-bend of diagonal 2|Δ|, or an axis excursion on a diagonal;
  - no step lies inside a deck or a parallelogram's exact pieces.
- *Why L5 is feasible:* inside frames, the coordinates across travel become constants. Only the lengths along travel and the gaps between frames stay free, and those are bounded only from below.

**L4 Phase** (new; replaces the ladders' re-read loop).
- *Decides:* for each face between frames whose walls approach again (the median between E and B; the face between E2a and E2b), which breakpoints face which.
- *Model:* rules where the face is turn-regular; elsewhere, one read of an LP relaxation of L5's own rows, with clash rows off. That read is a one-shot decision, not a loop.
- *Invariant:* every pair of stretches that bound a common face has its separation rows, with directions from the per-stretch sides (TM1). Nothing else can clash, because faces are corridors (§6).
- *Why L5 is feasible:* only in its LP relaxation. Integrality is the one residual risk, and it is already measurable: `PLANAR_IIS` prints "integer-only (LP feasible)". This is the only hand-off where I can offer a measurement, not a proof.

**L5 Lengths** (planar_model plus a builder that reads L2, L3 and L4; drop the optional-segment machinery: `opt_group`, `OPTMAX`).
- *Decides:* piece lengths, bend positions, deck spans, coverage extent.
- *Model:* one MILP with no binaries and no lazy rows; area linearised (D329). Choosing units in which parity is implicit (REVIEW-sideways Q3) could make it near-integral.
- *Invariant handed to signals:*
  - every row track has coverage of at least 2n_prio+4 summed along its lane (TM5p's reach rule);
  - every row has at least ⌈coverage/SD⌉ plain axis touch spots, as rows of L5;
  - doubled lanes are equal.
- *Why L6 is feasible:* the spots exist by construction.

**L6 Signals and prio** (TM5).
- *Decides:* drag signals, and joins on known row/lane pairs.
- *Model:* a deterministic walk.
- The checker becomes an oracle only (D193, settled).

**Terrain** stays after L5, with flat seed then terrain facts. Note it as the next generate-and-check site to design out (D218).

**Where variants are enumerated.**
- In L1 only.
- They are ranked by L2 turning plus L3 step cost plus an LP bound from L5 (§10.1), and the MILP runs for the top one to three only.
- Because no lower layer can fail, no variant is ever solved to discover that it is infeasible.
- Local search runs over these cheap scores, not over MILPs.

**Speed and scale.**
- L0 to L3 run in milliseconds today: slots in a few ms (planar CLAUDE), TM1 under 20 ms (STATUS), the shape program in ms.
- L5 with the choices fixed takes 15 ms (LL²) to 0.38 s (T3) (DESIGN-components §8).
- So LL³ in 5 s is plausible with one or two MILPs. The unmeasured risk is MILP integrality time at LL³ size.
- Variant bits stay at channel level (§14). The multilevel lift becomes "same L1 decisions, wider L3".

**Modules mapped to layers:**

| Module | Layer | Verdict |
|---|---|---|
| planar_topology | L0 | keep |
| planar_channels (channels, sites, node_legs, shares) | L1 | keep |
| planar_regions: track map, faces, `Side` | L1 | keep, as the single source; `PlanarLineSides` folds into it |
| `ResolvePlanarSites` (planar_gaps) | L1 | keep, moved |
| `BuildPlanarGapSteps`, `PlanarMergeExitStep`, candidate steps | L3 | drop: they fall out of port-side frames. Keep their outputs as test oracles |
| `BuildPlanarPaths` | L2 | rework: keep the crossing rules; drop detours and fork jogs |
| `BuildPlanarLineHeadings` (G2) | L2 | rework per D360: node geometry only |
| planar_shape | L2 | rework: track stretches plus capacity rows |
| planar_slots | L3 | rework: frames over node groups and port-side corridors; rows; no hand facts |
| `LadderRungs` | L4 | keep the rung rows; drop the re-read |
| planar_model | L5 | keep; drop optional segments |
| `MappedComposer`, `Fray`/`Join`/`Fork`/`Flex`/`Bundle` | none | drop at the D357 gates; it is the benchmark until then |
| hand `Composer`, `Block227`, the by-name branch | none | drop from the product path now (D326, D330) |
| `SolveLazy` | none | test oracle only: assert no clashes on L5's solution |
| variant search (pc:3105-3198) | L1 | rework onto cheap scores |
| signal-pass prio search | L6 | rework (TM5) |

## 6. Probe branch: keep or drop

wt-g3b (87a7e135fa, 25ce5fd099, cfdd91a517) is the first three commits of probe-g3e, with nothing beyond them. Harvest it, then delete it.

| Commit | Mechanism | Verdict against §5 |
|---|---|---|
| 87a7e135fa | TrackComposer: one strand per line, one segment per heading, decks per over piece | keep the idea as L5's builder; rework it to read L2/L3 |
| 25ce5fd099 | D359 port steps; decks per run of crossings | drop the steps (sideways moves outside L3). Keep the deck grouping, made an L2 output |
| cfdd91a517 | copies as D351 parallelograms in series; shared `PlanarPairDecks` | keep (ruled geometry); take copy side and range from the map only |
| fd86d4db3d, e824c9891b | S-bends from slots; bends where an interval changes with no crossing | keep "L5 builds slot steps". Drop "link step minus join copies" (g3e:3103-3108), which patches something L3 does not model |
| 806c6c3b78, e57e1aa14c, 15bbd2abf5 | DUMPSEGS, NOCURVES, MAXH, SIDEDUMP | diagnostics only; keep DUMPSEGS if it is cheap |
| 77eddbe4bc | sides per stretch from the track map | keep the source, for L4's rung directions; drop its use in the lazy loop |
| 98ef7c11c0, 587ab90166 | G2 drop rule loosened twice | drop: under D360 the rule should not exist |
| 26556e185d, d5040c6f73 | probe test takes a hub and tag | keep as a harness |
| 70e9275628 | track map: one copy per crossed joiner (D322) | keep: it is an L1 fact; land it with a test |
| 27c44a5fad | a copy doubles per parent piece, in series | keep, but split copies by the L2 deck group, not inside the composer |
| 93d53a2890, f40d43c61c | FORKMASK, MERGEMASK | drop. What they showed: choosing legs alone cannot help, because the binding moves are the close-ups at pinned exits |

## 7. Verdict

**On the right track?** In the docs, mostly. In the code, no.
- Right:
  - the topology, faces and track map;
  - slots as a tension LP;
  - D340, D350, D360 and D361 as rulings.
- Wrong:
  - the control flow is still "decide by heuristic, solve, read the IIS, patch";
  - the one layer meant to own sideways moves cannot see across channels;
  - the shape has no invariant toward lengths;
  - separation is lazy;
  - the product path breaches D326.

**Critical path to end to end, checker clean, T2, T3 and LL², riskiest first:**
1. L3 frames: channels, node groups (15.5, generalised) and port-side corridors; excursions on diagonals; hand facts removed. Gate: the experiment below.
2. L2 capacity rows and deck groups (kills f0), and G2 cut to node geometry (D360).
3. L4/L5: no lazy rows. Gate: first-round clashes = 0 on every base-feasible variant, with the lazy loop as an assertion.
4. Rows as tracks with coverage and touch-spot rows (D349), and TM5. Gate: end to end, checker clean on all three hubs (D357).
5. Variant ranking on cheap scores. Time per hub against D297.
6. Do now, cheap and independent: remove the by-name branch from the product path.

**The cheapest experiment for the riskiest invariant** (L3 output plus L2 headings can always be built, with clash rows off). About a day, on a throwaway branch from probe-g3e:
1. G2 reduced to channel-path headings plus the 45° node legs.
2. D359 off; the masks off.
3. Slot steps built by the existing `PLANAR_SLOTBEND`.
4. Count `SolveLazy1`'s first solve, which has no clash rows ("base infeasible", pc:2202-2205), over all T2, T3 and LL² variants.

Today's slots give the red. Then add node-group frames, then port-side frames.

Pass: every variant that L1 and L3 accept is base feasible.

Every remaining IIS must name a relation that crosses frames; that names the next frame or capacity row. Expected order, a prediction to falsify:
- LL² merges fixed by node groups;
- T2 W-side G3b fixed by port-side frames;
- f0 fixed only by L2 capacity.

**What I think is wrong in the design or rulings:**
- **D359:** withdraw it. Replace it with port-side frames in L3, not with 15.5 alone (§4, gap 3).
- **§16.1 and §16.3, "TM2 only if needed":** wrong. Accounting for sideways position is the central invariant. STATUS already says "it is needed".
- **15.5 as drafted:**
  - it covers two-channel sites only;
  - it leaves out frames between channels;
  - it makes slots depend on headings.
- **D341**, scoring shapes by full solves, is generate-and-check. Room belongs in L2's capacity rows instead.
- **D345**, prio by search judged by the checker: it should retire once rows are tracks. Settle D193 in favour of the oracle.
- **D347b,** read as one bit per crossing, explodes at LL³. Rule it as one form bit per join, as §16.3 already says.
- **BUDGET = 15 s per variant** (pc:52) is three times D297's 5 s per seed. A search that solves variants by MILP cannot meet D297.
- **D339's re-read** is acceptable only as L4's one-shot decision.

**Suggested tests:**
1. **L3 soundness:** MappedComposer's solved LL² o17t0f1s0 layout satisfies the node-group and port-side frame rows. A known feasible layout must fit.
2. **L3 completeness:** the base-feasible count above, per hub.
3. **L2 capacity:** on T2 o1t0f0s0, the capacity row refuses B's single deck group over D and F, or picks pieces that split it. No MILP needed.
4. **Separation:** first-round clashes = 0 on every base-feasible variant, with ladders from L4 and the lazy loop as an assertion.
5. **Product path:** `PlanarSeedFor("T2")` reaches the generic solver.
