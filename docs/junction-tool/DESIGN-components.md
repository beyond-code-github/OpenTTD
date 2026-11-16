# DESIGN-components: coop components inside bundles (D310, D311)

Orchestrator draft, 2026-11-16. It replaces DESIGN-planar §2a's free track placement (a departure I failed to flag) with the engineer's model: **bundles are the skeleton, and components run inside them** (engineer, 2026-11-16: "bundles/channels don't connect components, but the components run inside them"; "one component may be two tracks wide (i.e a prio) so the channel width at any given time can vary"). It builds on D305-D307 (channels, order not distance), D305's "blocks become named groups of these nodes (templates as presets)", and DESIGN-blocks (block programs, rule families R1-R9).

## 1. Model
- **Bundle.** A channel of the topology (D305/D306): an ordered set of tracks that run together. It has a path (a sequence of headings and bends, shared by all its tracks) and a **width profile**: the number of lines in use at each point along it. Tracks keep their order except where a component swaps them.
- **Lines.** Across a bundle, positions are integer lines, one tile apart, 0 on the left of travel (as in DESIGN-blocks). A track sits on one line at each point. A component may add lines (a prio row, a doubled deck, a fray widening) or remove them (a join), so the width varies along the bundle.
- **Component.** A stretch along one bundle (or the meeting of bundles) with a **fixed internal shape**: which lines it touches, its S-bends and nodes, written as a block program (DESIGN-blocks §1: points along the heading, lines across, segments Run / SBend / Bridge). Its length comes from the rule floors (R1-R9) and stretches only where a floor allows.
- **Layout.** The skeleton (bundle paths), each bundle's components in order along it (from the topology's event order), each component's variant, and the positions. The solver never invents track shapes outside components and plain bundle runs.

## 2. Catalogue
Pictures head east (travel left to right); line 0 is north. `=` is a plain bundle track, `/` and `\` are S-bend pieces, `H` a bridge head, `P` a prio row.

### C1 Run (plain bundle stretch)
```
0 ==========
1 ==========
```
- **Lines:** all of the bundle's, unchanged.
- **Length:** any, at least 0.
- **Bends:** the whole bundle turns together (all lines share the heading sequence; curve gaps per track, R8).
- **Topology:** every channel between events.

### C2 Lane split (a track divides onto a new line)
```
0 ==========
1 ===\======   (the diverging leg steps one line out)
2     \=====
```
- **Lines:** n in, n+1 out. Variants: diverging left or right.
- **Length:** signal room before and after (R2, R6), then the S-bend.
- **Topology:** a 1→2 node whose two legs stay in the same bundle (e.g. a choice split, `E2 -> E2a, E2b`).

### C3 Fray (a bundle divides into bundles by destination)
```
0 ===============        bundle A (out)
1 ====\==========
2      \===H=====        (order swaps where tracks must cross)
3 =====\===H=====        bundle B (out)
4       \========
```
- **Lines:** n in, n_A + n_B out, with the gap the next stretch needs.
- **Internals:** a lane split per input track (C2), then any order swaps (C8) so each destination's tracks are adjacent; the exit block (DESIGN-blocks §2.2) is the two-lane instance.
- **Variants:** which leg is straight per split; swap forms (C8).
- **Topology:** a node group whose out-vector splits by destination (`node EF1, EF2 -> E1, F1, E2, F2`).

### C4 Join (bundles converge, tracks merge into a lane)
Variants seen in coop:
- **227 join block** (DESIGN-blocks §2): two bundles in (fixed + choice track each), choice tracks split and cross, two lanes out, each with a prio row (C5) on its fixed track. The two-lane instance.
- **3-into-1** (real PSG 227, ANATOMY reading 3): fixed lane plus two choice lanes merge on one tile or adjacent tiles.
- **2+2 cascade** (real PSG 227): the two choice lanes merge first, then the merged track joins the fixed lane.
- **Lines:** n_A + n_B in, the out bundle's width out (plus prio rows while they run).
- **Topology:** a node group whose in-vector merges (`node E1, EDj1 -> EO1`), with the choice splits and joins of its routes (LL² `E2 -> E2a, E2b`, `E2a, D1a -> EDj1`).

### C5 Prio row (a second track beside the fixed track before its merge)
```
0     PPPPPPPP        prio row (adds a line for P tiles)
1 ==============X==   fixed track, merge at X
2 =============/      joining track
```
- **Lines:** +1 for the prio coverage P (`spec.prio_tiles`), on the side away from the joining track (DESIGN-blocks R9: no node or bend on the fixed track from mL - P - 1 to mL; the prio line holds nothing else).
- **Variants:** our prio element (D282/D287); the real hub's other styles (two-way, path signals) are later.
- **Topology:** a merge of a prio route (D301) with a non-prio route.

### C6 Doubling (an over track splits to two decks for a crossing)
```
0 ===+==H====H\            copy 0: straight through its bridge, S-bend after it
1     \H====H==+===\       copy 1: S-bend before its bridge, heads one tile upstream; merge on line 1
2                    \===  (shift back when safe)
```
- **Form: the parallelogram** (D105, D118, D145, D287; engineer 2026-11-16 "make sure our doubled bridges are the parallelogram style"): no gap line between the decks, each copy bends once, the copies merge on the copy line and the track shifts back when safe.
- **Lines:** +1 over the doubled span; equal lengths and bends on both copies (D80 line sync).
- **When:** the over track carries a prio or through route (D80, D188); not for choice legs (D299).
- **Topology:** none; a layout realisation of a crossing (D309).

### C7 Crossing stack (where two bundles cross)
```
        B (under, heading south)
        | |
