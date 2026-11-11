# RECON: how coop hub builders cross many tracks (bridge survey)

Read-only survey of 13 ABR screenshots plus ABR articles 06 and 10 (scratchpad/abr). Counts are only given where visible; otherwise "unclear". Most hubs here are monorail/maglev on flat ground; the screenshots show almost no true elevated bridges.

## Per-image table

| Image | (1) Longest/typical bridge middle | (2) Max tracks passed by one bridge | (3) What they do instead | (4) Signals between bridges | (5) Doubled bridges |
|---|---|---|---|---|---|
| hubs_slh01 | No bridge visible. All crossings are tunnel portals (pairs/triples of mouths). | 0 | Tunnels, short portal groups; lines run side by side on ground, split lines dive. | Signals sit on the open track between portal groups; spacing too small to count reliably | none seen |
| hubs_slh04 | No bridge visible. Tunnel portal fans, ~6 mouths in a row. | 0 | Tunnel fan for split lines; main line stays on ground | Signals on open ground between fans; count unclear | none |
| hubs_slh06 | No bridge visible. 4-mouth tunnel fans. | 0 | Tunnel fans | unclear | none |
| ABR10_SLH03 | A few short bridges (rail-on-bridge, brown) at top right, lower right and a bridge-with-pylons near centre; each looks 2-4 tiles. Cannot count exactly at this scale. | 1-2 parallel lines (looks like) | Mix of short bridges and tunnel portals; the "cut section" style | Signals near bridge ends; spacing unclear | Yes, bridges appear as parallel pairs at right edge (2 wide), count uncertain |
| 186_bridges | One stone/wooden bridge about 6-8 tiles diagonal, crossing a track pair; picture too small (397x184) to count precisely. | 2 tracks seen under | Single bridge over a pair, then ground track continues | None visible | The bridge itself appears doubled (two parallel decks) |
| hubs_bbh02 | No bridge. Two long parallel ground lines; tunnels (pairs, triples) used for every crossing. | 0 | Tunnels, portals clustered | Signals on open track between clusters; unclear | none |
| hubs_bbh04 | No bridge. 4-mouth tunnel fans, long open lines between. | 0 | Tunnels | unclear | none |
| hubs_173bbh02 | Short covered (glass/arch) bridges at 4 spots: centre top, right, bottom-left, bottom-right; each about 2-3 tiles. The long diagonal in the middle passes over a ground-level grass cut (sunken green strip, about 5 tiles wide). | 1-2 (decks look doubled) | Short bridges plus a ground "cut" for the long crossing, 7-mouth tunnel fan lower left | Open track visible between consecutive bridge groups; spacing not countable | Yes, bridge shows as parallel pair |
| hubs_173bbh14 | No bridge. Tunnels only; "A" and "B" labels mark lanes | 0 | Tunnels | unclear | none |
| hubs_3way | No bridge. Three tunnel portals; ultimate 3way on ground | 0 | Tunnel under the crossing line | n/a | none |
| hubs_msh03 | No bridge visible; tunnel fans 4-6 mouths. Hub is mergers heavy | 0 | Tunnels, mergers | unclear | none |
| hubs_split | Single covered bridge about 2-3 tiles, double-decked (two bridges stacked: one over the other = 3 levels with track beneath) | 2 bridges at crossing, 1 track under | Three-level stack: tunnel? / rail / bridge pair in the same tile cluster | n/a | Yes, two decks stacked, 2-3 tiles |
| PSG200 | Not a hub; world map. Nothing countable | n/a | n/a | n/a | n/a |

## Summary

- Longest bridge seen: 186_bridges, about 6-8 tiles (image too small to count; low confidence). Nothing near 10 or 11 appears in any image. I did not count any bridge over 8 tiles.
- Typical span: 2-4 tiles. Covered bridges in hubs_173bbh02 and hubs_split are 2-3 tiles.
- Max tracks under one bridge: about 2. No image shows a bridge over 5 tracks.
- Dominant technique when many tracks must be crossed: tunnels, not bridges. Nearly every crossing in the SLH, BBH and MSH images is a fan of tunnel portals (2-7 mouths). Builders group crossings into "parts", each with its own tunnel, and let split lines dive while the main line stays on the ground.
- Others, from the text: a ground-level "cut" (hubs_173bbh02), short bridges in series (the article says "adding another bridge is probably the simplest option"), three-level stacks (tunnel/rail/bridge) and bridge+tunnel mixes.
- Signals between consecutive bridges: open ground track with signals is visible between groups, but I cannot count tile spacing in any image. Text says only that bridge splits are "highly friendly with presignals".
- Doubled bridges: present (hubs_split, hubs_173bbh02, 186_bridges, SLH03). Text: "double bridges need tripling" shows the parallel-bridge idea is normal, but each is short.
- Implication for our generator: an 11-tile bridge over five tracks is outside every example. The builder way is either a series of 2-4 tile bridges with track between, a tunnel, or a cut section. The engineer's belief (10-tile uninterrupted bridges are very rare) is consistent with the images.

## Quotes

- 10-advanced-building-revue-10-invisible-hubs.txt: "Too long bridges? Why don't I just cut them in half! When the bridges just exceed some limited length we have to solve it either by adding another bridge or cut them in half to make them shorter."
- same: "Adding another bridge is probably the simplest option. Another way could be making there a 'cut section' which would be tunnelled by the other hub parts."
- same: "if you build half of the junction and suddenly discover that your double bridges need tripling, some F-word storm might get heard."
- same: "And here we have a TL2 version of a SLH. (some bridges might be too long)"
- same: "As always, bridges allow for much more flexible placement... tunnels make it look clean."
- 06-advanced-building-revue-06-hubs.txt: "A super-powerful move while building hubs is using 3level tracks correctly (tunnel - rail - bridge)... bridges are good because they are more compact, but on the other hand, can not go under anything."
- same: "you can save a lot of space with bridges made like this. Also, it is highly friendly with presignals"
- same: "The basic point is you try to use as long tunnels as possible, and stuff there as much things as possible."
- same: "the 3 levels of track atop of each other are like +50% than normal tunnel over track in terms of tracks-per-tile... they are doubled therefore have half value"
