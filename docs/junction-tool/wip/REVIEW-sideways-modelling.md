# REVIEW-sideways-modelling: steer on sideways position and S-bends (docs at 51a9b64a53, code at c829db7742)

**Verdict.** No established framework makes S-bends free. Exact area-optimal dogleg routing is NP-complete (Szymanski 1985, "Dogleg channel routing is NP-complete"). VLSI jogs come free only because its wires have no curve-spacing rules. What the fields do have, and we lack, is a separate **track-assignment layer** between shape and lengths: every line gets a slot (its rank across travel) per corridor interval. Once the order is fixed, slots form a difference-constraint system with an |Δslot| objective. That is a min-cost tension problem, the dual of min-cost flow, so its LP is integral and S-bends come out of it as results, not yes/no variables. DESIGN-tracks §3a already describes this layer. Two gaps keep it from paying off. First, the code still builds every sideways step inside Fray, Join and Double. Second, §3a's rule "packed in order from the pinned side" is the greedy left-edge rule. That rule has no slack for steps the room does not force, so those steps fall back to variant bits (§3a "candidate steps").

## Q1. Frameworks that fit
1. **Layered-drawing coordinate assignment** (Gansner, Koutsofios, North, Vo 1993, "A technique for drawing directed graphs"; Brandes and Köpf 2001). This is the closest fit. A layer is a corridor cut at a topological event (node, crossing, component end). The vertex order in a layer is our slot order, already fixed by the topology. The x-coordinate is sideways position: minimise Σω|x_u − x_v| subject to x_b − x_a ≥ ρ, solved by network simplex, so the result is integral. A bend in a long edge is an S-bend. A copy or a row is one more vertex in the layer (D333, D349). Brandes-Köpf's at most two bends per long edge means at most one S-bend per lane per corridor.
2. **Ordered bundles / metro-line ordering** (Benkert, Nöllenburg, Uno, Wolff 2007; Pupyrev, Nachmanson, Bereg, Holroyd 2011, "Edge routing with ordered bundles"; Bast, Brosi, Storandt 2018, LOOM). An edge is a channel stretch and its line order is our slot order. Each line sits at rank × spacing; LOOM's "separation" cost (neighbours in one edge parting at the next) is an S-bend. This is §3a as written.
3. **Channel routing and track assignment** (Hashimoto and Stevens 1971, left-edge; Deutsch 1976, dogleg router; Yoshimura and Kuh 1982; Batterywala et al. 2002, "Track assignment: a desirable intermediate step"). A net's trunk is a lane, a track is a slot, the channel density is D327's width profile, the vertical constraint graph is the topology order, and a dogleg is an S-bend. Today's prio stagger, where lane k's row and lane k+1's copy share one line on different stretches, is left-edge packing: two intervals in one track. D349 gives each its own track.
4. **River routing** (Tompa 1981; Leiserson and Pinter 1983): one layer, fixed order, no crossings. This models one corridor interval. Different end offsets force jogs, and the length a jog needs has a closed form. That gives the length solve a constant required length per |Δslot|.
5. **Topology-shape-metrics** (Tamassia 1987; Bridgeman et al. 2000, turn-regularity; Klau and Mutzel 1999). This fits turning (D340) and clash sides, not S-bends. A min-cost-flow orthogonaliser never makes a zero-net bend pair, because the two flow units cancel. `SolvePlanarShape` costs |turn| per step (planar_shape.cpp:122-131), so for the same reason it never emits an S-bend. S-bends cannot come from the shape layer.
6. **Metro-map MILP** (Nöllenburg and Wolff 2011) is our layer 4. It draws the lines inside an edge by rank afterwards, outside the MIP.

