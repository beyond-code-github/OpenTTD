# Recon (2026-11-15, at b681da77e7): old band lines vs occupancy order

Kept for the planar redesign (D305-D309); the experiment "sides from band lines in the old pipeline" was not pursued.

- The order stage's sweep (OrderJunctionTrackGraph, topology.cpp ~560-870, south to north) stores the full west-to-east line of edges per band before and after every event: `JunctionTrackOrder::band_lines[b]` (bands[b].size()+1 vectors). Events: node (split/merge expansion) or crossing (swap of two adjacent edges). The initial line is the S port edges in slot order. Lines hold edges, not tracks.
- ArrangeJunctionTrackGraph keeps only diffs (band_replacements), but band_lines stays in `order`.
- Placement never reads band_lines or band_replacements; occupancy orders every pair on every row by a stable sort on flat columns (placement.cpp ~2079-2091), comparing adjacent pairs and groups; the strand side rule is the only rank override.
- Band line could order band-edge pairs inside bands; nothing orders row-edge vs band-edge pieces off item rows, footprints, blocks vs band edges. Groups, tie/apart, legality and the joint's recorded clashes all assume the flat sort.
- related() has no bridge logic; a deck and its crossed edge never meet because Heads makes tiles only at the heads.
