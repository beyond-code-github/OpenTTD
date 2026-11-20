# CARD E1a: authored crossings, tunnel over tunnel, nodes over a tunnel, prio per merge (orchestrator, 2026-11-20)

Implements DESIGN-sss-v2 §3.1 items 1-6 (E1, PLAN-sss-v2). Rulings read: D343, D344, D364, D365, D369, D387, D388; psg197-gaps §1-2, §9; psg197-decisions.

## What the probe found (wip/probes/e1, 2026-11-20, a few seconds of Python, no C++)
Test: for every two paths of different routes that share no line, the crossings between their lines must be odd exactly when their ends interleave on the port ring (a necessary planarity condition, exact for these pairs).
1. The 112 tile-evidence crossings break it on **30 of 847** pairs.
2. The evidence rule ("a track piece on a tunnel middle tile") misses **14 tunnel-under-tunnel crossings** (no track on the shared tile): A1c/A1d/A1e x F2/F2b/F3/F3b, A2b/A2c/A2d x F1/F1b/F3/F3b. With them: **12 of 847** bad.
3. All 12 are one artefact: **5 split/merge nodes sit on a tunnel's middle tile** (81,168 on E2b; 87,164 on C12b; 97,177 and 97,185 on A1d; 100,165 on F12); the crossing was counted on both the node's in and out lines. Counting it once, on the node's trunk (a split's in line, a merge's out line): **0 of 847** bad, **123 crossings**.
4. So PSG 197 has a consistent crossing set, and the D388 example of a partial bundle crossing ("F3 crosses A1c only") was the missing tunnel-under-tunnel crossings. Per tunnel, G02 and G05 have identical crosser sets.
5. The genuine partial case is narrower: in **4 tunnel groups** (G12, G16, G19, G27) a crosser channel splits or merges *on the ground above the group*, between two of its tunnels (e.g. D23b crosses A1e, splits over A1d, D23d and D23e cross A1c).

## Proposals
**(1) Authored crossings in the topology text** (D343: the topology owns which lines cross).
`cross L: M1, M2, ...` lists every crossing of line L, in travel order along L. A topology with any `cross` statement is fully authored: derivation is off, every crossing appears in the lists of both its lines in a consistent order, and each listed order is pinned (D344's pin; a topology imported from a save pins everything). Same-route crossings and port lines are allowed. Without `cross` statements, derivation works as today (T2, T3, LL2 unchanged).
Validation at parse: the parity test above over every disjoint path pair (refuses with the path pair named); Structure's P1 then checks the planar map.
Over and under are not topology (D343): Structure still picks separators and kinds; 197's tunnel evidence becomes a test that its variant is reachable.

**(2) A node over a tunnel:** the topology records the crossing on the node's trunk line only (the rule that made the probe consistent). Geometry lets the node sit above the span (Slots: a tunnel's middle tiles are ordinary ground for surface tracks). Nothing new in Structure.

**(3) Tunnel over tunnel** (P4 today: deferred): two tunnel separators may cross; Structure hands the pair down as "different levels", and which one is deeper is Heights' choice (D365 owns levels). Bridge over bridge stays impossible. A portal on another tunnel's middle tile is the same pair seen at a portal: also a Heights matter, plus a landing of 0 between a portal and a crossing over a tunnel (an E2 rule field).

**(4) Crosser nodes inside a group** (DESIGN-sss-v2 §7 q2): when a crosser channel splits or merges between two parallel separators of one group, the separators stay one channel (one spine, one crossing-stack gadget) and the crosser's site is placed inside the stack. Channel identity compares crossings by crosser *site group*, not crosser line. Gadget form (a node on the stack's lid) is designed in E3.

**(5) Prio per merge** (psg197-gaps §9): `prio <merge out line>: <holding leg>`; today's `prio R1, R2` stays as shorthand for every merge where that route's leg holds and no gated leg carries it. My default, reversible; recorded as a toggle candidate.

## What I need from Pete
- **(1)** the `cross` notation: fully authored and pinned when present, parity-checked at parse. Alternative: a Structure sidecar file beside the topology (rejected by me: D343 wants one self-contained artefact).
- **(4)** the channel rule: crosser nodes inside a tunnel group keep the group one bundle.
(2), (3), (5) follow from rulings or are reversible defaults; I record them in DECISIONS unless Pete objects.

## Next, once agreed
E1b brief (Sonnet, test-first): extract each line's crossings in travel order from the save trace (positions along the line, tunnel-under-tunnel by tunnel geometry, node rule (2)) into `cross` statements in psg197.txt; parser accepts `cross` with the parity check; tests: psg197 parses (gaps §1-2 closed), a hand topology with one wrong parity is refused naming the pair, T2/T3/LL2 parse identically.
