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
