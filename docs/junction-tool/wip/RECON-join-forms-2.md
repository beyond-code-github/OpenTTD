# Recon: join forms, second pass (complete 2026-11-20)

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
| PublicServerGame_136 box 22,60,53,143 (psg136_7) | 6 cells, x30-43, y100-126 | entry-gated joiners at 37,100 / 40,103 / 43,106: doubled bridge pairs UNDER (2 each). path_oneway joiners at 30,114 / 36,120 / 39,123 / 42,126: doubled bridge pairs OVER (2 each) | yes (7 of 7; earlier text said 6, rerun lists 7 joiners) | each cell uniform; the group of 6 cells is mixed (UNDER cells and OVER cells by gate type) |
| PublicServerGame_136 box 0,354,21,434 (psg136_21) | 8 cells | all UNDER, doubled bridge pairs, 2 crossings each | yes (8 of 8) | yes |
| PublicServerGame_136 box 463,45,544,117 (psg136_5) | 2 | both UNDER, doubled bridge pair / single | yes (2 of 2) | yes |
| PublicServerGame_13 box 25,96,58,120 (psg13_23) | 2 joiners (gates 35,104 and 36,104) | one tunnel 35,107-35,111: joiner 35,103y UNDER, joiner 36,103y OVER | yes (2 of 2) | NO (the two joiners of one tunnel differ) |
| PSG 314 box 181,43,246,99 (psg314_5) | 2 joiners, merges 39 tiles apart | merge 237,52 joiner 236,52l (entry): 2 bridges 228-229,54-58 OVER. Merge 198,60 joiner 198,61l (path_oneway): bridge 200,67-76 UNDER at R16; at R26 four bridges (200,67-76; 199,68-77; 197,71-77; 196,72-77), all UNDER | yes, yes | yes (separate joins; each uniform) |
| PSG 314 box 320,378,382,434 (psg314_13) | 2 joiners | merge 355,408 joiner 354,408l (entry): doubled bridge 343-347,405 OVER (R16 and R26). R26 adds merge 356,411 joiner 355,411x (entry): bridges 339,404-409 and 340,404-409 UNDER | yes, yes | NO if the two merges are one group (R26 only; unchecked) |
| PSG 314 box 399,389,478,467 (psg314_15) | 1 joiner | merge 436,425 joiner 437,425r: bridges 443,417-424 and 445,421-424 OVER | yes | yes |
| PSG 314 box 95,453,152,505 (psg314_18) | 1 joiner (R16), 2 (R26) | merge 147,468 joiner 146,468d: doubled bridge 138-139,464-467 OVER. R26: merge 147,472 joiner 146,472d: doubled bridge 134-135,467-470 OVER | yes, yes | yes |
| PSG 173 box 752,881,803,934 (psg173_68) | 1 | merge 789,889 joiner 788,889l: bridge 779,887-892 OVER | yes | yes |
| PSG 09 (PublicServerGame_09_Final) box 173,571,233,617 (psg09_13) | 1 joiner (R16), 2 (R26) | merge 179,592 joiner 180,592u (combo): tunnel 190-193,592 OVER. R26 adds merge 178,590 joiner 179,590x (combo): tunnel 194-197,590 UNDER | yes, yes | NO if the two merges are one group (R26 only; unchecked) |
| PSG 197 box 81,141,123,198 (control, tunnels only) | 7 prio joiners | 0 joiner-main crossings at R16 and R26 (also box 76,137,127,198) | n/a | n/a |

All rows VERIFIED as script output (OD/joinforms/show.py, `python3 -I show.py SAVE X0,Y0,X1,Y1 R`, R=16 and R=26 both run for every new row; outputs in the session scratchpad joinforms/). No joiner in any hub lists crossings of two forms at either R, so no joiner needed a hand check. Where a row says R26 adds a joiner, that joiner had no crossing at R16; the joiners found at R16 kept the same form at R26.

## Port groups (step 5)
- 13_23: merges 35,102 and 36,102 are adjacent tiles, one tunnel 35,107-111 under both joiners' lines (35,103y UNDER, 36,103y OVER). Adjacent merge tiles with a shared combo gate row (gates 35,104 / 36,104). VERIFIED that the merges are adjacent; INFERRED that they are two adjacent out lanes of one port (pic.py not read at lane level).
- 136_7 and 136_21: cells step 3 tiles diagonally (37,100 / 40,103 / 43,106 ...), each an S-bend bridge pair; a ladder of single merges, not obviously one multi-lane join. INFERRED (picture only).
- 314_13: merges 355,408 and 356,411, 3 rows apart with line tiles between (pic 336-362,402-416). INFERRED separate merges; whether they leave one port is not decided.
- 09_13: merges 178,590 and 179,592, 2 rows apart, parallel E-W lines with one spacer row (pic 174-200,584-598). INFERRED possibly adjacent out lanes with a spacer; not decided.
- 314_5: merges at x237 and x198, 39 tiles apart: separate groups. VERIFIED by coordinates.
- 314_18 R26: merges 147,468 and 147,472 both OVER, uniform either way.

