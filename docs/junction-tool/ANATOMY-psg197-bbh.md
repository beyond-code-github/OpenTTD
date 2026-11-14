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
