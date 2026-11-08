# SUMMARY (measured, no interpretation)

Units: straight piece = 1 tile, diagonal piece = 0.5, bridge head = 1 + span. Signals: B block, N entry, X exit, C combo, P pbs, O pbs_oneway. dist = track length to the platform edge. Full detail in <slug>.md.

### 1 Platforms
| | Buborough | Frindworth | Fledhattan | Chinningpool |
|---|---|---|---|---|
| main platforms (axis, length) | 8 (Y, 5) | 8 (Y, 5) | 16 (Y, 5) | 12 (X, 5) |
| cross-axis lines | 79-83,87-89 | 70-77 | 414-429 | 495-506 |
| stray/short tiles | none | 4 tiles (X1, Y1, X1, X1) | 1 tiles (Y1) | 2 tiles (X2) |

### 2 Direction
| | Buborough | Frindworth | Fledhattan | Chinningpool |
|---|---|---|---|---|
| travel dir (all platforms same?) | SE (all same) | SE (all same) | NW (all same) | SW (all same) |
| signal score min..max | 6..8 | 2..5 | 3..8 | 9..12 |
| one-way PBS (O) at entry | 6/8 platforms | 8/8 platforms | 3/16 platforms | 4/12 platforms |

### 3 Entry side (per platform arm)
| | Buborough | Frindworth | Fledhattan | Chinningpool |
|---|---|---|---|---|
| bay signal->platform | 1 | 1 | 0..4 | 3 |
| first signal type | OP | O | OX | O |
| arms holding TL | 0/8 | 0/16 | 0/7 | 0/4 |
| platform->first split | 1 | 1 | 1..4 | 0.5..3 |
| platforms with 2 feeding arms | 0 | 8 | 0 | 12 |

### 4 Entry tree
| | Buborough | Frindworth | Fledhattan | Chinningpool |
|---|---|---|---|---|
| tree roots (maximal reach) | 1 (reach 8) | 1 (reach 8) | 4 (reach 4,4,4,4) | 5 (reach 12,12,12,12,12) |
| box-edge leaves / other leaves | 4 / 0 | 2 / 2 | 8 / 0 | 4 / 0 |
| split levels root->platform | 2..3 | 3..5 | 1..3 | 2..4 |
| split dist from platform (P0 path) | 1(2) 9(2) 17.5(2) | 1(2) 12(2) 42(2) | 4(2) | 3(2) 10.5(2) |
| root dist from platform | 15.5..18.5 | 43..45 | 3.5..4.5 | 11.5..29.5 |
| bridges inside tree / at leaves | 0 heads / 4 | 16 heads / 0 | 0 heads / 4 | 22 heads / 0 |

### 5 Balance
| | Buborough | Frindworth | Fledhattan | Chinningpool |
|---|---|---|---|---|
| every leaf reaches every platform | no | no | no | yes |
| leaf reach sizes | 5,5,8,8 of 8 | 4,8,4,4 of 8 | 4,4,4,4,4,4,4,4 of 16 | 12,12,12,12 of 12 |
| independent groups (max reach sets) | 1 x 8 | 1 x 8 | 4 x 4/4/4/4 | 1 x 12 |

### 6 Path length root->platform (per platform min..max)
| | Buborough | Frindworth | Fledhattan | Chinningpool |
|---|---|---|---|---|
| values over platforms | 15.5..18.5 | 43..45 | 3.5..4.5 | 10..29.5 |
| each platform min==max | yes | yes | yes | no |
| all platforms equal | no | no | no | no |

### 7 Exit side
| | Buborough | Frindworth | Fledhattan | Chinningpool |
|---|---|---|---|---|
| first signal dist,type | 1 B | 1 B | 1..2 B | 0.5..1 B |
| dist platform->first merge | 2..11.5 | 2..5 | 2..4 | 2..4.5 |
| outgoing-line roots (feed sets) | 2: {0-3}; {4-7} | 2: {0-3}; {4-7} | 4: {0-3}; {4-7}; {8-11}; {12-15} | 3: {0-3}; {4-7}; {8-11} |
| merge states / prio (N,X,C) at merges | 5 / no (block) | 6 / no (block) | 12 / no (block) | 12 / no (block) |

### 8 Stagger (offset along axis vs neighbour, platforms by line)
| | Buborough | Frindworth | Fledhattan | Chinningpool |
|---|---|---|---|---|
| entry offsets | +0 +0 +0 +0 -2 +0 +0 | +0 +0 +0 +0 +0 +0 +0 | +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 | +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 |
| exit offsets | +0 +0 +0 +0 -2 +0 +0 | +0 +0 +0 +0 +0 +0 +0 | +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 | +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 |

### 9 Footprint
| | Buborough | Frindworth | Fledhattan | Chinningpool |
|---|---|---|---|---|
| tiles: platform+entry+exit = total | 40+56+71=167 | 40+137+32=209 | 80+52+76=208 | 60+254+48=362 |
| bbox x,y (size) | 79..92, 918..949 (14x32) | 54..77, 901..946 (24x46) | 402..429, 84..100 (28x17) | 54..86, 487..506 (33x20) |

