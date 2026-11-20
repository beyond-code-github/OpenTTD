# CARD E3-2: the inventory sorted: rules, not a catalogue (orchestrator, 2026-11-21)

Sorts wip/INVENTORY-gadgets.md (43 entries) by D400 (nodes are rules) into the layer that owns each. Supersedes CARD-E3-catalogue.md's data form if ruled.

## The sort
| Owner | Entries | What it is |
|---|---|---|
| **Node rule** (Shape H3 headings; the node tile's sharing and the cusp proven once per heading class and arity; Structure merges consecutive nodes) | A1 split, A2 merge, A3 diagonal-trunk stub, A4 trident, A5 trident into a doubled bridge, A6 three-input join on one tile (a 3->1 node), A7 2+2 cascade, A8 peel splits, A9 n-lane fray/join, C5 rejoin then split (D302) | nodes of arity 2 or 3 and cascades of them |
| **Structure rule** (Structure's structures and copies; Shape's span rules H5, H6, H11; tile rules of a span proven once) | C1 bridge crossing stack, C2 tunnel, C3 parallel tunnel group, C4 doubling (copy tracks, exact offsets), C6 series with landing, C7 crosser shapes under a span (Corner, S-bend, doubled pair: angle per run, D376 and D396), C8 order swap (a crossing inside one channel), C10 tunnel over tunnel, node on a tunnel, C11 three levels | spans, copies and their crossers |
| **Corridor rule** (offset curves inside a bundle: SPEC-slots §11, D349, D384) | B1 prio row, B2 bendy prio and diagonal halves, B3 rows of n lanes | the row as an exact-length beside track |
| **Signals** | B1's joins and signal pattern (placed by `PlacePrio` today), B6 entry-gated merge (D390 (4)), F5 choice by pre-signal | signal-level, after Slots |
| **Slots rows** | D1 port pins and stubs, D2 fan and close-up (D397 flex, D363), D3 waiting bay, E1 slot step, E2 flex, E3 exit step, E7 jogs | lengths and steps |
| **Style presets** (later, from the corpus) | F4 merger styles, F1/F2 PSG 227 blocks (oracles), F3 corpus patterns | compositions, not shapes |
| **Out of scope** (rulings) | B5 non-row prio styles (D287), E4 shifter, E5 unbalancer, E6 lane change (D126, D187), C12 level crossing (D182, D217) | |
| **Gadget candidates** | C9 direction cross (2x2: a split and a swap sharing tiles; PSG 227 only; D311 (4) "later"), B4 prio over a bridge (undefined, D347) | the only shapes no rule expresses yet |

## What it means
- No catalogue data structure is needed now. Every shape T2, T3, LL2 and PSG 197 need is a rule of the layer that already owns it; "verified once" becomes **one proof test per rule** (a node's cusp per heading class and arity; a span's tile rules per kind and angle mix; a corridor's offsets per bend), not per gadget instance.
- E3 shrinks to: spines (H14), the node-rule proofs (cusp; Structure's node merging for tridents), corridors for rows and copies, free-face cells (H12/H16, turn-regularity candidate). Gadgets return only when a shape no rule expresses is needed (the 2x2 direction cross first).
- Doc conflicts the sort settles (mine to fix in the promoting commits): DESIGN-sss-v2 §2/§3.2 "gadget per lane count" and H15; CARD-E3-catalogue's data form; D110's checker clause (G4) for 0-tile pairs once Structure merges nodes; D355 split gaps (a Slots lower bound, not a gadget parameter).

## Decision for Pete
- **Context:** v2 (D388) planned a gadget catalogue verified once per lane count; after D398 and D400 the inventory shows almost every shape is a rule of an existing layer.
- **Options:** A. Drop the catalogue for now: rules per layer, each with a proof test; gadgets only for shapes no rule expresses (C9 first, when needed). B. Keep a catalogue data form anyway, holding the rules' parameters in one place.
- **Recommendation:** A (plainest model first; D400's reasoning applied to every entry).
- **Reversible:** yes.