A  =====H=H=====      every A track bridges B's width with one deck
A  =====H=H=====      decks side by side, heads aligned (staggered when doubled)
        | |
```
- **Forms:** square (bundles perpendicular) or S-bend (the under bundle crosses on a diagonal); which bundle is over (cost; D156/D164: main-line bridging allowed and often best).
- **Decks:** one deck per over track spans all the under tracks it crosses (merged decks, D203 (2)); doubled per C6 when required.
- **Topology:** a channel crossing (D307), replacing |A| x |B| line crossings.

### C8 Order swap (two adjacent tracks of one bundle exchange lines)
```
0 =====\  H=====
1 =====H\=======      one track bridges its neighbour while stepping across
```
- **Forms:** bridge over the neighbour; the real hub's 2x2 diagonal cross ("direction cross", 4 in / 2 out on one 2x2) where a split and the swap share the tiles.
- **Topology:** a crossing between two tracks of the same channel (`F1 x E2` in the T and LL² frays).

### C9 Arm end (a bundle meets the boundary)
- **Lines:** the leg's line profile (D38 notation, e.g. LL_RR) at the box edge; port stub R1.
- **Topology:** a port (one direction's tracks); the two directions of a leg as one corridor is deferred (engineer: "cross-directional bundles later if needs be").

## 3. From the topology to components (derived, not authored)
- Channels from node groups and ports (D305/D306), with each track's slot.
- Each split group: C3 fray if its outs go to different destinations, else C2 lane splits.
- Each merge group: C4 join; its variant is a choice.
- Each merge of a prio route with a non-prio route: C5 prio row inside the join.
- Each crossing between two channels: one C7 crossing stack; its line crossings are its decks. A crossing within one channel: C8.
- Each C7 whose over bundle carries prio or through routes: C6 doubling inside the over bundle.
- Order of components along a bundle: the topology's event order.

## 4. What the solver decides
- **Skeleton:** each bundle's heading sequence and bends (one per bundle, not per track); node-group orientation follows from the bundles meeting there (no invented node heading).
- **Variants:** straight leg per split, join variant, C7 form and over bundle, C8 form.
- **Positions:** points along each bundle (component starts and stretches) and bundle offsets, as an LP with the rule floors; components keep their internal shapes. The parity-and-dive solve and the ring sides (probe 5) carry over.
- **Cost:** the agreed objective (DESIGN-planar: build cost plus compactness, D114); judged against real hubs by the engineer's eye.

## 5. Questions for the engineer
1. Is the catalogue complete for the T and LL² (C1-C9)? What is missing or wrong in the pictures?
2. C4: should the first version build the 227 join block as one component (as DESIGN-blocks has it), or 3-into-1 and 2+2 cascades as separate variants?
3. C5: is the prio row always on the side away from the joining track?
4. C8: is the 2x2 diagonal cross a variant we want from the start, or bridge-over-neighbour first?
5. C3/C4 internals: should a fray and a join be fixed block programs per lane count (like DESIGN-blocks), or composed from C2/C8/C5 inside a stretch?

**Rulings (D311):** (1) C1-C9 stand. (2) One Join and one Fray type, composed inside their stretch from the primitives (C2 lane split/merge, C5 prio row, C8 order swap, C7 crossing stack), so width follows from the tracks (orchestrator default from the engineer's "fewer block types where possible especially where it affects width"; toggle candidate). (3) Prio row on the side away from the joining track. (4) C8 is bridge-over-neighbour only for now.

## 6. C++ port (D321; orchestrator, 2026-11-16)
The executable spec is `wip/planar-probe8.py` (one generator for T2 and LL²). The port translates it; it does not redesign it.
- **Where.** `src/junction/planar/planar_components.{h,cpp}`. Public: `PlanarProgramPiece`, `PlanarRouteProgram`, `PlanarComponentLayout`, `SolvePlanarComponents(topology, spec, layout, why)`. Everything else (linear expressions, the model of strands/segments/optional segments/nodes/crossings/decks/copies/prio rows, the compositions, curve rows, the lazy clash loop, the area linearisation, the program export) is file-local.
- **Compositions.** By topology name: `T2` (`PlanarTText(2)`) and `LL2` (`PlanarLL2Text()`); strand names are the topology's line names (copies add `"`, `'`, `''`; prio rows `prio:<line>`). Any other topology: no composition (the caller keeps the §2a solver).
- **Variants.** LL²: S fray side × diagonal west fray (4); T2: S fray side × S join side, F over D, flipped crossing triangle (4). The other crossing triangle and D over F never come within 1.5× of the best (probe 8, all 12 T2 variants) and are not built. Variants solve in parallel (one thread each; HiGHS single-threaded per solve); the least area wins, ties by variant order.
- **Spec.** TL = `spec.tl`; CL = `DeriveCurveSpacing(spec.reference_speed, spec.curve_speed, spec.tl)`; P = `spec.prio_tiles`; G = min(2TL-1, max(2, CL)).
- **Solver.** One fresh `JunctionMilp` per solve (rows are only ever added); every variable integer as in the probe; parity by an integer auxiliary per axis coordinate; each solve warm-started (`SetStart`) from the model's last solution; a per-solve time limit of 5 s (`JunctionMilp::SetTimeLimit`, new), a solve that hits it counts as infeasible, as in the probe.
- **Output.** Per route path and copy choice a `PlanarRouteProgram` (pieces with tile, heading 0 N .. 7 NW clockwise, kind, path line index; copy switches), the same content as the probe's `--routes` file. `RenderPlanarRoutePrograms` (planar_plan) turns them into routes, layout and signal network the way the hidden probe-hook test does (turn +1 is the rasteriser's L), so the existing signal pass, checker and level solve run unchanged.
- **Seed and hover.** `PlanarSeedFor` solves the components for T2 and LL² and caches them; the plan renders the cached programs (flat placement, levels solved after on terrain); the hover reuses them. Terrain facts in the component solve are later work.
- **Acceptance.** LL² area ≤ 807 and T2 ≤ 963 tiles (D320), clash-free, checker-clean through `RenderPlanarRoutePrograms` → `SignalJunctionTrackRoutes` → `CheckJunctionRules`; wall time per hub reported.

