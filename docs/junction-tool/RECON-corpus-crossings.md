# RECON: how coop hubs cross tracks (study S1)

Scripts and raw output: `psg/study1/` (s1.py, sum1.py, s12.txt, s3.txt, pics.txt, out.pkl) on OneDrive.

## Method and caveats
- Hub set: census2.tsv rows that reached the classifier: 95 classified + 199 unclassified = 294 hubs ("hubs" below). Mesh-large (277 boxes) kept apart: those boxes are whole-network bounding boxes full of countryside bridges, so they are context only.
- A structure belongs to the first hub whose box (+3 tiles) holds a head; each structure is counted once. Stitched whole-map tiles, rail bridges/tunnels only (head `far_end` gives the span).
- Crossed track: rail tiles strictly between the heads, grouped into connected tracks (adjacent under-span tiles join when both have an edge across their shared border). Road and water crossings are not counted ("0 crossed" = 18% of bridges).
- Pass-under class per crossed track, deck axis A: perp (only the other axis); diag-S (only diagonal pieces, 2+ tiles = a sideways step, S-bend); diag-1 (one diagonal tile); parallel / parallel+bend (axis-A track on a tile under the span, without / with diagonal pieces under the span = square run); mixed.
- Square-run bends are looked for under the span and 1 tile beyond the run; the walk is straight-only, so bends further out are missed (no run was found with a longer gap).
- Twin: bridge on the adjacent row/column, same axis, overlapping span; stagger = head offsets at both ends of the nearest twin. Bridge decks: all 2237 are axis (0 diagonal), as expected.
- Series: from a far head walk straight outwards along the line through plain track tiles to the next head of the same axis. k = tiles between; k>=8 is mostly unrelated bridges on a long straight, so read the k=1..3 part.
- Not measured: whether S-bend tiles under a bridge are a main-line band edge or a branch; heights.

## Totals (294 hubs: 176 have bridges, 106 have tunnels)
Bridges 2237, tunnels 775.

**1. Length (middle tiles)**
| | 1 | 2 | 3 | 4 | 5 | 6+ | median |
|---|---|---|---|---|---|---|---|
| bridge | 2% | 67% | 9% | 12% | 3% | 5% | 2 |
| tunnel | 8% | 32% | 28% | 13% | 3% | 16% | 3 |

**1b. Crossed rail tracks per structure** (of those with >=1; bridges 1837, tunnels 714)
| | 1 | 2 | 3 | 4 | 5 | 6+ | median |
|---|---|---|---|---|---|---|---|
| bridge | 81% | 12% | 2% | 4% | 1% | 0.2% | 1 |
| tunnel | 53% | 30% | 6% | 8% | 1% | 3% (max 8+) | 1 |

Bridges with 3+ crossers are 7% and almost all are packed (4 crossers on a length-4 span 66 times, 3 on 3 20 times); the corpus max is 6.

**1c. How the crossed tracks pass under (per crossed track; bridges 2430, tunnels 1358)**
| | perp | diag-S | diag-1 | parallel(+bend) | mixed |
|---|---|---|---|---|---|
| bridge | 37% | 61% | 0.2% | 0.5% | 1% |
| tunnel | 67% | 27% | 1% | 3% | 3% |

Per bridge (all crossers one class): diag-S 76%, perp 20%, everything else 4%. Mesh-large context: perp 56%, diag-S 36%, parallel+bend 2%.

**2. Doubling**
| | share |
|---|---|
| bridges with a parallel twin | 92% (91% of those crossing rail) |
| twinned bridges that have two twins (both sides) | 40% |
| stagger (head offset lo,hi) 1,1 (parallelogram) | 62% |
| stagger 0,0 (symmetric) | 31% |
| stagger 0,1 or 1,0 | 6% |
| any stagger above 1 | under 1% |
| unpaired crossed bridges | 171, of which diag-S 85, perp 65 |

Median twin stagger 1. Spans of twinned bridges: 2 tiles 69%, 3 tiles 9%, 4 tiles 12%.

**3. Series (bridge to next bridge on one straight line, 885 links)**
| k (plain tiles between) | 1 | 2 | 3 | 4-7 | 8+ |
|---|---|---|---|---|---|
| bridges | 73% | 7% | 6% | 6% | 9% |
| tunnels (134) | 49% | 13% | 7% | 10% | 11% |

k=0 does not occur in the hub set (3% in mesh-large). The gap tiles carry a signal in 98% of links (k=1: 631 of 648 = 97%). Median k is 1.

