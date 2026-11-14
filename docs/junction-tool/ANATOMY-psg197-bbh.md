# Anatomy: PSG 197 BBH01 "Tunnel only" (V453000), a balanced 3-way of LL_RR and two LLL_RRR

Source: `PSG_197.sav`, company HQ signed "BBH01 V453000", centre about 91,169 (engineer). Hub box 81,141-123,198 (engineer). Export `OD/psg/anatomy/psg197/bbh.json` (box 76,136-128,203), ASCII `OD/psg/anatomy/psg197/pic.txt` (OD = `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad`). Junctionary BBH list: LLLRRR "Tunnel only", PSG197, V453000.

## Shape (orchestrator, from the export)
- Box counts: 555 plain track, 501 signal tiles, 105 tunnel heads, 3 bridge tiles (outside the hub, by a station at the box's SE corner), 12 station tiles, 1 depot.
- Three arms: top (LL_RR: two 2-track pairs at x 84-85 and x 92-93, the directions about 7 tiles apart), right (LLL_RRR, y 163-173), bottom left (LLL_RRR, x 81-91).
- Tunnel heads come in parallel groups (TT, TTT) throughout the core.

## The engineer's reading (2026-11-14, in his words, grouped)
1. **Same skeleton as PSG 227, by tunnels.** "A similar shape to the first one with a line incoming from the top, another from the right, and another bottom left ... the top is LL_RR and the other two are LLL_RRR." "V is using a similar strategy with tunnels to the one used by MFB with bridges."
2. **Split, then side-by-side tunnels of different lengths.** "Each line splits into three, and then enters tunnels side by side, but the point at which each one pulls up is different, emerging into the core of the hub in the right position to join up with its siblings from the other incoming lines."
3. **A 3 -> 2 merger after the direction split.** For the bottom-left lines: "there's actually a 3 -> 2 merger (note the prios) outbound to the right *after* the direction split - this works because some of the traffic has already left to go out through the top, so two lines can run through/around the outside of the core. The same pattern can be seen on the lines travelling out to the top of the hub. Heading right, shortly after the 3->2 are the choice splits for the three outbound lanes."
4. **History.** "It looks like this was originally a BBH with two LL_RR inputs and one LLL_RRR output, then at some point the third line in/out from the bottom left was added" (a sign reads "3rd line /V.").
5. **Size as the trade-off.** "A pretty straightforward design which is indicated by its sheer size. The tradeoff that was made is where a few lines travel SUPER far through long tunnels - up to 15 tiles in some cases."
6. **A tunnel bundle as a tripled bridge, with choice by free tunnel.** "This works *because* those tunnels are actually all leading to different output choice lines in one direction (for example top left to bottom right) - and not only that - pre-signals are used to ensure that trains make their choice of which line to choose based on which tunnel is free. So for all intents and purposes it's a tripled bridge as far as throughput is concerned."
7. **A suspected error.** "Two of the three-split tunnels exit at the bottom right and both feed the outermost line."

## Compared with PSG 227 (orchestrator)
| | PSG 227 (mfb) | PSG 197 (V453000) |
|---|---|---|
| Lines | three LL_RR | LL_RR + two LLL_RRR |
| Crossing | bridges (19), 1 tunnel | tunnels (105 heads), no bridges |
| Line choice | one choice lane per (input line, destination); the fixed lane has the prio | split into one track per output lane before the tunnels; the choice is made at the tunnel entrances by pre-signals (the free tunnel wins) |
| Joins | three-input, on one tile or two stages | tunnel exits emerge in place to join their siblings |
| Lane count inside | constant | 3 -> 2 merger after the direction split, then 2 -> 3 choice splits |
| Core | 27 x 36 | about 38 x 38 (hub box 43 x 58) |

## What this adds to the model (orchestrator, to confirm)
- **A parallel tunnel bundle is a crossing block** with one tunnel per output lane: tunnel lengths are free, so each exit lands where its join is; throughput as a tripled bridge.
- **Choice by availability:** pre-signals at a bundle's entrances let a train take whichever tunnel (and so output lane) is free; the split before the bundle is the line choice.
- **Lane counts may change inside a hub** (3 -> 2 after traffic has left by another direction, then back to 3).
- **Footprint against tunnel length:** a hub can trade size for long tunnels (up to 15 tiles here).
- **The checker as an oracle:** the suspected two-tunnels-to-one-lane error is a good test that our rules (or a throughput measure) should flag.

## Not yet done
A lane-by-lane trace (as for PSG 227) to confirm 2, 3, 6 and 7 with tile boxes, tunnel lengths and the pre-signal types at the tunnel entrances.

## Trace (tile evidence)

Recon 2026-11-14, from the export plus the save's piece graph. Scripts and raw outputs: `OD/psg/anatomy/psg197/trace/` (`reach.txt`, `block_table.md` (all 68 blocks), `blocks_raw.txt`, `events.txt`, `per_lane_events.txt`, `per_lane_blocks.txt`, `paths_all.txt`, `joins.txt`, `tunnels2.txt` (50 tunnels), `tunnels3.txt` (signals upstream of each entrance), `signals_bridges.txt`; entry points `a1.py`..`a9.py`, `t_tun2.py`, `t_tun3.py`, `t_tab.py`, `t_f.py` over `core.py`/`net.py`, adapted from the PSG 227 trace).

**Method and caveats.** Box 76,137-128,198 (the engineer's hub box 81,141-123,198 cuts the right arm mid-structure; the station tiles at 128,181-184 are ignored, they sit on a spur). Lanes at the box edge: the save's direction inference is wrong here (N91, E166, E167), so directions were hand-checked against the one-way signals (`LOCAL=1`, plus a fixed lane list). Movement model: one-way signals constrain (block, entry, exit, combo, path_oneway cannot be entered from behind); plain path signals (12) and two-way pieces (11, combo or exit both ways) pass both ways; unsignalled track passes both ways, so the reach table is a superset of any strict direction model. Excluded as not part of the three arms: spur lanes E186, E190 (station line), S115, S116, S128 (the y 198-203 station and bridge line). Arm names follow the picture (`pic.txt`, x right, y down). INFERRED: on the standard screen view the x=128 edge is the lower-left and the y=198 edge the lower-right, so the engineer's "right" is probably the S arm and his "bottom left" the E arm; the table gives both names. The earlier note's "right / bottom left" were by picture position.

**Arms and lanes (task 1).** Lane id = edge coordinate; lane order across the arm is out | in with the gap between.

| Arm (picture) | Engineer's name (INFERRED) | Edge | Out lanes | In lanes |
|---|---|---|---|---|
| N | top, LL_RR | y=137 | N84, N85 (x 84-85, going NW) | N90, N91 |
| E | bottom left, LLL_RRR | x=128 | E164, E166, E167 (E164 = the y 163 line after an S-bend) | E169, E173, E174 (E173 shifts to y 172 inside) |
| S | right, LLL_RRR | y=198 | S88, S89, S90 | S81, S82, S83 |

Outermost lane = farthest from the arm's centre: out N84, E164, S90; in N91, E174, S81.

**Reach table (task 2).** 37 of the 42 cross-arm pairs (`reach.txt`).

| in \ out | N84 | N85 | E164 | E166 | E167 | S88 | S89 | S90 |
|---|---|---|---|---|---|---|---|---|
| N90 | | | x | x | **-** | x | x | x |
| N91 | | | x | **-** | x | **-** | x | x |
| E169 | x | x | | | | x | x | **-** |
| E173 | x | x | | | | x | x | x |
| E174 | x | x | | | | x | x | x |
| S81 | x | x | x | x | x | | | |
| S82 | x | x | x | x | x | | | |
| S83 | x | x | **-** | x | x | | | |

Five pairs are missing (bold), all structural (no adjacent track blocked by a signal): N90>E167, N91>E166, N91>S88, E169>S90, S83>E164. Sources per out lane: N84 and N85 six each (E169, E173, E174, S81, S82, S83); E164 = N90, N91, S81, S82; E166 = N90, S81, S82, S83; E167 = N91, S81, S82, S83; S88 = E169, E173, E174, N90; S89 = five; S90 = E173, E174, N90, N91. So the hub is not a balanced all-to-all LL_RR/LLL_RRR: each of E164, E166, E167, S88, S90 lacks one source (INFERRED: the hand-built, partly retrofitted state, see (f)).

**Rule checks (task 4).**
- (a) Each input lane splits into one track per output lane, entering side-by-side tunnels with staggered exits: HOLDS in part. Holds: N90 to S (three tunnels at x 91, 92, 93 from y 161 to y 174, one each to S88, S89, S90, 13 apart); S82 to N ((81,171) to N84, (82,171) to N85); S83 to N (pairs x 84/85, three stages: N85 via 84, N84 via 85); S83 to E ((88,171) E166, (88,172) E167; second pair at x 95); S81 and S82 to E (a fixed track to E167 plus a choice track to E164/E166: (90,163) and (90,164); (93,167) and (93,166)). The exits land in staggered order: a bundle's exits emerge at one y and the next stage enters at the next join (N90 triple exits y 174, continues at (92,176), (94,176), (92,181)). Fails: N91 to S (see e); the E-arm lanes do not split per output lane before their tunnels: E173 to N runs two parallel tunnels (111,171) and (111,172) that both reach N84 and N85, E174 to N two such pairs ((117,174/175), 7 long, then (107,174/175), 4 long), and E169 has no tunnel of its own until (93,181) (shared with E173). N91 to E and N90 to E have no triple: a pair (99,162) E164, (99,163) E166 for N90, and a path-signal cluster for N91.
- (b) Pre-signals at the tunnel entrances, choice by free tunnel: HOLDS for the first stage after each direction split. 25 of 50 entrances carry an exit pre-signal 1-3 tiles before the head; each is fed by an entry signal 3-5 tiles up the line after the fork. N90 bundle: entry 92,158y, exits 91,159d / 92,160y / 93,159r. N91 bundle: entry 95,154r plus combo 96,155r, exits 95,156y / 96,157y / 97,156r. S82 pair: entry 81,174y, exits 81,172y / 82,172y. S83 pair: entry 84,182y, exits 84,180y / 85,180y. E173 pair: entry 114,172x, exits 112,171x / 112,172x. E174 pairs: entry 120,174x, exits 118,174x / 118,175x. S81/S82 to E: entries 87,164d and 90,166x, exits 89,163x / 89,164x and 92,166x / 92,167x. The 22 later-stage (second and third) tunnels and the 3 with path signals use plain block signals on the entrance: the choice was made at the first stage. Entrance signal tally: exit 25, block 22, path 3.
- (c) A 3 -> 2 merger with prios after the direction split, then 2 -> 3 choice splits, on the bottom-left lines heading right and on the lines heading out the top: HOLDS (INFERRED mapping E to S, or S to E under the other naming). E to S: E173 doubles at 116-118,173 (2x1), then E173 (entry gate) merges with E169 (block) at 107-109,172-173 (3x2) and with E174 (block) at 110-111,176 (2x1): three lanes become two merged tracks; choice splits follow within 4-12 tiles: 104-105,174-176 (2x3, S88,S89 | S88), 106,177-178 (S88 | S89,S90), 97,176-177 (S88 | S89), 105,178-179 and 103-104,179-181 (2x3, S90 | S89). S to E: S81 and S82 merge at 97-98,165 (block/block) into one choice track and again at 101-102,166-167 into the E167 track, S83 keeps its own E166/E167 pair; the choice split 99-103,165 (5x1) follows. To the top: pairwise merges, E169+E173 at 98-101,171 (4x1, E173 entry-gated), E173+E174 at 100-108,174 (9x1, E174 through a path_oneway signal), S81+S82 at 83,160-161, S82+S83 at 84,155-156, then a chain of 2-input merges at x 84-89, y 147-160 into N84 and N85. The three-input join of PSG 227 does not appear: all 33 joins have two inputs and are cascaded.
- (d) Tunnels: 50 rail tunnels (100 heads, x 81-117, y 149-190), no bridges inside the hub. Head-to-head distance: 2 x5, 3 x11, 4 x14, 5 x4, 6 x4, 7 x5, 8 x1, 13 x3, 14 x3. Over 10 (flagged): x 91, 92, 93 (y 161 to 174, 13 apart) and x 95, 96, 97 (y 158 to 172, 14 apart); 14 apart is 15 tiles counting both heads, so the engineer's "up to 15" holds. Both groups carry the N arm lanes to the S arm. Tunnel groups (entrance to exit, distance, lane to out lane): N90 triple above; N91 triple (95,158) S89, (96,158) S90, (97,158) S90, then (96,174), (97,174), (98,174) to y 181 (7, S89, S90, S90) and (97,183), (98,183) to y 187 (4, both S90); N90 follow-ups (92,176) 3, (92,181) 2, (94,176) 8, (94,187) 3; N90 to E (99,162) and (99,163), 2; S82 to N (81,171), (82,171) 5, (82,163) 3; S83 to N (84,177) and (85,177) 5, (84,170) and (85,170) 6, (84,161) 3; (85,152) 3; S81 to N (89,159) 4; S81 and S82 to E (90,163), (90,164) 4, (93,166), (93,167) 3, (99,165) 3, (105,164) 3; S83 to E (88,171), (88,172), (95,169), (95,170) 4, (102,168) 3; E to N (111,171), (111,172) 4, (117,174), (117,175) 7, (107,174), (107,175) 4, (87,167), (88,167) 6, (88,159) 4; E to S (103,178) 3, (93,181) 2, (100,184) 2 (path cluster). Full list `tunnels2.txt`.
- (e) Two of a three-way tunnel group exit at the bottom right and both feed the outermost output lane: HOLDS. The group is the N91 bundle at x 95, 96, 97 (entrances y 158, exits y 172, heading toward high y): (95,172) feeds S89, (96,172) and (97,172) both feed S90, the outermost S out lane. The same doubling repeats at the second stage ((97,174) and (98,174) both S90) and the third ((97,183) and (98,183) both S90). Consequence: N91 reaches no S88 (N90's triple, by contrast, maps S88, S89, S90 one each).
- (f) The "3rd line /V." addition, which tracks are the bottom-left third lane: NOT separable by geometry; INFERRED candidates only. Style hints: path signals (12, with two path_oneway) occur in exactly three clusters, which look retrofitted: N91 to E167 at 98-100,158-162, E173/E174 to S90 at 99-101,181-190 (with a doubling and a cross, 1x1 each), and E174 at 109,174-175 (path_oneway) with two-way combo/exit pieces at 102,174-175. The E174 route is a strip along y 174-176 (x 100-128, 43 tiles private to E174), the outermost in-lane row; the weak reach lanes (S83 misses E164, E169 misses S90, N90 misses E167, N91 misses E166 and S88) do not point to one arm. S83's own tracks are 114 pieces (x 82-112, y 156-198), pairs of tunnels in series. If the third lane of the bottom-left arm is E174 (INFERRED), its strip and the E to S90 path cluster are the add-on.

**Block table (tasks 3 and 5).** Boxes are tile boxes of the signal-free block; in/out = tracks crossing signals. 68 blocks (33 joins, 19 line/choice splits, 8 direction splits, 5 doublings, 3 crosses); the full table with inputs, gates and outputs is `block_table.md`. Footprint: split and merge blocks fill x 78-127, y 147-194; without the E arm's direction splits at x 117-128 the core is x 78-116, y 147-194 (39 x 48); tunnel heads x 81-117, y 149-190; 994 rail tiles in the box (the earlier note's "about 38 x 38" is low).

| Block | Box | Size | In/out | Lanes |
|---|---|---|---|---|
| Direction split E173 | 128,173 (edge cell) | 1x1 | 1/2 | E173 |
| Direction split E174 (+ doubling 120,174) | 123,174 | 1x1 | 1/2 | E174 |
| Direction split E169 | 118,170-171 | 1x2 | 1/2 | E169 |
| Direction split N90 | 91-92,147-149 | 2x3 | 1/2 | N90 |
| Direction split N91 | 92-93,147-148 | 2x2 | 1/2 | N91 |
| Direction split S81 | 78,173-174 | 1x2 | 1/2 | S81 |
| Direction split S82 | 81,176 | 1x1 | 1/2 | S82 |
| Direction split S83 | 84,184 | 1x1 | 1/2 | S83 |
| Line splits to S (E arm) | 117-118,173; 104-105,174-176; 97,176-177; 106,177-178; 105,178-179; 103-104,179-181 | 1x1 to 2x3 | 1/2 | E169, E173, E174 |
| Line splits to N (E arm) | 114-115,172 (doubling); 88-89,170 | 2x1 | 1/2 | E173, E174 |
| Line split N90 to S (3-way) | 92,157-158 | 1x2 | 1/3 | N90 |
| Line splits N90 to E, N91 to S | 94,157-158; 93-95,153-154; 96-98,154-155 | 1x2 to 3x2 | 1/2 | N90, N91 |
| Line splits S to N | 81-82,168-169; 81,174-175; 84,182-183 | 1x2 | 1/2 | S81, S82, S83 |
| Line splits S to E | 87,164; 90,166; 82,178; 99-103,165 | 1x1 to 5x1 | 1/2 | S81, S82, S83 |
| Joins to N | 98-101,171 (4x1); 100-108,174 (9x1); 99-100,175; 83,160-161; 84,155-156; 86,157-160; 86,151-156 (1x6); 88-89,149-151; 85,146-147 | 1x2 to 9x1 | 2/1 each | six lanes into N84, N85 |
| Joins to S | 107-109,172-173; 110-111,176; 95-96,180; 88,181-185; 92-93,185-186; 96-98,185-187; 92,192-193; 94-96,193-195; 97-98,190-192 | 1x2 to 3x3 | 2/1 each | into S88, S89, S90 |
| Joins to E | 97-98,165; 101-102,166-167; 103-104,162; 110-111,164; 111-113,166-167; 104-105,169; 98,158-160 | 1x3 to 2x2 | 2/1 each | into E164, E166, E167 |
| Path clusters | 98-100,158-162; 99-101,181-190 | 1x1 to 3x4 | 2/2 | N91 to E167; E173, E174 to S90 |

Order on a path, e.g. N91 to S90: direction split (d13), line split (d21), doubling (d23), bundle of three tunnels (14 long), joins at d61-67. E173 to N84: direction split (d0), doubling (d15), parallel pair of tunnels, joins d34-72.

**Bridges (task 5).** None in the hub. Outside it: (116,199)-(116,202) span 2 (heights 10/9) and (125,203) to (129,203), far end off the export (height 10), plus the station tunnel (128,198)-(128,201), span 2 (heights 9/10).

**Prio styles (task 3).** Signals in the box: 375 block, 51 exit, 26 entry, 16 combo, 12 path, 2 path_oneway. Join gates over the 42 two-input join and cross clusters: block/block 25, block/entry 5, path/path 2, block/combo 1, block/path 1, entry/path 1, exit/path 1: most merges are plain block signals with no priority; six inputs are entry-gated (E173 at 98-101,171, 107-109,172-173, 100-108,174 and 110-111,176; E169/E173 at 86,151-156 and N90 at 94-96,193-195), INFERRED as yielding inputs, as in PSG 227 style 2. Pre-signals sit on the line splits and tunnel entrances (b), not on the joins.

**Spacing (task 5).** No vehicles in the export, so train lengths were not read. Over the 37 shortest paths (`signals_bridges.txt`, 1147 gaps): 63 percent are 2 tiles, 13 percent 3, 13 percent 1, 5 percent 4, the rest 5 to 17 (the long ones are the 13- and 14-tile tunnels counted by length, no signals inside). One-tile spacing clusters on straight stretches: 98-102,176 (x), 109-113,173, 100-104,172, 123-127,174, 125-127,173, and at diagonal pairs such as 95,166-167 and 90,159-161. Several paths carry three to five consecutive 1-tile gaps (E173 to N84: 104,172 to 99,172).

**Findings for the model (INFERRED).** (1) Not all-to-all: five of 42 cross-arm pairs are unreachable and (e) is a real tunnel-lane duplicate; a throughput measure or reach checker would flag both. (2) Joins are binary and cascaded (block signals, a few entry-gated inputs), not the three-input joins of PSG 227. (3) Choice by availability is a pre-signal block (entry before the fork, exit on every tunnel entrance) at the first stage only; later stages are block-signalled serial tunnels. (4) Tunnel stages chain: a lane crosses two or three tunnels in series (13 + 8 + 3), each stage re-splitting. (5) Core 39 x 48 with 13-14-tile tunnels: the size-for-tunnels trade-off holds. (6) Retrofits show as path-signal clusters.