## Tally (all examined hubs)
- Joiners with crossings: 32 uniform, 0 mixed (227 control 2; 186 1; 136_7 7; 136_21 8; 136_5 2; 13_23 2; new 314_5 2, 314_13 2, 314_15 1, 314_18 2, 173_68 1, 09_13 2; counts use R=26 where it adds joiners). VERIFIED (script output).
- Port groups: 7 uniform (186, 136_21, 136_5, 314_15, 314_18, 173_68, 314_5), 5 mixed: 227 E port (VERIFIED, same port), 13_23 (VERIFIED forms, adjacency INFERRED), 136_7 (INFERRED group), 314_13 and 09_13 (VERIFIED forms at R26, grouping INFERRED). In every mixed case the forms differ between joiners, never within one.
- Form tracks gate type only in 136_7 (entry UNDER, path_oneway OVER); 13_23, 09_13 mix within one gate type (combo/combo) and 314_13 within entry/entry. Evidence only.

## Separator view (coordinator clarification)
Separator = the line carrying the structure: joiner when it is on the bridge deck or in the tunnel, main when the main is on the deck or in the tunnel. From the script's (form, kind): OVER bridge = joiner separator (J); UNDER tunnel = J; OVER tunnel = main on the tunnel, main separator (M); UNDER bridge = joiner passes beneath the main's bridge, M. VERIFIED mapping from script output; the J/M reading of OVER-tunnel and UNDER-bridge follows from the form definitions in Method.

| hub, merge / joiner | kind | form | separator |
|---|---|---|---|
| 227: 379,339 / 378,339l | tunnel 371-375,338 | UNDER | J |
| 227: 378,343 / 377,343x | bridge 372,334-339 | OVER | J |
| 186: 174,243 / 174,244u | bridge 172,248-260 | OVER | J |
| 136_7: 37,100 / 40,103 / 43,106 (3 entry joiners, 2 crossings each) | bridge pairs | UNDER | M |
| 136_7: 30,114 / 36,120 / 39,123 / 42,126 (4 path_oneway joiners, 2 each) | bridge pairs | OVER | J |
| 136_21: 8 entry joiners (10,375 ... 4,426), 2 each | bridge pairs | UNDER | M |
| 136_5: 509,87 (2) and 510,110 (1) | bridge | UNDER | M |
| 13_23: 35,102 / 35,103y | tunnel 35,107-111 | UNDER | J |
| 13_23: 36,102 / 36,103y | tunnel 35,107-111 | OVER | M |
| 314_5: 237,52 / 236,52l | 2 bridges | OVER | J |
| 314_5: 198,60 / 198,61l | 1 bridge (R16), 4 (R26) | UNDER | M |
| 314_13: 355,408 / 354,408l | doubled bridge 343-347,405 | OVER | J |
| 314_13 (R26): 356,411 / 355,411x | bridges 339-340,404-409 | UNDER | M |
| 314_15: 436,425 / 437,425r | 2 bridges | OVER | J |
| 314_18: 147,468 / 146,468d (R26 also 147,472 / 146,472d) | doubled bridges | OVER | J |
| 173_68: 789,889 / 788,889l | bridge 779,887-892 | OVER | J |
| 09_13: 179,592 / 180,592u | tunnel 190-193,592 | OVER | M |
| 09_13 (R26): 178,590 / 179,590x | tunnel 194-197,590 | UNDER | J |

Within every joiner the kind and separator are the same on all its crossings (VERIFIED from the rows).

Separator tally: joiners 32 uniform, 0 mixed. Port groups: 8 uniform (227 E port, 186, 136_21, 136_5, 314_15, 314_18, 173_68, 314_5), 4 mixed (136_7, 13_23, 314_13, 09_13). The separator view dissolves the 227 E-port mix (both joiners are separators) but not the other four. Group membership remains as marked above (INFERRED for 136_7, 13_23 adjacency, 314_13, 09_13).

## Not done
- PSG 219 (cluster x1-29 y54-98, boxes 1,54,29,98 and halves): census2 analyse_box refuses ("analyse_box: station", station tiles in box); unconfirmed hub. PSG 248 Furball: not located (no hotspot box for 248; whole-save census is mesh-large). PSG 268: only a mesh-large 0,0,510,516 census row, no hub box. PSG 197: run, no crossings in joins (consistent with the first recon).
- ASCII-only hubs (137, 145, 167_20, 173_14, 231, 314_9, 327_12): no save-resolved run; 173_14 and 327_12 are lane-keep cores with no joins.
- PSG 186 direction inference is weak (65 contradiction tiles): its zero-crossing result is a lower bound.
- Hubs outside the sweep filter (ncore < 20, box > 140) not looked at; the 12-step gate walk and R heuristics unchanged.
- Whether any group is a multi-lane prio join in the engineer's sense is only INFERRED above.
