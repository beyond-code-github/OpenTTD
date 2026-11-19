# Recon: join forms, second pass. PARTIAL (checkpointed 2026-11-19)

OD = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg. Scripts and outputs: OD/joinforms/ (copied from the session scratchpad). Read-only recon; Python run with `python3 -I`.

## Method (new, generic; reproduces 227)
OD/joinforms/lib.py `analyze(save, box, R)`: builds the directed piece graph with the 227 trace code (net_g.py, core_g.py = copies of OD/anatomy/psg227/trace with save and box as parameters). Prio merge = a merge whose input is gated (last signal within 12 steps back) by entry, combo or path, and another input is not. Joiner = gated input, mains = the rest. Cones = pieces reaching each input within R=16 tiles (bridge span counted by length). Crossing = bridge or tunnel span whose upper and lower pieces lie one in the joiner cone and one in the main cone. Form: OVER = joiner on the deck of a bridge, or on the surface above a tunnel; UNDER = joiner in a tunnel or beneath a main's bridge. Heights are not used. INFERRED limits: the 12-step gate walk and R=16 are heuristics; direction comes from signals (contradiction zones left free).
Control, PSG 227 box 335,311,392,368 (VERIFIED, `python3 -I jf.py PSG_227 335,311,392,368 16`): merge 379,339 joiner 378,339l UNDER (tunnel 371,338-375,338); merge 378,343 joiner 377,343x OVER (bridge 372,334-372,339). Same as the first recon. At R=26 the SW-port doubled bridges 375,346 and 376,345 appear as OVER.

## Hubs done
Sweep: 102 census2 hubs (ncore >= 20, box <= 140, not station/trivial/mesh-large), OD/joinforms/sweep.txt and sweep.json. 51 have prio joiners; 10 have joiner-main crossings. Rows below are (merge, joiner) units.

| hub | joins / joiners with crossings | crossings and forms | uniform per joiner | uniform per join |
|---|---|---|---|---|
| PSG 227 (control) | 2 (E port) | tunnel UNDER, bridge OVER | yes, yes | NO (mixed at the E port) |
| PSG 186, box 163,243,228,274 | 22 prio joiners, 1 with a crossing | merge 174,243 joiner 174,244u: bridge 172,248-172,260 OVER, 1 crossing | yes | yes (single crossing) |
| PublicServerGame_136 box 22,60,53,143 (psg136_7) | 6 cells, x30-43, y100-126 | entry-gated joiners at 37,100 / 40,103 / 43,106: doubled bridge pairs UNDER (2 each). path_oneway joiners at 30,114 / 36,120 / 39,123 / 42,126: doubled bridge pairs OVER (2 each) | yes (6 of 6) | each cell uniform; the group of 6 cells is mixed (UNDER cells and OVER cells by gate type) |
| PublicServerGame_136 box 0,354,21,434 (psg136_21) | 8 cells | all UNDER, doubled bridge pairs, 2 crossings each | yes (8 of 8) | yes |
| PublicServerGame_136 box 463,45,544,117 (psg136_5) | 2 | both UNDER, doubled bridge pair / single | yes (2 of 2) | yes |
| PublicServerGame_13 box 25,96,58,120 (psg13_23) | 2 joiners (gates 35,104 and 36,104) | one tunnel 35,107-35,111: joiner 35,103y UNDER, joiner 36,103y OVER | yes (2 of 2) | NO (the two joiners of one tunnel differ) |

All rows VERIFIED as script output (OD/joinforms/show.py, run as `python3 -I show.py SAVE X0,Y0,X1,Y1 [R]`). Whether the 136 cells and 13_23 are "multi-lane prio joins" in the engineer's sense (adjacent out lanes of one port) is NOT checked: the 136 cells look like a repeated ladder of single merges with S-bend bridge pairs (picture: `python3 -I pic.py PublicServerGame_136_Final 22,60,53,143 24,84,50,130`). INFERRED.

## Not yet done
- Sweep hits not examined: psg314_5 (OVER and UNDER, possibly mixed), psg314_13, psg314_15, psg314_18 (OVER only), psg173_68 and psg09_13 (OVER only).
- PSG 219 (cluster x1-29 y54-98, unconfirmed), PSG 248 Furball (not located), PSG 268, the PSG 227 windows other than the BBH, PSG 197 (control, tunnel-only), and the ASCII-only hubs (137, 145, 167_20, 173_14, 231, 314_9, 327_12): no run yet. 173_14 and 327_12 are lane-keep cores with no joins (RECON-corpus-anatomy.md), so expect nothing.
- PSG 186 direction inference is weak (65 contradiction tiles); cones saturate at R=30, so its zero-crossing result is a lower bound.
- Hubs outside the sweep filter (ncore < 20) not looked at.

## Tally so far (examined hubs only)
Joiners: 18 uniform (6+8+2+2), 0 mixed (plus 227 control: 2 uniform, 0 mixed). Joins: mixed at 227 E port, 136_7 group and 13_23; uniform at 186, 136_21, 136_5. No joiner mixes forms in any hub checked. Two-form mixing happens between joiners of one group.

## Resume
1. `cd OD/joinforms`; sweep is done (`python3 -I sweep.py`, about 3 min).
2. For each unexamined hub: `python3 -I show.py SAVE BOX 16`, then a second run with R=26 and `pic.py` for a picture; classify per joiner and per out-port group.
3. Locate 219 and 248 boxes (hotspot files, RECON-corpus-anatomy.md lines 82), run show.py.
4. Check that 136 and 13_23 cells feed adjacent out lanes of one port; update the tally; cut to about 1000 words.
