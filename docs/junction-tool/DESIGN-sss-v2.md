# DESIGN: SSS v2, the planar solver at bundle level

Plan of record from 2026-11-21 (D388). One document for the whole design: what each layer decides, what it promises, how the promises add up to a buildable hub, and where PSG 197 (the litmus test, D387) lands in it. The epics that build it are in PLAN-sss-v2.md. Rulings are cited by D-number; DECISIONS.md is the source of truth.

## 0. Why v2

SSS v1 (D362, D379) planned at track level: every lane, copy and prio row was its own track with its own faces, and one MILP (Slots) decided all lengths with keep-apart rows still to be designed (D381). Two days of Slots probes (G1-G2) never gave a clash-free drawing, and the work drifted into adding row families from clash counts, which D361 forbids. What v1 established and v2 keeps:

| Finding | Ruling | Kept as |
|---|---|---|
| Fixed octilinear headings cannot stretch across a cut without axis inserts in diagonals | D383 flex points | §3.2 flex marks on spine diagonals |
| A prio row is exactly its coverage long, and bends only with its lane | D384, D349 | §3.2 corridors, §3.3 exact rows |
| A beside track is its parent's offset curve (miter at every bend) | SPEC-slots-lengths §11 | §3.3 corridor rows |
| Faces with fixed headings fold; convex cells cannot | D385, H12 built (wt-cells 0506a740f2) | §3.2 cells, between bundles only |
| The tile grid is a mod-2 system solved before the MILP | D382 | §3.3 parity |
| Feasibility is a certificate problem, not a runtime retry | D386 amended by D388 (b) | §4 test oracle |

The independent review (FABLE-architecture-cells.md) found the track-level cells spent on faces coop treats as rigid (lane/lane, lane/row, separator/copy, crossing lattice), and nothing tying a channel's lanes together (F7). v2 returns to the engineer's D310 ("components run inside bundles") with SSS's by-construction guarantee in place of the old composers' clash loops.

## 1. Principles (binding)
- **Valid by construction (D361):** each layer's output satisfies every constraint it owns; no generate-and-check, no lazy clash cuts, no runtime retry.
- **Information flows down (D366); each layer refines and owns its invariants, tested at its output (D367).**
- **Outside-in (D368):** a layer is designed and built only on the settled hand-off of the layer above; real code, not probes, carries the design (engineer, 2026-11-21).
- **Proof first:** a layer's guarantee is stated and argued before its rows are built; clash and IIS counts are oracles, never the design loop.
- **Any hub at scale (D326):** no per-hub code; components are atomic primitives (D330) composed by rule.
- **Litmus (D387):** PSG 197 BBH01 (V453000) must be representable and buildable; architecture choices are judged on it.

