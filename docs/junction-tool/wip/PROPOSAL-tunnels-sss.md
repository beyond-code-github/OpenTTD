# Proposal: tunnels, terrain and the hover under SSS (2026-11-20)

Answers STATUS "FIRST, next session": go back to wip/REVIEW-architecture-holistic.md (§5 layer stack) and factor in tunnels, or defer them. Implements: src/junction/planar/CLAUDE.md "Architecture: SSS" (D361, D362); amends the review's L1, L2, L3 and its "Terrain stays after L5" line. Prior art read: D154, D155, D171, D190, D209, D218, D220, D228, D228a, D297, RECON-tunnel-crossings.md, RECON-slope-physics.md, wip/RECON-join-forms-2.md, DESIGN-planar §3.

## 1. The one fact that decides it
In plan view a tunnel is a bridge with the roles named differently. Both have one line, the **separator**, that changes level and runs straight on an axis across a span; the other lines cross the span's middle tiles at grade, at any angle, free to bend (D342). A bridge's separator is on top; a tunnel's is underneath. Who is on top is a height fact; who is straight is a planar fact.

So the planar layers need only **the separator** and **the kind** (bridge or tunnel) per structure. "Over/under" in the SSS text conflates the two. The corpus says the same: at PSG 227's E port, joiner SW361 tunnels under the mains and joiner N355 bridges over them. That is mixed in over/under terms but uniform in separator terms (the joiner is the separator both times).

## 2. What a tunnel costs on flat ground (game rules, RECON-tunnel-crossings §1, RECON-slope-physics)
- A portal needs an inclined tile (never flat, never a foundation); the train runs flat at the floor through portal and middle.
- **Dive** (the coop pattern, D209: "crossers diving in tunnels with the main line at grade"): the separator drops one level. Its straight axis run is ramp, portal, middles, portal, ramp: **two tiles longer than a bridge** (a flat bridge head climbs on the head tile itself). Crossers stay at grade on untouched middle tiles. Cost: terraform (lower the cutting), 2 slope tiles on the separator, tunnel price.
- **Mound** (raise the middles; RECON-tunnel-crossings assumed this): every crosser climbs, on axis ramps. Worse in every coop case: it puts slopes on the crossers, usually main lines. Not used.
- Limits differ: a bridge's span is capped by the longest bridge and the D228a flow bound; a tunnel's by max_tunnel_length (64) and, when it carries a main line, the same D228a flow bound (a tunnel is one block, D154). This is D220's case for tunnels at LLL.
- Separator against separator: bridge over bridge is impossible; bridge over tunnel is fine; tunnel under tunnel needs two depths (deferred, below).
- Tiles beside a cutting get lowered corners and need foundations: a height cost for the Heights layer, not a planar fact.

## 3. Amendments to SSS (proposed)
- **Structure:** per structure (a crossing set sharing one bridge or tunnel, D203(2)), decide the **separator** and the **kind**. Both are variant bits; the kind's default on flat ground is bridge, and tunnel is taken where a bridge's capacity (§Shape) cannot hold the span. D347b's form bit is the separator bit (granularity: corpus, §5). Separator × separator crossings: only bridge × tunnel allowed in the first cut.
- **Shape:** the separator is straight on an axis across the span, plus, for a tunnel, one ramp tile at each end. Capacity rows per kind: bridge, the longest bridge and D228a; tunnel, max_tunnel_length and D228a for a main line. No other change: the deck group logic is the same object.
- **Slots:** no step inside a deck, a tunnel or its ramp tiles (the review already says "no step inside a deck"; the tunnel extends it). Frames unchanged, because the planar picture is unchanged.
- **Lengths:** unchanged; the ramp tiles are lower bounds.
- **Heights (new layer, after Lengths, before or beside Signals):** decides every tile's track level and the terraform on the seed's fixed footprint: ramps on axis straight pieces only, portal slopes, bridge clearance, cutting depth. Model: a small LP per line coupled at nodes, crossings and shared corners. Promise: always feasible with terraform allowed (D218), so heights are cost, not feasibility; only hard facts (houses under a track or bridge span, water under track, map edge, unpurchasable) can fail, and they fail before any solve, naming the tile (the hover's offset search and D213's repair path take them).
- **Signals:** unchanged (a tunnel is a wormhole like a bridge, D154).

## 4. The hover reusing the flat seed
- The seed is solved flat with every tunnel's ramps already in its footprint, so on real ground the footprint never grows: hills only make a portal natural (the ramp becomes plain track), never longer.
- The hover runs Heights only, on the fixed footprint: no length re-solve, no lazy terrain facts. This retires today's lazy terrain loop (planar CLAUDE "Back end": "terrain facts lazy, warm-started from the seed"), the generate-and-check site the review flagged.
- The hover may flip **kind** (never separator): tunnel to bridge wherever the span fits a bridge (the footprint shrinks by the ramps: always fits); bridge to tunnel only where the seed's straight run already holds the ramp tiles or the ground already slopes. A flip of separator changes Shape, so it is a seed-level variant.
- This replaces the earlier proposal's (2) and (3): (2) said tunnels keep the track level with ground raised (the mound, which puts slopes on the crossers); (3) said a kind swap only at right angles, but the crossers above a tunnel may be at any angle, like those under a bridge.

## 5. D347b in separator terms
From wip/RECON-join-forms-2.md (complete, 2026-11-20; 12 hubs with joiner-main crossings; PSG 227 control reproduced):
- Joiners: 32 uniform, 0 mixed, in over/under, separator and kind alike. No joiner ever changes form between its own crossings.
- Port groups: by over/under 7 uniform, 5 mixed; by separator 8 uniform, 4 mixed (136_7, 13_23, 314_13, 09_13). The separator view dissolves 227's E-port mix; the other four mix between joiners of one group, within one gate type in three of them.
- So one bit per join fails on the corpus (4 of 12 groups); one bit per joiner fits all 32.
- Recommendation: per joiner, one **separator** bit (joiner or main) and one **kind** bit (bridge or tunnel), shared by all its crossings with mains. Bit count grows with joiners, not crossings, which keeps the review's LL³ concern in check (the search ranks on cheap scores, review §5).

## 6. What I recommend deferring, explicitly
- Building any of it before the frame experiment and T2/T3/LL² end to end checker clean (review §7 steps 1-4): tunnels and Heights are an L1 bit, an L2 capacity row and a new layer after L5; the frame experiment does not depend on them, because a tunnel's planar picture equals a bridge's.
- Tunnel under tunnel (two depths), organic terrain repairs (changing shape to fit ground, the old D199 idea), seeds solved on terrain rather than flat.
