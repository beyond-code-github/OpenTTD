# DESIGN-modules: hubs built from blocks (D277, D279, D281)

Status: DRAFT for the engineer's review. Nothing is briefed from this until he has read it.

## 1. Why
- Today one search places a whole hub: every crossing chooses its form, order and length together (the joint MILP). It places T and LL_RR x L_R (both built in game), and L_R x LL_RR seeds.
- LL² keep fails on one site (bridge over S-bend, edge 13) at every gap, prio and tunnel setting tried. LL² any with tunnels did not finish one solve in 18 minutes (D281). The hover target is 100 ms.
- Real hubs are not one search. The wiki's BBH is "one ML crossing, one crossover, 3 exits and 3 joins", with balancing in the joins and prios last (RECON-corpus-anatomy). PSG 227 and PSG 197 are the same skeleton with different ways of doing the choice (ANATOMY-psg227, ANATOMY-psg197).
- So a hub becomes a few blocks, each small and mostly fixed in shape, joined end to end. A block's search is small, and composing them is cheap.

## 2. The blocks (coop words)
| block | what it does | lanes | stretch (what the solver sets) | seen |
|---|---|---|---|---|
| plain line | carries lanes | n to n | length | everywhere |
| exit (direction split) | each lane splits by destination direction; no balancing | 1 to 2 per lane | split spacing | every hub |
| core | carries each (input direction, destination) track to its arm; the ML crossing plus crossover | keep inside | bridge and tunnel lengths, S-bend widths | every hub |
| line choice | splits a track by output lane, before the join it feeds; rides on the crossing where one is needed (choice crossing) | 1 to 2, 1 to 3 | split spacing | wiki; 2 to 2 in 1-3 tiles, 3 to 3 up to 7 (corpus) |
| join (merger) | merges the tracks bound for one output lane; whether a track gets a prio is set by the balancing pattern (D282) | k to 1 | prio length (slider, 14 fixture) where there is one | every hub |
| crossing kinds | how one track passes another | | | |
| - single bridge | | | length | everywhere |
| - doubled bridge | two tracks, one span each, signalled between | | length | PSG 227 |
| - tunnel | wormhole; surface track may cross above it | | length (up to 15 seen) | PSG 197 |
| - tunnel bundle | one tunnel per output lane side by side; the choice is made by the free tunnel (pre-signals) | | each tunnel's length | PSG 197 |

A hub is: per input arm, exits (direction split); then line choice; then the core crossing; then per output arm the joins, with prios where the balancing pattern puts them (D282). Line choice always comes before the join it feeds (the join is where the choice is used: "joins require balancing"). Where exactly it sits (riding on the crossing, or just before the join) is the balancing pattern (§3).