## 7. Next: LLL_RRR × LLL_RRR (draft, orchestrator, 2026-11-16; engineer: "LLL_RRR * LLL_RRR or similar is good to start thinking about")
The T family at three lanes per direction on main and branch (the old pipeline's D263 test case). Probes 6-8 compose T2 and LL² by hand, one function per hub; three lanes need the composition **derived from the topology** (§3), which §3 always intended.
- **Plan.** (1) Derive the composition from `PlanarTopology` for the T family at n lanes: one fray per in port (its bundle splits by destination: n lanes into per-destination bundles), one bundle per (in port, destination), one crossing stack per pair of crossing bundles (C7, the over bundle's prio lanes doubled, C6), one join per out port (C4: fixed lanes under their prio plus choice legs, merging first per D224), the D321 levers inside each. Acceptance: it reproduces T2 and LL² at no more than probe 8's areas (935, 800) before anything new. (2) Author the 3-lane topology text (`PlanarTText(3)`, and an LLL² text by pairing) and solve. (3) Check, rig, the engineer's eye.
- **What scales.** Variants: fray side per fray and the diagonal fray stay discrete (2-4 per fray); everything else is continuous or optional bends. The solve grows with the line count (LL² 18 lane paths; LLL² any-pairing many more). Probe first, at the 15 s budget per variant.
- **Questions for the engineer (when back).**
  1. Which pairing first at three lanes (D221: any, lane keeps lane, lane per direction)? Recommendation: lane keeps lane (the T3 analogue of T2) first: fewest paths, proves the derived composition; then any (LLL²).
  2. The join at three lanes under pairing any: the 227 block generalised (each out lane: its fixed lane under prio, the choice legs from the other bundle merging first, D224), or another PSG pattern you'd rather copy? Recommendation: 227 generalised.
  3. Tunnels (D220, "desirable especially when getting towards LLL_RRR"): the component model has no tunnels yet. Leave them out of the first 3-lane probe and see how tall the decks get? Recommendation: yes, out at first.

## 8. What the solve time is made of, and the channel direction (probe 10, 2026-11-16)
The engineer's bar (D326): the tool solves whatever hub the user picks, at scale. Hand compositions per hub (probes 6-10, and `SolvePlanarComponents` keyed by topology name) don't meet it, and neither does the solve time at three lanes: T3 took 12-36 s per variant, and LLL² didn't converge. Probe 10 measured where the time goes. Scripts and logs: `wip/probe10/`.

**The cost is yes/no choices, not constraints.** The program's continuous part (lengths and positions) solves in milliseconds. Branching on binaries, and re-solving for every clash found, is the cost.
- LL² (-1F1w1, 800 tiles), measured with the clash sides and bend choices taken from its own finished solution:
  - today, with the clash loop and area refinement: 4.6 s;
  - clash sides given up front: 0.65 s;
  - clash sides and optional-bend choices both given: **15 ms**;
  - the same as a pure LP: 13 ms.
- Integer lengths cost almost nothing once the choices are fixed. The parity rows are needed: without them the layout breaks.

**Clash sides come from the topology's order.** Every pair the clash loop had to separate sat between topology neighbours at the hub's edge: 5 on LL², 9 on T3. These were lanes of one in-port as they fan, a fray leg beside the next port lane, and an exit's joiner run beside the neighbouring in-port. None was inside the core, and the side picked always matched the left-to-right order at the ports and nodes. On T3, four rule groups given up front left no clash at all:
- in-port fan order (EF, AB, CD);
- E-side port order (D's run into ED stays north of AB).

Results per variant: 1930, 1949, 1956 and 1969 tiles at 7-14 s, with zero clash rounds. That beats the clash loop's best (1949), and all four variants fit the budget. A fifth rule I typed by hand for the S side was wrong, because F is the straight bundle there in that variant. Typed rules are fragile; they have to be read from the topology.

**Optional bends: continuous steps lose area; channel switches don't.**
- **Each optional step as a plain length ≥ 0, with no switch:** the curve-spacing rule must then hold for curves that may not exist, so a step's own two curves need an exemption. Result: 2142-2173 tiles at 0.4-2.5 s, 11% bigger. A second pass that drops the steps the first pass set to 0 gives 2036-2154 at 1-3 s, still 5% bigger.
- The game's curve rule is genuinely discontinuous: a sideways step has two curves or none. The engineer rejected special-case rules for curves ("I didn't like that we had to come up with special rules for some of the curves").
- **Width explains the steps.** In the best T3, 19 of the 41 optional steps are used. Each is a whole-channel jog plus the width change the lane order predicts:
  - W in-port fan: 14, 12, 10 half-tiles, a 5-line jog plus 1 line of spread per lane (the prio fray's 2-line spacing);
  - E in-port fan: 10, 8, 6;
  - E exit: 8, 8, 6;
  - the A, F and D bundle staircases: each farther lane crosses one more deck.

  The bundle S-bends, and the W and S exits' optional steps, are unused.
- **One switch per channel transition (13 on T3) instead of one per lane (41):** 1956-2002 tiles at 4-6.6 s. With those 13 switches fixed: 1956-2002 tiles at **0.22-0.38 s per variant** (including the area refinement), about 40x faster than today at the same area and 1% off the best found (1930).
  - On, because the width changes: the prio frays and the crossing staircases.
  - Off, because nothing changes width: the bundle S-bends and the non-prio S fray.
  - About two per hub are real choices: an exit's jog to line up with its port (the E exit on; the S exit on or off by variant). These become variants.
  - The switch states were read from the solved result and checked against the width reasoning, not yet derived from the topology.

**Direction (D327, the engineer's idea): channels with width profiles.** "Is there no way we can have sideways steps emerge organically? Surely we always know how wide a channel needs to be at any given point."
- A channel is an ordered set of lanes between topology events. At every point its track count and order come from the topology, and its components add known widths: a prio element 2, a doubled lane 2, deck heads.
- Each lane is the channel's line plus a fixed offset from the order, so lanes inside a channel can't clash by construction. That replaces the in-port fan rule and the per-hub side rules.
- A step exists exactly where the width changes, and the order says which lanes move and by how much. The solver only places each transition along its channel, as a continuous position and length. "Fan at the last responsible moment" then falls out of the area objective.
- Clashes between different channels take their side from the topology's order. Whether that covers every clash is untested; a couple of lazy rounds remain the safety net.
- Still discrete:
  - which edge of a channel absorbs a width change (D317 suggests a convention: the edge facing the neighbouring channel stays straight);
  - a whole channel's jog to line up with a port.

  Both are a handful per hub, enumerated as variants and solved in parallel at about 0.3 s each.
- This is §3's derived composition made concrete, with a solve that doesn't grow with the square of the track count. The component catalogue and the engineer's rulings on shapes (D310-D322) carry over as the transitions inside channels.

**Open before building:**
- the channel builder from `PlanarTopology` (channels from node groups and ports, width profiles, transitions);
- whether topology order settles every between-channel clash;
- the edge convention for width changes;
- LL² and T2 as acceptance fixtures alongside T3.