**4. Pass-under of the crossed track: S-bend vs square run**
- S-bend under the span (diag-S): 1481 crossed tracks. Square run (parallel/parallel+bend): 12 crossed tracks, 49 groups when counting every run seen by the bend walk (47 with bends found). Runs passing under 2+ bridges between the same bends: 1 of 48 (2%).
- Typical S-bend: a length-2 bridge over a Left/Right (or Upper/Lower) pair, the crossed track stepping one row sideways under the deck.

**5. Grade crossings (X+Y tile)**: 541 tiles in 53 of 294 hubs (18%); median 3 per hub that has any. Mesh-large: 32599 tiles, 258 of 277 boxes.

**6. Tunnels**: lengths above; tracks over a tunnel: 1: 53%, 2: 30%, 3+: 17%; of the tracks above, 67% cross at a right angle, 27% as S-bends.

## Comparison with the tool's rules
1. Doubled main-line bridges: coop agrees (92% twinned), with the parallelogram (stagger 1 at both heads) as the majority form (62%) and the symmetric form (31%) second; a one-end-only stagger is rare (6%). Staggers above 1 are essentially absent. Matches the rule that the shape is free and copy 1's heads are one tile off.
2. Cap of 2 crossers: coop does it up to 2 in 93% of crossing bridges, but 7% cross 3-6 on one span (packed). The cap is stricter than practice, not contradicted by it. Tunnels cross more (17% with 3+).
3. Series gap of 1: clear agreement. Where bridges chain, 73% have exactly one plain tile between, signalled in 97% of them. Gap 0 never occurs in hubs.
4. S-bend vs square run: coop practice is overwhelmingly the S-bend under the span (61% of crossed tracks, 76% of crossing bridges) with perpendicular straight passes the other main form (37%). The square run is about 0.5% of crossed tracks. This clearly differs from the tool, which offers both forms as equal choices; the S-bend should be the preferred form and the square run a fallback.
5. Perp straight crossings are common (37%) and the tool's rules only describe band edges; grade crossings (X+Y) do occur in 18% of hubs while the tool never builds them.

## Example hubs
| pattern | hub | box |
|---|---|---|
| doubled length-2 bridges over S-bends, series k=1 | psg159_54 | 696,103,726,121 |
| same, other hubs | psg167_30, psg144_7, psg159_52 | 224,244,253,263 / 483,259,501,290 / 197,103,227,122 |
| doubled bridge over perpendicular track, plus tunnels | psg13_31 | 827,133,866,164 |
| single bridge over 2 perpendicular tracks | psg134_3, psg162_5 | 79,44,103,63 / 87,488,119,512 |
| tunnel over 2 perpendicular tracks | psg173_16 | 140,94,186,123 |
| square run (parallel+bend) | psg175_16 (bridge 186,284 to 186,289), psg314_15 | 170,268,220,315 / 399,389,478,467 |
| grade crossings | psg04_23 (10 tiles) | 238,270,256,303 |

Pictures (C1 legend; B head, b span, s signal tile, `< > ^ v` diagonal pieces, `+` X&Y). Full text in `psg/study1/pics.txt`.

psg159_54: doubled bridges over S-bend pairs `Bv^B`, staggered by one, series with a signalled tile.
```
 109s-s-s-s-s-s-s-s#s---------s-s-s
 110..........s-sBvsBs-#s----------
 111s-s-s-s-s-#sBv^Bs-#^...........
 112........s-sBvsBsBvsBs-#s-------
 113s-s-s-s-#sBv^BsBv^Bs-#s........
 114......s-sBvsBsBvsBsBvsBs#s-----
 115s-s-s-#sBv^BsBv^BsBv^Bs#s......
```
psg13_31 (tunnels `T` under tracks, doubled `BBBB` over a vertical track):
```
 152......ss||...............vT||T-T........
 153......BBBB..............v^.|<>..........
 154......bbbb..............T..<><>.........
 155....vs----s-s-s-s-s-s-s-s>..<><s........
 156...v^.bbbb..............T<>..<s#s-s-s-s.
 157...s..bbbb..............|.<s-s-s-s->..<>
 158...|..BBBB..............|..........<s..s
```
psg175_16 (crop x178..194 y278..295; the square-run bridge is the one at x186 y284..289, rows near `BB`, `Bb`, `Bv`):
```
 283..s.ss<>sss||s|.s
 284.v#.<#.|BB|ss|s.|
 285vss..s.|Tbs||s|.s
 286^.#s.#s||v^ss|s.|
 287..s|.s||#^.||s|.s
 288..||.|B#^b.ss|s.|
 289..||.BvsBB.||s|.s
```

## Undone
- Square-run bends beyond 1 tile past the span are not followed (the walk stops at the first non-axis tile); runs with a longer approach are undercounted.
- Doubled-versus-single is measured by geometry only, not by main-line versus branch role.
- Crossers are not split by hub role (band edge versus branch), so "crossers on a band edge" is not separate.