## 3. Balancing patterns (D279)
Each is a toggle in the topology window (D284), with its prio choice (D282); every pattern must place for every supported line pair, so no step may special-case one pattern. Throughput is scored later (§7).
| pattern | how | core | extra blocks | real example |
|---|---|---|---|---|
| keep | lane k to lane k | keep | none | psg173_14, psg327_12 (46 x 30) |
| choice crossing | per (input direction, destination) the track splits once per output lane before the crossing; the tracks pass side by side (one bridge or tunnel each) and come out at their joins; pre-signals send a train to the free one | keep core with each crossing widened to a parallel bundle | none | PSG 197 (tunnels), PSG 227's doubled-bridge choice split (bridges) |
| balanced, no prios | every input lane splits by direction, then by output lane; joins are plain merges, nobody has priority | keep plus line splits | none | the base the prio patterns add to |
| per-line choice, async prio (first "any", D283) | per (input line, destination): one lane fixed (has the prio), one lane re-splits by output lane; three-input joins | keep plus one split per pair | three-input join | PSG 227 |
| tunnel bundle | split by output lane before the crossing; one tunnel per lane; choice by the free tunnel | bundle | none | PSG 197 |
| full any (today's) | every lane split to every output lane inside the core | woven | none | none found in the corpus as one search; real woven boxes are 46-60 x 46-50 |

Reach: the choice crossing reaches every output lane from every input lane, so it meets D221 A's reach. Unlike today's any, the choice tracks are not placed one by one through the core: a bundle is one unit to the core, which then looks like keep with wider tracks. A separate balancer block after the joins was considered and dropped (engineer: "After the joins? That doesn't sound right"): it adds a second merge stage and does nothing a join with choice does not.

Prios follow the pattern (D282): a prio is not added at every join by default. The pattern says which track at a join has priority and which yields, or that none does. Seen: PSG 227 gives the prio to the fixed lane and lets both choice lanes yield; in a choice crossing the choice is by the free bridge or tunnel, and whether its join needs a prio is part of that pattern; keep gives the through lane priority over the turning track (today's prio element).

## 4. Placement under blocks (D286: split early, cross in the core, choose and merge late)
A three-arm hub (our LL² is a T, like PSG 227) is three kinds of block. Bundle = the tracks of one (input arm, destination) pair, running side by side.

1. **Exit block** (one per input arm). Each input lane splits by destination; the branches regroup into one bundle per destination, so the two lanes' branches for one destination end up adjacent. For two lanes and two destinations that is two splits and one crossing (lane 1's far branch over lane 2's near branch). Output: two bundles of two tracks, one per destination.
2. **Bundle core** (one per hub). Carries the six bundles from their exit blocks to their join blocks. A bundle moves as one unit: its tracks stay adjacent, cross on parallel bridges (or parallel tunnels), and S-bend together. So the core is a T whose tracks are two wide. No lane choice happens in the core.
3. **Join block** (one per output arm). Inputs: the two bundles bound for this arm. Outputs: the arm's lanes. Inside: the balancing pattern (§3) and its prios (D282): which tracks split by output lane, which cross, which joins have which prio style. Keep = plain lane-to-lane joins; balanced no prios = every track splits, plain merges; PSG 227 = one choice track per bundle splits, three-input joins with the prio on the fixed track (both of mfb's join styles, D285).

Why this should solve where today's search does not:
- Exit and join blocks are small and fixed by rule for a given lane count and pattern: generated once, their size known before placement (a table, not a search).
- The core only places a T with wide tracks. T places today; the new part is a track two wide.
- The blocks meet the core at fixed ports, so lengths along each arm are difference constraints (longest path).
- PSG 227 fits this INFERRED from the trace: direction splits first (d11-d39), line splits after the core crossings (d37-d73), joins last; its joins sit at the core's edge, not on the arms. Our join block may sit next to the core in the same way.

Zones (D286, refined by the engineer): the through bundles cross each other in the middle of the core; the choice tracks split and swap lanes in the outer core around it, where the join blocks sit. So a join block is placed against the core's edge, between the middle crossing and the arm's port, not out on the arm. This matches PSG 227: bridge stacks in the middle (x 358-379, y 330-351), line splits and joins at the edges of the 27 x 36 core.

### 4.1 Block shapes for two-lane arms (LL_RR)
Within a bundle the two tracks are equal until the join block, so which one is "fixed" is a label the join block picks: the track on the inner side (next to the other bundle) is the choice track. Orders are across the arm, outer to inner to outer.
- **Exit block** (per input arm, 2 destinations a, b): lanes [l0, l1] each split into (a, b): [l0a, l0b, l1a, l1b]; one crossing l0b x l1a regroups them to [l0a, l1a | l0b, l1b]. 2 splits, 1 crossing, no merges. Which bundle goes on which side is the side its destination leaves the core on.
- **Join block, PSG 227 (D283)**, bundles A, B in: [fA, cA | cB, fB], lanes [L0, L1] out. cA splits (cA0, cA1), cB splits (cB0, cB1); one crossing cA1 x cB0; the choice legs bound for one lane merge first (cA0 + cB0, cA1 + cB1; REPRESENT change 2); then each meets its fixed track, which has the prio (D287): L0 = fA + (cA0 + cB0), L1 = fB + (cA1 + cB1). 2 splits, 1 crossing, 4 merges, 2 prio elements. This is mfb's two-stage join; reach: every input lane reaches its fixed lane, every choice track both lanes.
- **Join block, keep**: [a0, a1 | b0, b1] -> L0 = a0 + b0, L1 = a1 + b1; one crossing a1 x b0; 2 merges; prio per D282 (keep: through over turning).
- **Join block, balanced no prios**: every track splits to both lanes: 4 splits, crossings by interleave, 4 plain merges per lane pair, no prio.
- So the exit block and the 227 join block are mirror shapes (2 nodes, 1 crossing, then pairs), and every block crossing is one single bridge over one track: a table, not a search. Lengths are the split spacing, the crossing's bridge length and the prio length.

Core finding (probe, 2026-11-15, wt-choice-probe 896a975c0f): today's row core cannot be the bundle core. With merges moved out (out arms 4 lanes, 12 one-to-one paths) the graph refuses "a main slot without a through partner" (`junction_topology.cpp:194`): every main lane must be a row from W to E, each row "in, one leg, out". The bundle core is therefore the T (L_R x L_R, which places) with every edge two tracks wide; recon RECON-bundle-copies.md checks whether the doubled-bridge copy machinery is that base.

Each block is testable alone: an exit block or a join block on a straight line, checked by `CheckJunctionRules` with its own ports and movements, buildable in game.

## 5. What changes in today's model
1. **Topology** = core type + per-arm options ("balanced? yes/no", "shifted? yes/no"), as the engineer put it, instead of one pairing threaded through the core. Today's pairing modes map to: keep = keep core; any = keep core with choice crossings (default) or another pattern.
2. **Tunnels are first class.** The bridge-only rule families (bridge over S-bend, heads hug crossers, clearance) must not apply to a tunnel crossing; the site probe showed the tunnel variant still carries the bridge-over-S-bend row (D281). Fix first: it may be what stops the keep core.
3. **Notation gaps up to 12** (`ParseLineNotation` refuses over 3; real gaps 3-12, Mark's BBH17 is 10).
4. **Three-input joins and lane-count changes (3 to 2 to 3)** come with the PSG 227 and PSG 197 patterns, not before.
5. **Rows and bands** stay as the core generator's internals; nothing outside the core sees them.

## 6. Order of work (D289: blocks by template, core by the generalised graph/MILP pipeline; each a small step with its own test)
Two streams, run in parallel.
Core (graph/MILP, RECON-bundle-copies steps 2-12):
- C1. Graph data: a leg may end at a block port (third endpoint kind), and an edge has a width (tracks, each its own path). Width 1 and node ends everywhere reproduce today exactly (identity tests).
- C2. The bundle T core graph: 6 legs, 3 crossings, ends at block ports. Graph test.
- C3. Order and structure for a port-ended core. C4. Programs width 2 on row legs. C5. Programs width 2 on band legs.
- C6. Placement chains for parallel tracks (row and band). C7. Render, one route per path. C8. Signals and checker for parallel tracks.
- C9. Series cut for bundle bridges. C10. Parallel tunnels.
Blocks (template generator):
- B1. Seam (RECON-template-seam): a template emits one route per lane track (move strings, rasterised by the route tools) plus a small graph and per-piece tags, so the signal pass, level solve, pricing and build run unchanged.
- Valid by construction (D193): each template encodes its rules (signal spacing, TL, prio room and length, bridge-head clearance); the hover never runs the checker. Block tests assert the template's invariants directly (reach per pattern, spacing, prio length, every track signalled); `CheckJunctionRules` runs only as the test oracle on composed hubs with the hub topology, and a violation there is a missing template rule.
- B2. Join block alone, per pattern (227 first, then keep, balanced no prios): two bundles in, two lanes out, flat ground, checker clean, reach as the pattern says.
- B3. Exit block alone, the same way.
Then:
- H1. Composition: exits + bundle T + joins; LL² any (PSG 227 pattern) places, checker clean, hover under 100 ms.
- H2. LLL_RRR x LL_RR and LLL_RRR x LLL_RRR (blocks by rule for 3 lanes).
- H3. Every pattern as a toggle (D284): a test over every pattern x every line pair places with the checker clean.
- Still open, independent: tunnels skip bridge-only rule families; notation gaps up to 12.

## 7. Throughput (later, D279)
A score per pattern so the tool can trade footprint against throughput: trains per hour through each movement under the benchmark train (D138/D280), from the existing train model and signal blocks. Needed before the tool chooses patterns itself; not needed for steps 1-6.

## 8. Questions for the engineer
**Q1. First "any".** Decided (D283): PSG 227's per-line choice with async prios. The choice crossing comes later.

**Q2. HiGHS.**
Context: with blocks, lengths are difference constraints (longest path, no MILP). The core may still need a small search over crossing forms.
Options: (a) keep HiGHS for the core only until step 5 shows whether it is needed, then decide; (b) drop it now.
Recommendation: (a).
Reversible: yes (dropping is one step later).
