# RECON: where the composer decides sideways position

All lines are `src/junction/planar/planar_components.cpp` (wt-planar a8b146d1ff) unless stated. Units are half-tiles; 1 slot = 2. "Mapped" means reachable from `SolveMappedVariant` -> `MappedComposer::Build` (:1151).

## Table

| # | file:line, function | decides | inputs | slot replacement | axis / diag | mapped? |
|---|---|---|---|---|---|---|
| 1 | :1140 `JoinEntry` (used :1287-1288) | core lane offsets from far lane: 0,1,3,5.. (lane j: 2j-1 lines), mirrored when far lane is on the right | n, `res[join].straight/prio/side` | core `post` (and `pre`) slot differences between consecutive lines; the "each crossed lane steps one line out" is the Copy/Row presence in `x<k>`/`post` plus the >=1 / >=2 gap rules | axis (cores are one-heading) | yes |
| 2 | :1043 `Flex` (called :1295) | equalities `Dot(r,last[j].a/.b)-Dot(r,last[0])=2*offs[j]` (:1060-61); optional required diagonal `Turn(h,jog)` per lane, equal lengths (:1054, :1057); piece is a required diagonal when `jog!=0` | lanes, h, offs, jog | offs = `post` slot differences of the core channel (or `pre`); jog = link/line step between `pre` and `post` of the core's channel. Its equalities become `slot(post,j)-slot(post,0)` | axis lanes, diagonal jog | yes (cores) |
| 3 | :1292-1294 jog choice in `build_core` | variant bit `steps` -> `jog = cand.toward` | `cand_steps` (`PlanarCandidateSteps` :1185), `variant.steps` | the line's slot change pre->post (its charged step); the bit disappears when slots decide the step | n/a (decides a diagonal) | yes |
| 4 | :1070 `Bundle`, lead :1081 and offsets :1101-1105 | `Eq(Dot(r,piece_segs[c][lane j][k].a)-Dot(r,lane0..), 2j)` and the same on `tails[j].b`: lanes adjacent (1 slot) on every piece | channel lanes, heads (path headings) | consecutive tracks differ by exactly 1 slot becomes the slot difference in the interval that piece lies in (pre/mid/post; x if the piece carries the deck) | axis and diagonal (`Dot(r,.)` along the piece's own Right) | yes |
| 5 | :1083-1100 doubled piece in `Bundle` | parallelogram: left lane's copy goes -1, right +1 (`sg`, :1087), step 2 exactly, deck, then back step `-sg` (:1096) exactly 2 | `doubled` set (FullLanes==2, 2 lines, axis piece, :1255-1269) | `Copy` track in `x<k>` at slot line+-1 (copy exactly 1 from line). The out-and-back steps (2,2 pieces) are the copy's fork/rejoin geometry: no sideways slot change of the line | axis only (refused on diagonal :1265) | yes |
| 6 | :901 `Double` (calls :960, :995, :1000, :1356) | lane deck, step `sg` exactly 2, copy mirrors, lane ends on copy line (:909-910); caller adds back-step | h, sg, suffix | as row 5: `Copy` interval tracks; `sg` = copy side (kind Copy, slot +-1) | axis | yes |
| 7 | :1344-1361 `build_core` copy sides | `away = far_left?+1:-1`; far lane `-away`, others `away` (D312); back-step :1358 | `far_left` from join side, `FullLanes` | same rule exists in slot layer (copy beside full-share line, far line outward) | axis | yes |
| 8 | :1363-1366 `build_core` run | `Eq(Dot(Right(h),run[j].a)-run[0].a, 2*offs[j])` post-deck run | offs | core `post` slot differences | axis | yes |
| 9 | :932 `Fray`: prio fan :940, fan run :941 | required piece `Lt=Turn(h,-1)` length 2 (cost 1) then FANW run; plain fray has none | `prio` | prio-fray `pre` -> last interval step; fan toward = a link step (`links`) | axis, fan diagonal | yes |
| 10 | :950-953 fray gap | `Ge((Dot(r,sp[k])-Dot(r,sp[k-1]))*sigma, prio?4:2)`: split points 2 (plain) or 4 (prio) apart | sigma = site side | slot rule "between lines in last interval of a prio fray's channel >=2" (4 half-tiles); plain: >=1 | axis | yes |
| 11 | :954-969 fray legs and decks | S lanes straight, V leg `Turn(h,sigma)` 2; prio: `Double(S[j],-sigma)` (:960) + PairDecks; plain: deck d0 only | sigma, prio | V leg = the other channel starting; copy = Copy tracks in the fray channel's crossing interval | axis S, diagonal V | yes |
| 12 | :974 `Join` n-lane: joiner lo :987-988 | J[k] diagonal `Turn(h,-sigma)` length lo (6, 4 for prio k>0, `lo0` k=0); exact (`hi=lo`) when n==2 | prio, n, lo0 | joiner diagonal lengths are crossing/landing geometry. Land line step = `links` entry of the site | diagonal | yes |
| 13 | :994-1002 prio join doubling | one `Double(S[j],h,sigma)` per crossed joiner (:995), then `AddSeg(S[j],Turn(h,-sigma),2,2)` (:997) | j, P | Copy in `x<k>`/`mid<k>` of the straight channel; steps are copy fork/rejoin | axis | yes |
| 14 | :1017-1022 prio close-up | `prio_rows` per lane (:1018); `O[j]` step `Turn(h,-sigma)` exact length `2j` (:1021) | j | `Row` track beside lane in `post`; the close-up of j lanes = line's step to its out slot (j slots, 2j half-tiles) | diagonal 2j long | yes |
| 15 | :1024-1029 exit steps | `O[k]` piece `Turn(h,exit_step)` length>=2, FANW (:1026-27); ordering `Ge(Dot(STEP,a.a)-Dot(STEP,b.a),0)` (:1036) so inner steps no earlier | exit_step from gap steps (:1397-1399) | site link step (`links`) for the out lines, or `post` rank step for port lines | diagonal | yes |
| 16 | :1038 join out gap | `Eq(Dot(r,End(O[k+1]))-Dot(r,End(O[k])), 2*sigma)`: out lanes exactly 2 apart after the join | sigma | out channel `pre` slot difference = 1 | axis | yes |
| 17 | :853 `Fork` | two optional diagonals (`opt`, z), `Ge(Dot(r,b.b)-Dot(r,a.b),2)` (:861) | lane split (:1412-1417) | no slot site for lane split choice (slot layer has links only for straight lines across sites); candidate: pre->post step of each leg, but the optional z (binary) is a variant bit | diagonal | yes |
| 18 | :1420-1434 `build_nodes` one-lane join | `Join(h,side,{S},{J},{O},prio,step,lo0=2)`; tie bit `variant.ties` (:1423); step from `PlanarMergeExitStep`/`PlanarInnerMergeExitStep` (:1433) | node legs | per-node: link between S and O lines (`links`); tie = resolved sites `res` | diag | yes |
| 19 | :1180 `BuildPlanarGapSteps` -> used :1397-1399 only | `toward` of a join channel's close-up; `fan` entries are not read (fan hard-coded :940) | `ch`, `res` | gap steps = `links` + `post` rank steps; replace 1:1 | diag | yes |
| 20 | :1185 `PlanarCandidateSteps` | `toward` of a candidate jog (row 3) | ch | as row 3 | diag | yes |
| 21 | :1557-1574 fan rule, `sep` :1539 | up-front inequalities: a lane right of another keeps its fan and legs `>=2` right/ahead of the other's stub/fan (`f = Right+STEP`) | port lines | port `pre` rank order (slot order) makes the side explicit; these are clash separations (not equalities), keep as hard rows | axis, diag | yes |
| 22 | :1576-1604 side rule | joiner's last two segments `>=2` on its own side of a neighbouring in port's lines | res side, ports | `post`/`pre` rank order gives sides; same note | axis, diag | yes |
| 23 | :1707-1725 port order | `Eq(point_k diff, 4 first of port else 2)` (ports 2 tiles, lanes adjacent) | port lines, reading order | `post`/`pre` slot = free offset + rank (already a slot rule); ports gap 4 = 2 slots not in slots | axis | yes |
| 24 | :1199 `PortIn` (:880-888) | port-line start on box edge | side | none, boundary | axis | yes |
| 25 | :1130-1135 `UnderSegs`, :1466-1514 deck groups, :1516-1535 under-deck | which seg a crossing sits on (before/after own deck); not sideways | crossing order | none: crossing geometry. Gives the x<k> segment mapping, see below | both | yes |
| 26 | :363 hand `Join` (2-lane), :516 `DoubledB`, :162 `Composer` | same patterns as 5, 12, 14, 16 with constants; `Flex` hand :429 | T2/LL2 | n/a | | no: only `SolvePlanarComponents` T2/LL2 (:2818-2830); the mapped path never calls them |

## How lane positions are expressed today

- Points are `Point` of `Ex` (`Var` x,y in half-tiles) in `planar_model.h`; `Seg.a/.b` end points, `Seg.L` length in half-tiles, `d` heading 0 N .. 7 NW. `STEP[h]` is the unit step, `Right(h)` the right-of-travel step (`planar_model.h:45-70`). Lateral position of a point is `Dot(Right(h), p)` (`:160`), in half-tiles (a tile is 2 half-tiles; diagonal piece 1 each axis).
- Worked example: two lanes L0, L1 on axis heading h, lane L1 one slot right of L0. `M.Add(Eq(Dot(Right(h), laneSeg1.a) - Dot(Right(h), laneSeg0.a), 2))`. Rows that express it: `Flex` :1060 (`offs[1]=1` -> 2) and :1061 for `.b`; `Bundle` :1103/:1104 (`2*j`); `build_core` :1365 (`2*offs[j]`); out lanes of a join :1038 (`2*sigma`); fray gap :952 (`>=2`/`>=4`).
- Slot s -> half-tiles `2*s` along the segment's own `Right`; on a diagonal the equality still uses `Dot(Right(h), .)`.
- Sideways S-bends are pieces (`AddSeg(l, Turn(h, k), lo, hi)`); required with a length floor, `hi==lo` for exact (:988, :1021).

## How the composer would read `PlanarSlots`

- In `MappedComposer::Build`: `this->paths` is built at :1160 (`BuildPlanarPaths(t, ch, variant.over, shape)`), then `res = ResolvePlanarSites(t, ch, variant.ties, why)` at :1171, and the slot layer can run right after :1171 (before `gap_steps` :1180) with
  `const PlanarSlots slots = BuildPlanarSlots(t, ch, res, this->paths.over);`
  `t` = member `this->t`, `ch` = member `this->ch` (already the flipped channels, since the caller passes `flipped[variant.flips]` :2902/:2966/:3020), `res` = local `const std::vector<PlanarResolvedSite> res` (:1171), over = `this->paths.over` (per channel crossing, the over channel; NOT `variant.over`, which is a bit mask).
- `slots.channels[c]` is indexed like `ch.channels` (verify: `PlanarSlots::channels` is per channel). Intervals per channel in travel order: `pre`, `x0`, `mid0`, `x1`,..., `post`; no crossing: `pre`, `post`. A `Row` is only in a prio straight channel's `post`; a `Copy` only in `x<k>` where the channel is over and the line is full-share. `slots.links` is per site (line before, line after, step).

## Intervals -> composer segments

- Core (one heading, `build_core`):
  - `pre` = the `Flex` lead run, `piece_segs[c][lane][0]` (:1297) (the diagonal `jog` is between earlier part and `last`; the run `last` is the lead).
  - `x<k>`: all decks of the channel sit on one segment per lane: `d0` (single, :1347) or the doubled `d0`/`d1` pair (:1356) with `wire_decks` (:1309-1341), decks chained with `Ge(n0 - prev_f0, 2)`; `x<k>` = the deck k on that segment, `mid<k>` = the stretch between consecutive decks on the same segment (no separate segment), so the slots of `x<k>` and `mid<k>` are on `d0`'s line plus the Copy (`d1`, one slot off).
  - `post` = `run[j]` (:1363) = `piece_tails[{c,0}]`; returned stored under :1366. The steps :1357-1358 (`Turn(h,-sg)`) are the copy rejoin before `post`.
- Bundle (several headings):
  - Piece k = heading `heads[k]`; per lane `piece_segs[c][lane][k]` = the lead of piece k (:1082). The crossing i of channel c sits on piece `paths.paths[c].crossing_piece[i]`.
  - On an undoubled piece the interval `x<k>` is that lead segment; `pre` is the earlier pieces plus this lead's start; `post` the later pieces.
  - On a doubled piece: `x<k>` = `dbl_decks[{c,piece}][lane].first/.second` (d0/d1, :1098); `post` begins at `piece_tails[{c,piece}][lane]` (:1099, `tails`).
- Under channel crossed: `UnderSegs` (:1113): the crossing sits on `piece_segs[u][lane][upiece]` (before the piece's own decks) or `piece_tails[{u,upiece}][lane]` (after), by crossing order (:1121-1127).
- Open: `channel` indices of slot layer vs composer assumed identical; not checked in code (only `planar_slots.h:48` read).
