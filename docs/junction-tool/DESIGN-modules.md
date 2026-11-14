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

Each block is testable alone: an exit block or a join block on a straight line, checked by `CheckJunctionRules` with its own ports and movements, buildable in game.

## 5. What changes in today's model
1. **Topology** = core type + per-arm options ("balanced? yes/no", "shifted? yes/no"), as the engineer put it, instead of one pairing threaded through the core. Today's pairing modes map to: keep = keep core; any = keep core with choice crossings (default) or another pattern.
2. **Tunnels are first class.** The bridge-only rule families (bridge over S-bend, heads hug crossers, clearance) must not apply to a tunnel crossing; the site probe showed the tunnel variant still carries the bridge-over-S-bend row (D281). Fix first: it may be what stops the keep core.
3. **Notation gaps up to 12** (`ParseLineNotation` refuses over 3; real gaps 3-12, Mark's BBH17 is 10).
4. **Three-input joins and lane-count changes (3 to 2 to 3)** come with the PSG 227 and PSG 197 patterns, not before.
5. **Rows and bands** stay as the core generator's internals; nothing outside the core sees them.

## 6. Order of work (each a small step with its own test)
1. Tunnels skip bridge-only rule families. Test: a tunnel crossing has no bridge-over-S-bend row.
2. Notation gap up to 12. Test: `LL_______RR` parses with gap 7.
3. (dropped: the core is now a bundle T, step 5, not today's keep graph.)
4. Join block alone, per pattern (keep, balanced no prios, PSG 227): two bundles in, two lanes out, on flat ground; checker clean; reach as the pattern says. Then the exit block the same way.
5. Bundle core: a T whose tracks are two wide (parallel bridges and tunnels, S-bends together); places and checks as a T does today.
5a. Composition: exits + core + joins; LL² any (PSG 227 pattern) places, reach as PSG 227's table, checker clean; hover under 100 ms.
6. LLL_RRR x LL_RR and LLL_RRR x LLL_RRR the same way.
6a. The other patterns as toggles (D284): keep, choice crossing (bridges and tunnels), full any. Acceptance: a test over every pattern x every line pair places with the checker clean.
7. Later: three-lane bundles and lane-count changes (PSG 197), throughput score (§7). The choice crossing fits the same blocks: its line split moves into the exit block and its bundles are as wide as the output arm.

## 7. Throughput (later, D279)
A score per pattern so the tool can trade footprint against throughput: trains per hour through each movement under the benchmark train (D138/D280), from the existing train model and signal blocks. Needed before the tool chooses patterns itself; not needed for steps 1-6.

## 8. Questions for the engineer
**Q1. First "any".** Decided (D283): PSG 227's per-line choice with async prios. The choice crossing comes later.

**Q2. HiGHS.**
Context: with blocks, lengths are difference constraints (longest path, no MILP). The core may still need a small search over crossing forms.
Options: (a) keep HiGHS for the core only until step 5 shows whether it is needed, then decide; (b) drop it now.
Recommendation: (a).
Reversible: yes (dropping is one step later).