## 2. Vocabulary
- **Track:** one physical line of rail in the map (a lane, a copy, a prio row, a leg).
- **Channel (bundle):** tracks that travel together with the same start, destination and crossings (planar CLAUDE "Channel"); v2 extends this to partial crossings (§3.1).
- **Spine:** a channel's shared heading list; its lanes are offset curves of the spine at whole lanes.
- **Gadget:** an instance of a catalogue component (DESIGN-components C2-C9: lane split, fray, join, prio row, doubling, crossing stack, order swap, arm end) at a site or crossing block, with a fixed internal shape per lane count and parameters, and ports where spines attach.
- **Corridor:** a face inside a bundle (between two lanes, a lane and its row, a separator and its copy, or inside a crossing lattice). Rigid by rule: its width is exact.
- **Free face:** a face between bundles (bounded by spines' outer lanes, gadget hulls and the box).
- **Cell, dummy:** a free face is split by dummy edges from its reflex bends into convex cells (H12).
- **Exact quantities:** port pins and side gaps, lane offsets, deck gaps and spans, D384's prio length, gadget internal sizes. **Rigid zone:** a region whose sizes are all exact.

## 3. The layers

### 3.1 Structure (combinatorial; SPEC-structure.md)
**Decides:** the track map from the topology (tracks, sites, crossing order where not pinned, D344), channels, structures (crossings sharing a bridge or tunnel, separator and kind, D364, D369), allowed kinds, variants and their ranking (D366).
**New in v2 (PSG 197's blockers, D388 c):**
1. **Authored crossings** (D343, D344): the topology text can pin a crossing between two paths, including same-route crossings (psg197-gaps §1-2).
2. **Tunnel over tunnel:** a tunnel structure may cross another tunnel's span at a different level (197: tunnels #23, #26 over #3, #4); the levels are a Structure decision handed to Heights.
3. **A portal on another tunnel's middle tile** (197).
4. **Prio per merge,** not per route (psg197-gaps §9).
5. **Partial crossings of a bundle:** a crosser that crosses some lanes of a channel and not others splits the channel's crossing set per lane (197: F3, F3b over A1c only); channel identity follows per-lane crosser sets.
6. **Lane counts that change inside the hub** (3 -> 2 -> 3, psg197-gaps §4): already expressible as merges and splits; verify.
**Hands down (D366), replacing every probe stand-in:** rule fields per variant: `limit`, `side_gap` (built), and the lower bounds and exact values of SPEC-slots-lengths §8.1 (piece, clear, cross, deck gap, landing, node leg, curve window, bay, coverage as an exact value per D384).
**Promise:** P1-P8 (SPEC-structure §4) plus the new items above as tests at its output.

### 3.2 Shape (headings, gadgets, cells; SPEC-shape.md)
**Decides:**
1. **One heading list per channel (spine)** (D388 a, review F7): every lane of a channel shares it; lanes are offset curves at whole lanes. Cusps and fans that open or close a bundle belong to the gadget at that site.
2. **Gadget instances** at every site and crossing block: which catalogue component, its lane count, its orientation (headings at its ports), its parameters. A gadget's internal tile geometry, including the coop tile sharing (joins' half pieces D342, D374; D353 diagonal halves; tridents D110), is part of the catalogue, verified once per lane count against the real tile clash rule. No clearance exemptions are written anywhere else.
3. **Crossing blocks:** a structure's crossers under one span with their angles (H11 per run of crossers, not per span, review F8); the crossing lattice is inside the crossing-stack gadget.
4. **Flex marks** (D383): on spine diagonals outside rigid zones.
5. **Cells on free faces only** (D385 as amended by D388): dummy edges by the H12b rule (d order h, h-3, h-1, h-2; first facing wall; 4+-wall cells preferred); dummies may land only on physical walls (spine outer lanes, gadget hull walls, box), never on another dummy (review F5).
**Promise:** H1-H11 (H11 per run), H12 (free-face cells convex), plus: H14 every channel's lanes follow one spine; H15 every gadget is a catalogue instance with legal parameters; H16 every dummy lands on a physical wall.
**Parked:** D385 (ii) bend pairs at track level. Inside bundles, steps belong to gadgets (19% of coop steps sit next to splits, RECON-corpus-steps); between bundles, the need is re-measured on free faces before any design (review F6).

### 3.3 Slots (lengths; SPEC-slots-lengths.md, to be rewritten for v2)
**Decides:** spine part lengths, flex insert lengths, gadget parameters within their ranges and anchor positions, dummy landing points, and hence every point; the box.
**Rows (complete, read from Structure's and Shape's output before the solve):**
- Closure per free cell; landing of each dummy on its named wall (point within the wall).
- Corridors: lanes as offset curves of their spine (miter rows at every bend, flex inserts shared, SPEC-slots-lengths §11).
- Gadget ports: a spine attaches at its gadget's port by exact offsets.
- Clearance between facing walls of each free cell (linear, order fixed by convexity), including walls that are dummies' neighbours (review F4: the rows continue across a dummy's two cells).
- Exact quantities and lower bounds from Structure's rule fields; D384's prio length exact.
- Grid parity by D382 (forced parities before the solve, unit switches).
**Objective:** area, then turning, then a stated order (G3).
**No binaries for keep-apart; no S-bend binaries between bundles** until the re-measure of §3.2 says otherwise.

