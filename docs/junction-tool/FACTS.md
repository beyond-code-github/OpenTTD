# FACTS: the junction problem (draft 2026-11-26)

Read this before designing, briefing or proposing anything. Part 1 is the problem: what is true of every hub whatever solver we build. Part 2 is what our attempts ran into, each traced to the Part 1 fact it hit. Rulings live in DECISIONS.md; code facts live in the CLAUDE.md files.

Each row: **id** fact. *Source:* where it is shown. Source tags:
- [game]: OpenTTD's rules, pinned in the game or in the code;
- [coop]: openttdcoop practice or an engineer ruling about what a good hub is;
- [derived]: follows from the rows it names;
- [corpus]: measured on expert hubs;
- [engineer]: stated by the engineer.

A probe that teaches us something about the problem adds a Part 1 row. A probe that teaches us something about a method adds a Part 2 row, and names the Part 1 row behind it.

# Part 1: the problem

## Track geometry
- **G1** [game] Track runs in 8 headings. A straight tile is 16 train steps and a diagonal half piece is 8. Every piece's end point lies on a tile-edge midpoint. *Source:* `GetNewVehiclePos`; domain `grid` rule.
- **G2** [game] Two tracks may share a tile only as a switch, or as the two halves of a diagonal pair. *Source:* `CheckPlanarDomain` (D428). The X|Y diamond is excluded by our own ruling (D400), not by the game.
- **G3** [game] Curves cost speed. A 90-degree bend caps a train at 61. Two or more same-direction 45-degree bends under one train cap it at `232 - (13 - s)^2` for average spacing s in half tiles. One left plus one right is exempt only when they are the only bends under the train. *Source:* `Train::GetCurveSpeedLimit`. So curve spacing (CL) comes from the train length.
- **G4** [derived G1, G3] A track moves sideways only along diagonal pieces, and each sideways step (S-bend) is two 45-degree bends kept CL apart along travel. So every S-bend costs length along the track, and its sideways size costs diagonal length.
- **G5** [engineer, derived G3] Curve spacing bounds how many bends fit a stretch: a piece 7 tiles long holds at most two bends.

## Topology and planarity
- **T1** [derived] Two routes that share no line must cross exactly when their ends interleave around the box. *Source:* `planar_topology`, proved by parity.
- **T2** [derived] With every crossing taken as a vertex, a hub is a planar map. The topology plus the order of crossings along each track fixes the faces, and which side of each neighbour every track lies on, with no lengths. *Source:* track map (`PlanarMap`, V - E + F = 2).
- **T3** [derived T2, G1] A known side fixes the half-plane, not how two tracks are kept clear. Two tracks bounding one face may be cleared across (side by side) or along (one ends before the other starts), and which applies depends on their lengths. *Source:* D444, D445.
- **T3a** [measured] In practice, a known side settles almost no clearance forms. On T2 the map's exact rule fixes 4 of 1711 pairs, and the side-only shortcut (`inward`) rules out the legal drawing on 155 of 957 pairs. But most choices don't matter: about 800 of 957 pairs on T2's legal drawing are far enough apart that any form holds. Which pairs matter depends on lengths. *Source:* ng2 adb83d02d6 (step 0).
- **T4** [derived G4, T3] So the length-free part of a hub is: which tracks exist, their order and sides, which crossings share a bridge or tunnel, where each turn falls between events, and the node legs. Every S-bend, every clearance direction and every crossing angle uses length. *Source:* engineer 2026-11-25 ("don't try to solve for bends without lengths").

## Crossings, bridges and tunnels
- **B1** [game] Bridges and tunnels run straight on an axis. A bridge middle tile may carry ground track beneath it at any angle. A bridge head cannot sit on a switch tile, and its deck-side end is not a ground end. *Source:* `CmdBuildBridge`, `CmdBuildTunnel`; domain pins 3b, 5b.
- **B2** [game] Signals cannot go on a bridge, so a bridge is one signal block. *Source:* `CmdBuildSingleSignal`.
- **B3** [coop] A busy bridge is limited by flow, not only by the game's maximum. A doubled bridge spans at most 2 d + tl, and longer spans are broken up by a signalled landing, or tripled. *Source:* D208, D228a, D346.
- **B4** [derived G1] A crosser at 45 degrees takes more deck than one at 90 degrees: half a tile extra at each side of its tile. So a structure's span depends on its crossers' angles. *Source:* `span` row (1ad12c9973). T3 is one tile short on a structure because of a 90/135 angle mix across sibling bridges.
- **B5** [coop] A full-share main line over a crossing is doubled: two equal-length bridges, staggered one tile, so that both lanes cost the pathfinder the same. *Source:* D19, D80, D105, D331.

