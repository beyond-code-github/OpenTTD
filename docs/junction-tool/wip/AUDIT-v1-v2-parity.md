# AUDIT: v1 planar rules vs the v2 drawing path (2026-11-25, Sonnet Explore, read-only; written by the orchestrator from its report)

Key facts: the Slots probe turns rules on by env, not by default: bays and curves are off (X1Fixture :152; `PLANAR_STAGEB_CURVES` stage B only), coverage, flank_turn, node_flank, node_cusp, grid_conditional are env-only; node_gaps stage B only. v2 has no plan-level caller (RenderPlanarDrawnHub only from tests; no prio_beside, no levels).

MISSING (most likely to break a rig build of T1/T2 first):
1. Chevron joins / row never drivable (D455; being built: BRIEF-hub-prio-rows run 4).
2. Exact coverage (option exists, 871d42d25b; env-only in the probe).
3. Curve spacing off in the probe; flex/step middles count as travel, not bends (D420): S-bends can breach it.
4. Bays off in the probe.
5. Grid parity not forced by default (grid_conditional env-only).
6. Prio element room: no tile exclusion on row tiles (v1 planar_layout.cpp:921-957 family 14); only keep-apart spacing.
7. Doubled bridges: equal arms, copy deck stagger, signal tile before the copy split (v1 planar_components.cpp:189-203, 317-324, 479-502; D118, D145, D80): no draw rows.
8. Cusp / flank / node_flank env-only (D448, D451).
9. Copy switches absent from the v2 signal input (v1 `copy_switches`).
10. No split/merge of a main-line lane inside prio coverage (D108): no draw row.
11. prio_beside + levels (terrain only). 12. Tunnels (no levels). 13. Terrain facts; outline-area objective (size, D354).
IN: ports/side gaps, node legs (stage B), node lead, deck gaps, end runs + end_node, span limit, cross_piece, keep-apart (geometric), box, node/port tiles, event order.
DROPPED by ruling: prio never over (D347/D347b, D342); port line >= 2 tiles (D412, D434).