## Q2. Forced versus area steps
- **The split can be computed.** Write the slot system as difference rows: neighbours at least their widths apart, ports pinned, the legs of a node tied. A slot change of lane i in interval e is forced exactly when fixing Δ = 0 makes a negative cycle (Bellman-Ford, microseconds per test). ANALYSIS-merge-steps §2 shows EO2 and WO2 both on and off in solved layouts, so by this test they are area steps.
- **Area steps go in the objective of the same integral LP:** Σ w·|Δ| (w: an S-bend's curve pieces) + Σ λ_C·width_C, with λ_C taken as corridor C's length from the rough solve (§10). This needs no bits. The greedy alternative, packing from the pinned side, is what produced the 82.5% merge rule (ANALYSIS §3).
- **Costs:** L1 counts step size, not step count (fine for ±1-slot steps); λ is a proxy, so expect a re-read loop like the ladders'; not exact (Szymanski).
- **This differs from D327's rejected "plain lengths" (5-11% area).** That approach left every possible step in the length solve with curve rows always on. Here the slot pass decides which steps exist, so the length solve has exact curves and no binaries.
- **Speculation: ties may fold in.** S4 recon (§12) says a tie changes core offsets, gap steps and the exit joiner side. Those are slot facts, so "which leg keeps its slot" is an |Δ| choice. The lane order inside a fray may not be.

## Q3. Keeping the LP tight
- **Fixing the discrete choices is the measured lever:** LL² 15 ms and T3 0.22-0.38 s with choices fixed, against 12-36 s (D327).
- **Structure.** An axis segment is a difference row in one axis only (planar_model.h:414-415), so each axis on its own is totally unimodular. Two things break this:
  - a free-length diagonal shares one L between x and y;
  - the parity rows x = 2k + p (planar_model.h:409-411, 572-575) are lattice constraints that the LP ignores.

  If slots fix every S-bend diagonal at 2|Δ|, those lengths become constants. Only the free diagonals of turning stretches, and `Cross`'s parameter b on a diagonal under segment (:482-483), still couple x and y. HYPOTHESIS for 66 ms against 435 ms: the program shape has more free diagonal stretches.
- **Natural units:** sideways position in slots and axis lengths in tiles make most parity rows implicit.
- **Weak big-M:** `AddSeg` still uses OPTMAX when a segment has no upper bound (planar_model.h:401, 406). Today only fork legs reach it. Use a geometric upper bound (§8.4).

## Q4. Layering
- **Insert a slot layer:** topology → shape (headings) → slots per corridor interval (lanes, copies, rows, joiners, legs) → components placed on slots → lengths. This mirrors global routing → track assignment → detailed routing. §11 names only the two outer stages.
- **Components become cells with pin slots.** They keep their fixed coop cross-sections but no longer make sideways steps.
- **Keep the ladders:** they are VLSI 1D compaction (rows read off the current layout, longest paths; Liao and Wong 1983) on octilinear geometry.
- **Where S-bends belong:** not in the shape layer (Q1.5) and not in the length MILP. Ties belong in the slot layer.

## Q5. Where the docs are ahead of the code
- §3a ("nothing else creates one"), against these places that still make sideways steps:
  - Double moves the lane one slot (planar_components.cpp:904-910);
  - the prio Join's close-up diagonal is in [2, 2j] (:1020-1021);
  - exit steps come from a rule (:1025-1029), and a left step is refused (:977-979);
  - the prio fan (:940) and Flex's jog (:1054).
- §2.3 (blocks as role-tagged constraint contributors), against copies recorded by index (`CopyRec` d0.i to d0.i + 2, :911).
- §13.1 and D349 (the row is a real track), against the reserved `prio_rows` (:1018; `PrioRow` "a parallel unused track", planar_model.h:197-203). In a slot layer a row is one more rank.
- §8.2 (clash loop retired), against the loop still running in every solve (planar/CLAUDE.md, Line sides).
- Width-profile binaries are already fixed by their bounds (planar_model.h:391-395).
- planar/CLAUDE.md is stale: the first Shape program bullet ("nothing in the solve calls it yet") contradicts its third bullet and D341.

## Suggested tests
1. Forcedness: build the slot difference system for each LL² (over, flip) group and classify the EO2, WO2, SO2 and AFj1 steps as forced or free; compare with ANALYSIS §2.
2. Run the tension LP against ANALYSIS's 672 old choices: agreement rate, and area after the length solve.
3. Root fractionality on the 435 ms variants: count free diagonals and the parity rows the root LP violates, default shape against program shape.
4. D349 two-lane prio join: with rows as ranks, check that the tension LP's S-bends match the widened bundle the ruling describes.