## Coop rules that use length
- **L1** [coop] A non-prio route needs a whole-train bay (at least 2 TL) between its last split and each merge. *Source:* D83, D88.
- **L2** [coop] A prio is exactly its coverage long, worked out from the trains' acceleration. *Source:* D233, D384.
- **L3** [coop] A split needs a signal-legal tile, carrying a signal, before the next bridge head. *Source:* D113, D399.
- **L4** [coop] A branch port's stub is 2 tiles, and ports sit at notation spacing. *Source:* D115a, D313.

## Compactness and what experts do
- **K1** [coop] The goal is the least area inside the outline. Coop builds are a benchmark, not a ceiling. *Source:* D315, D329, D153.
- **K2** [corpus] V's PSG 197 uses 50 sideways steps. After a split, a leg steps toward its sibling (35 of 36). Before a merge, a leg steps away from it (29 of 40). *Source:* psg197.steps.txt, IDEAS §4.7.
- **K3** [corpus] The expert hub fits inside our rules: V's 197 keeps every bay and curve row (86 and 430). *Source:* planar CLAUDE.md (drawing program).
- **K4** [corpus] Expert hubs exist for many topologies: 252 PSG saves (178 load); one extracted so far (197). *Source:* corpus-growth memory.

- **K5** [measured] T2 has a domain-legal drawing with every prio row exactly its coverage long in a 58 x 41.5 tile box (2407 tiles²); B's T2 was 94 x 29 (2726). Without coverage, the same choices fit 42 x 26.5, so prio coverage is a large part of T2's size. *Source:* ng2 adeb65f2c3, wip/probes/sizing/step1.md. Bounding box, not the outline area of K1.

## Scale
- **N1** [derived] Tracks per hub: T2 32, T3 54, LL2 57. PSG 197 has 122 crossings. *Source:* ng2 wip/probes/sizing/counts.md.
- **N2** [derived T2, T3] Pairs of tracks bounding a common face: T2 about 1700, T3 about 3200, LL2 about 3600. Only a small share are close enough to bind in a drawing: about 155 of 957 on T2's legal drawing, after zero-length pieces are merged away. *Source:* ng2 adb83d02d6.

# Part 2: what our methods ran into

- **M1** Fixing sides and bends first, then solving lengths, is fast (15-700 ms) but loses legal drawings. That is T3/T4: those choices need lengths. *Evidence:* the A and B approaches (RECON-v1-v2.md): templates and the slot layer stop at sizing. The slots chooser on T2: B's variant is rank 1760; stage A/B is Infeasible on C's rows. D449's pre-program.
- **M2** Choosing bends before keep-apart, then keeping apart, loses legal drawings: the first stage zeroes flanks the legal drawing needs (G4, T3). *Evidence:* HANDOVER-d456 UPDATE 7.
- **M3** Checking each structure's span alone misses sibling bridges sharing crossers (B4). *Evidence:* T3, HANDOVER UPDATE 8.
- **M4** Writing every face pair's clearance choice up front, with big-M rows, does not converge on T2 (N2). *Evidence:* D381 -> D444. Lazy whole-hub loops take 22-26 rounds of 10-36 s. *Evidence:* IDEAS §3.
- **M5** Local questions (one pair, one span, one face) are decided in about a second by CP-SAT. *Evidence:* IDEAS §3.
- **M6** Shortcuts that fix clearance directions from the face shape are exact only for anti-parallel facing walls (33 of 33 on T2's legal drawing, but only 4-33 pairs per hub). The `inward` shortcut is not exact: 155 pairs of T2's legal drawing break it (T3). *Evidence:* ng2 adb83d02d6.
- **M8** With every clearance direction and bend fixed from a legal drawing, T2's lengths solve in 0.15-0.3 s (HiGHS, no binaries) and the result is domain-legal. Choices frozen from a stretched drawing (68 x 50.5) still allow 58 x 41.5 with coverage, so most of the stretch came from the feasibility-only objective, not from the choices. Whether freeing the choices compacts it further is open (Q2). *Evidence:* ng2 adeb65f2c3.
- **M7** Convex cells (D431) and sides read from earlier drawings (D418) are retired.

## Open
- **Q1** Can a clearance direction be fixed exactly from T2/T3 facts for more pairs (`walk`, `walk_strict`: exact or not on T2's legal drawing)?
- **Q2** Does one solve deciding bends, clearance directions and lengths together (native CP, no big-M) give a compact legal T2/T3/LL2 in under 5 s? Abandon criterion and fallback: IDEAS §8.