### 3.4 Heights and Signals
Unchanged from D365 (Heights after Slots on the fixed footprint, terraform always feasible, tunnel levels from Structure) and the Signals walk (checker as oracle, D363). PSG 197 adds: tunnel-over-tunnel levels (from Structure) and a pre-signal flag on a split for choice by free tunnel (psg197-gaps §3, a Signals matter).

## 4. The guarantee
**G-planar (argument):** inside a bundle, lanes are offset curves at whole lanes: parallel tracks a lane apart cannot meet. Inside a gadget, the shape is a verified catalogue instance. Between bundles, free faces are cut into convex cells; a convex cell with its walls' lengths >= 0 closing cannot self-intersect; cells tile their face, faces tile the box; clearance rows keep facing walls of each cell apart. Bundles and gadgets meet only at ports. Hence no two tracks share a tile except inside a gadget, where the catalogue allows it. Open points to close in the epic that builds it: SPEC-shape §8.3 (a)-(d) restated for free faces.
**G-feasible (test oracle, D388 b):** the full feasibility program (all Slots rows, integer, forced parities, no objective) runs in tests over T2, T3, LL2, PSG 197 and the corpus as the oracle of Structure's and Shape's promises. A failure names the missing promise (the infeasible rows); the fix goes upstream. At runtime Slots solves; a runtime failure is a bug report, never a retry.

## 5. PSG 197 in v2 (from the review and ANATOMY-psg197-bbh.md)
| 197 feature | Where v2 handles it |
|---|---|
| Per-output-lane splits before the tunnels | cascaded lane-split gadgets (C2) at sites; short bundles |
| Parallel tunnel groups (29: 15 pairs, 3 triples), different lengths, exits at their joins | crossing-stack gadget per group with per-lane crosser sets (§3.1 item 5); tunnel length = span from its crossers plus end runs; exit-to-join stretch free in Slots |
| Tunnels in series per line | structures in series with landings (D375) |
| Tunnel over tunnel; portal on a tunnel's middle tile | Structure items 2-3; levels to Heights |
| 3 -> 2 merger after a direction split | cascaded two-input joins (C4) with prio per merge |
| Tunnels up to 15 tiles | `limit` (max_tunnel_length) |
| Terraced portals (tunnel longer than its crossers need) | not representable without terrain seeds (D365 defers); accepted gap |
| Size (~101 lines, 62 node statements, 50 tunnels) | the speed risk: Shape is 8.8 s on LL2 today; bundle level must cut the variable count; measured at the E3 gate |

## 6. What carries over, what is superseded
- **Carries over (code):** Structure (channels, sites, track map, structures, `limit`, `side_gap`), Shape's heading program and oracle H1-H11, H12 cells (`PlanarShapeDummy`, `RefinePlanarShapeCells`, `CheckPlanarShapeCells`, branch wt-cells), D382's parity machinery (probe; to be ported).
- **Superseded:** track-level cells everywhere (D385 scope), the runtime certificate (D386 (1)), bend pairs at track level (parked), the Slots probe as a design tool (planar_slots_probe.cpp stays as diagnosis only), D379's "Slots decides every track's sideways position" (now: lanes follow spines; gadgets fix internal positions).
- **Docs to align (review F9):** SPEC-shape §8 (scope), SPEC-slots-lengths (rewrite for v2 rows), planar CLAUDE "Architecture".

## 7. Open questions (each settled in its epic, by design + card before code)
1. Gadget parametrisation: which internal lengths are free (e.g. a fray's split gaps are lower bounds, D355) and how a gadget's hull is described to the cells.
2. Partial bundle crossings: the channel rule per lane crosser set, and the crossing-stack gadget's per-lane form.
3. Steps between bundles: are any needed on free faces once gadgets own fans and cusps (re-measure, then design).
4. Tunnels as a second layer: whether a tunnel's body cuts surface faces (today it does, as a crossing event); a deliberate Structure/Shape decision, measured on 197.
5. Speed: Shape at 197's size; the variable count at bundle level.
6. Parity inside corridors and gadgets (offsets at whole lanes keep the grid; verify per gadget).
