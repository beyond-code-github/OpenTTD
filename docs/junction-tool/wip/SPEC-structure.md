# SPEC: the Structure layer's hand-off (draft, 2026-11-20, orchestrator; for the engineer)

First of the SSS hand-off specs (D368). Implements src/junction/planar/CLAUDE.md "Architecture" (Structure) and DESIGN-tracks §17.1. Written from the contracts and rulings, not from the code (D368: code is mapped onto it afterwards). Rulings it rests on: D343/D344 (topology), D362, D364, D365, D366, D367, D369, D349 (rows), D351/D322 (copies), D203(2)/D228/D228a (shared structures, flow bound), D347/D347b (prio over a bridge), D24 (split before merge).

## 1. Input
- The topology (authored, D343): ports with their lines in ring order, lines, splits, merges, crossings and their order along each line, routes. Each crossing order is pinned or open (D343/D344); Structure decides only the open ones (triangle flips) and never re-decides a pinned one (D367).
- The spec's rule fields: train length, prio length, lane shares and flows (for doubling and D228a), game limits (longest bridge, max tunnel length).
Nothing else. No headings, positions or lengths exist yet.

## 2. What Structure decides (and nothing else)
Per variant:
1. **Ties:** at every split and merge the rules cannot settle, which leg runs straight (Terms: tie).
2. **Triangle flips:** the order of three mutually crossing lines, where the topology leaves it open.
3. **Structures (D369):** the crossings grouped into structures: a structure is a run of consecutive crossings on one line, its **separator**, with no node and no other crossing between them on that line. Every crossing belongs to exactly one structure.
4. Per structure: the **separator** (which of the crossing lines carries it; per joiner at a prio join, D364) and its **allowed kinds** (bridge, tunnel, or either; D365, D366).
5. **Tracks:** every physical track: the topology's lines; a **copy** beside the separator over a structure where its share needs doubling (D351, D322, D228a; one copy per structure, in series); a **row** beside each lane of a prio join over its coverage (D349).
The variant is the combination of 1-4 (5 follows from them by rule). Variants exist here only (D362).

## 3. Output model (each variant), refined by every later layer (D367)
- **Track map:** every track with its ends (port end, node, or a copy/row split and merge on its parent); per track its events (crossings, nodes) in travel order; the planar map with its faces; sides per stretch.
- **Sites:** each split and merge with its straight leg and the side of the other leg.
- **Structures:** per structure, its separator track and span (first to last crossing event on it), its crossers (the tracks it spans, in order along the separator), its allowed kinds, its copy if any.
- **Structure cost:** a cheap score for ranking (below).
Variants pass down as a list, ranked by Structure cost; later layers add their own cost (D366).

## 4. Promise (each a test at Structure's output, D367)
- P1 The map is a valid planar map of every track (V − E + F = 2, simple faces), every authored crossing appears once, and events are totally ordered per track.
- P2 Sides per stretch have no conflict.
- P3 Every crossing is in exactly one structure; a structure's crossings are consecutive events on its separator with no node between.
- P4 No two separators cross unless one can be a bridge and the other a tunnel (bridge over bridge is impossible; tunnel under tunnel deferred, D365): their allowed kind sets are narrowed so that holds.
- P5 Each structure's crossers fit its kind's limit at the most compact crossing angle (longest bridge / max tunnel length; D228a for a main line), for every kind in its allowed set.
- P6 Copies and rows follow their rulings: a copy exactly where its separator's share needs doubling, beside it over the structure's span; a row beside each prio lane over its coverage, away from the joiners.
- P7 Split before merge on every route path (D24).

## 5. Open questions for the engineer
- Q1 **Structure cost.** Proposal: count of structures, plus copies, plus crossings carried by the main line's separators (D156's lift preference shows up later in Shape and Slots costs, so not here). Alternative: no Structure cost; rank only on Shape and Slots costs.
- Q2 **Grouping choice.** One structure per crossing run is the maximal grouping; splitting a run into structures in series is the D228 cost choice. Proposal: enumerate only maximal runs plus the splits P5 forces, to keep the variant count down (review: bits at channel level, §14). Alternative: every split of a run is a variant bit.
- Q3 **Copies over a tunnel.** D228a applies to a main line in a tunnel (one block). Proposal: a copy per structure whatever the kind, so the kind can stay open (D366's "either") without changing the track map.
- Q4 **The corridor band count** the review asked Structure to hand down: it follows from the map's faces and track order, so the proposal is not to hand it separately (the map is in the model every later layer receives, D367).
