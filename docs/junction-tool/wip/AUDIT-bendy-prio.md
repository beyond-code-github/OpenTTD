# Audit: straight/axis-only assumptions about prios (bendy prio readiness)

Read-only audit of the worktree /home/pete/OpenTTD/.claude/ng1. No builds, no edits. Nothing was run.
Terms: "axis piece" = X/Y trackdir (IsDiagonalTrackdir). "half piece" = Upper/Lower/Left/Right (the 45-degree pieces). "opp-half row" = row on the opposite half of an ML half-piece tile.
CONFIRMED = I read the code path and the failure follows. SUSPECT = plausible from the code, not traced to the end.
File prefixes: R = src/junction/junction_rules.cpp, S = src/junction/signals/junction_signals.cpp.

## A. CheckPrio (R:1213-1520)

A1. CONFIRMED. R:1361-1364 `if (!p_tiles.contains(key)) { row.push_back(key); continue; }`
Assumes a row tile is never an ML tile. `row` holds only tiles off the ML path.
Bendy: an opp-half row on an ML tile is never put in `row` (it takes the R:1392-1396 `unused[key] == TrackBits{opposite}` `continue`). Everything keyed on `row` then ignores it:
- the row signal collection at R:1480-1482 (`std::ranges::find(row, ...)`). A row signal on an ML tile is not in `prio_signals`, so it is never checked for Exit/Combo or facing.
- the "exactly one prio signal between consecutive segments" count at R:1503-1507 then reads 0 and gives a false violation.
- the `row_blocks` "prio block needs exactly one join" check at R:1455.
Fix: build `row` from the unused bits of coverage tiles, not from off-path tiles.

A2. CONFIRMED. R:1392-1396 `if (!IsDiagonalTrackdir(p_piece.trackdir)) { ... opposite ... if (unused[key] == TrackBits{opposite}) continue; }`
Assumes at most one ML piece per tile. `piece_index_of` (R:1337-1342) returns the first ML piece on the tile. It also assumes the opp-half row is the only unused content of an ML half tile.
Bendy: this is the ruling's allowed case, but it is whitelisted only for a tile with exactly one unused bit equal to the opposite half. A tile with any extra unused bit gets "prio track on the ML is not a streamlined join". Examples are an S-bend tile where the row also touches an inner-corner half of the same tile, or a tile shared with a neighbouring ML lane.
Also nothing checks that the opposite half actually connects into the rest of the row: the tile is skipped and R:1409-1420 (step 2) is the only remaining test.

A3. CONFIRMED. R:1378 `if (TracksAt(layout, to_tile(key)) != join_tracks || unused[key] != TrackBits{h1}) continue;`
Assumes a join tile holds exactly {axis, h1} (plus the joiner on the merge tile), so the ML axis tile holds one unused bit.
Bendy: an axis ML tile that also carries a row half (the row doubling back through the same tile in an S-bend) is never a join.

A4. CONFIRMED. R:1382-1383 `other_tracks == row_tracks` with `row_tracks = {piece_axis, h2}` (or `{h2}` on the merge tile)
Assumes the row tile at the join is a plain straight row tile holding the same axis as the ML piece.
Bendy: next to a bend the row tile holds half pieces (the inner-corner tile, which two neighbours share). It cannot equal {piece_axis, h2}, so no join is found there. A join adjacent to a bend is rejected as "prio track on the ML is not a streamlined join" or "the first prio join must be on the merge tile".

A5. CONFIRMED. R:1331-1335 `axis = TrackdirToTrack(merge_piece.trackdir)`, `along_x = axis == Track::X`, `along = ...`
Assumes the whole coverage runs on the merge piece's axis. `along()` is the one coordinate used for every ordering below.
Bendy: after a bend the ML runs on the perpendicular axis, so `along()` is not monotone with walk order. Three consequences:
- R:1488 `farthest = max |along(sig) - along(merge)|` and R:1496 `wanted = ... == farthest ? Exit : Combo` pick the wrong Exit. Two signals on a perpendicular run tie.
- R:1498 `between = pos > front && pos < back && not a join position` fails when two joins or a signal share a coordinate. The column-equal case is likely in a tight S-bend (a signal on the Y-piece column equal to a join column).
- R:1503 `pos > join_positions[k] && pos < join_positions[k+1]` counts 0 signals between equal join positions, giving the false "exactly one prio signal" violation.
Fix: order by walk index on the ML (distance in `ml.pieces`), not by one coordinate.

A6. CONFIRMED. R:1492-1494 `faces_upstream`: an ML axis piece at Manhattan distance 1 with `ReverseTrackdir(piece.trackdir) == signal->facing`
Assumes a row signal sits on a tile beside an ML axis piece and faces along an axis trackdir.
Bendy: a row signal on an ML half tile (opp-half row) is at distance 0. A row signal on an inner-corner tile faces a half-piece trackdir, which never equals an axis trackdir. Both are "prio signal does not face upstream" (the first only if it is counted at all, see A1).

A7. CONFIRMED. R:1115 `if (std::ranges::any_of(layout.signals, ... SameTile(s.offset, jt.offset))) continue;` (ComputeBlocks)
Assumes one relevant track per tile for block topology. A tile holding any signal is dropped from the block graph whole, and a tile is one node whatever tracks it holds.
Bendy:
- an opp-half tile holding a row signal on Lower also removes the ML's Upper from every block (and the reverse), though the game keeps the two halves apart for signalling (SettleSignals does handle this per track).
- an ML tile and its opp-half row tile are one node, so ML block and row block merge when no signal separates them.
Effects: `blocks.find(...)` fails for the ML piece (`BlockOfPiece` returns nullopt, so the block is skipped at R:1439-1441), and the merge-block component set at R:1297-1302 and the R:1437-1458 join-per-block counts are wrong. G8 CheckDeadlock (R:1150ff) also uses these blocks.
Fix: make block nodes (tile, track-group), with Upper/Lower and Left/Right parallel halves as separate nodes.

A8. CONFIRMED (lenient direction). R:1448 `if (TracksAt(layout, piece.tile) == axis_bits) joinable = true;` and R:1446-1450
Assumes a joinable ML tile holds exactly the merge piece's axis.
Bendy: a perpendicular-axis tile (after a bend) or a tile holding an opp-half row is never "joinable". A block with 0 joins is then treated as unjoinable and silently accepted instead of reporting "ML block within the train length needs exactly one join".

A9. SUSPECT. R:1409-1420 (step 2, "component touches nothing used")
Assumes unused bits touch used tracks only at joins. It tests by tile-edge adjacency: any other-tile track touching the reverse edge that is not an unused bit of the component is a violation.
Bendy: an opp-half row half (e.g. Lower touching SE and SW) can meet an edge of a neighbour tile that holds a used ML half or another lane's track. This is likely in the S-bend, where the neighbouring ML tile touches the row's half edge. That would be a "prio track touches a used track" false positive. Not traced against a concrete layout.

A10. SUSPECT. R:1240-1246 union-find (node = tile, "all unused bits of a tile are one node")
Assumes one connected unused component per tile.
Bendy: two unused fragments on the same tile (the opp-half row plus an inner-corner row half of another stretch, or another prio's row crossing a shared ML tile) are one node. This can merge two prios' components ("more than one prio component", R:1313, or a wrongly joined component).

A11. SUSPECT. R:1437, R:1469 `2u * ctx.spec.prio_tiles` vs S:108 `n_prio = prio_tiles + (prio_tiles & 1)`
Assumes the prio span is whole axis pieces (2 units each), so rounding to even is harmless. Existing mismatch for odd `prio_tiles`.
Bendy: a half piece is 1 unit (UnitsOf, R:80), so coverage in pieces differs from n_prio. The checker walks units and the pattern walks pieces (see S1).

A12. CONFIRMED. R:1317 `"ML merge is not on an axis piece"`; R:1331 axis from the merge piece
The merge tile itself must still be an axis piece with a straight join. The bend must stay upstream. Fine for the stated scope (bend in coverage, merge tile straight), but a bend adjacent to the merge tile also hits A4.

Deck (row under a bridge):
A13. CONFIRMED (no break found, noted for completeness). R:1327-1329 `if (!piece.on_bridge) p_tiles.insert(...)`; UsedTrackdirs skips on_bridge pieces; blocks key by tile.
A foreign deck over ML and row ground tiles does not appear as ground tracks. It is invisible to CheckPrio, which is correct. `TracksAt(layout, tile)` is ground-only.
Residual risk, SUSPECT: R:1189-1198 `BlockOfPiece` finds a bridge by a bounding-box test over every layout bridge, for on_bridge pieces only. If the ML's own on_bridge pieces sit in the coverage of a prio whose deck crosses another bridge's box, the first matching box wins. It is not specific to a foreign deck over ground.

## B. CheckPrioReach / G9b (R:1694-1788) and SettleSignals (R:1545-1688)

B1. CONFIRMED (correct, record the assumption). R:1566-1568, 1630-1650 `here == {Upper,Lower} || here == {Left,Right}`
Per-track occupancy is applied only when the tile holds exactly one parallel pair. This matches OpenTTD (TRACK_BIT_HORZ/VERT).
Bendy: an ML half + opp-half row tile is handled per track. Any third track on that tile (a join half, a stub) switches to whole-tile occupancy, so the ML train also reds the row signals, and the row's own signals can no longer be told apart. Faithful to the game, but it is the failure mode a bendy layout can hit.

B2. CONFIRMED. R:1777-1781 `const OccupiedPosition position{tile, TrackdirToTrack(piece->trackdir)}` dedup on that
One tile can carry several relevant tracks, and the position carries the track, which is correct for a half tile. G9b does not test a train on the row, which is fine.

B3. CONFIRMED. R:1785 `if (!std::ranges::any_of(unique, [&](const RuleViolation &u) { return SameTile(u.tile, v.tile); }))`
Comment says "one violation per tile and message", code dedups by tile only.
Bendy: where the ML half and an opp-half row or another prio's coverage share a tile, a second, different violation on that tile is dropped. The deferred search (S:508) matches on tile only, so it is unaffected. Low severity.

B4. SUSPECT. R:1738-1742 `has_prio`: any unused track on the merge tile
It does not check the unused track is this merge's join. Only matters if an opp-half row of another prio sits on the merge tile.

B5. SUSPECT. R:1763-1769 coverage walk: `units < limit`, then `+ lane.pieces[last-1]` when `last` is signalled
Walks pieces by units (half piece = 1, axis = 2). It matches CheckPrio's walk but not the generator's n_prio piece count (S1). A bend shifts the covered range in tiles. Consistent between checks, so no false result expected from the check alone.

B6. CONFIRMED (deck). R:1567-1572 SettleSignals `bridges` is keyed by head tiles only; ground tracks under a span are ordinary tiles; R:1790-1800 span-tile handling is for on_bridge ML pieces only.
A foreign deck over the ML and row ground tiles is modelled correctly, because the train on the deck is a wormhole between the heads and never touches the ground tiles. No assumption broken. Not tested: a row tile that is itself a bridge head. `explore` takes `bridges.find(tile)` before `tracks.find`, so a head with ground track would be read as a head only (CheckBridges R:642 flags "track on head" anyway).

B7. CONFIRMED. R:1625-1632 `Track track = Track::X; for (Track t : masked) { track = t; break; }` on a signal tile
When more than one masked track touches the entry edge (join tile: axis plus h1), the first is taken. The game uses the signalled track.
Bendy: matters only if a join tile or a half tile carries a signal and another track touching the same entry edge. Not specific to bendy but the opp-half row makes a shared signalled tile common. SUSPECT for the shared tile.

## C. Signal pattern, phase 0 (S:107-215)

C1. CONFIRMED. S:163-168 `fits` loop: `P[j-s].tile == ml(s)` and `piece.trackdir == mp.trackdir` for s = 1..n_prio
Straight line of n_prio axis pieces with the merge trackdir. Any bend inside n_prio pieces is "does not fit".
Behaviour: with `prio == nullptr` (S:177) the prio goes to `deferred`. With `prio != nullptr` (caller's spans, S:178-181) it is not deferred. It reports "the prio element at ... does not fit" and the prio is dropped. A bendy span from the caller therefore fails outright.

C2. CONFIRMED. S:169 `fits = tracks_at(tracks, row(s)) == TrackBits{}` and S:170-174 `row(n_prio+1)` touching the exit edge
The row must be a straight line of empty tiles, one tile beside every ML tile. An opp-half row (non-empty ML tile) can never pass. Also no `heads.contains(row(s))` check here (only in `candidate`, S:433), so a straight row over a foreign bridge head would be drawn. SUSPECT for that last point; CheckBridges R:642 would catch it afterwards.

C3. CONFIRMED. S:186-190 `for (const auto &[tile, bits] : span->tiles) { draw(tile, bits); ... tile.y != M.y ... {tile.x, M.y} }`
`tile.y != M.y` pairs the row tile with `{tile.x, M.y}`, which assumes the ML runs along X at constant y. It is wrong for an ML along Y or one that bends.

C4. CONFIRMED. S:193-209 `draw(ml(s), h1)`, `add_signal(row(s), back, ...)`, `add_signal(ml(s), mp.trackdir, ...)`
Joins at even s, signals at odd s, all on straight `ml(s)`/`row(s)`. The signals face `mp.trackdir`/its reverse (the merge piece's axis). For a spans-fed prio (S:187) the drawn tiles come from the span but the signals are still placed at straight ml/row positions.

C5. SUSPECT. S:145 `if (offset == 0) continue;` and S:146-152 `side` from the joiner's last tile before the merge
The row side is derived once from the joiner's approach tile at the merge. A bendy row that crosses to the other side of the ML is not representable. Handedness is fixed by `turn` in S:363 (consistent, OK).

C6. CONFIRMED. S:95 and S:228 `TracksOverlap(tracks_at(tracks, piece.tile))` in `is_legal` / R:844
Upper+Lower (HORZ/VERT) is not "overlap", so the ML half + opp-half row tile stays legal for an ML signal. Fine. A third bit on the tile makes it illegal for Fill and Bay signals, and `last_ok` then skips that piece.

## D. Deferred search, phase 5 (S:351-557)

D1. CONFIRMED. S:387-396 `corner(m)`: needs `m >= 1`, `!IsDiagonalTrackdir(piece)`, `is_axis(m-1)` and `is_axis(m+1)`, and `ChangeDiagDir(in, turn) == out`
A bend is supported only as ONE half piece between two axis pieces turning TOWARD the row side, with the row on the inner corner tile.
Bendy:
- two or more consecutive half pieces: `is_axis(m±1)` fails, `row_of` is nullopt.
- a half piece turning away from the row side: `corner` returns nullopt. The needed row is the opposite half of the same ML tile, which `row_of` never returns.
- an S-bend always has one turn away and one toward. So no row is found across an S-bend, and the search aborts.

D2. CONFIRMED. S:539 `for (int m = pos.back() + 1; ...; m++) { ... row_free(m)` as the loop condition; `row_free` S:416-421 requires `tracks_at(tracks, *tile) == TrackBits{}`
The first m without a free row tile ends the whole candidate scan (it is the loop condition, not a skip). With `given == nullptr`, any half piece with `row_of == nullopt` ends the search at that depth. The opp-half row (a non-empty ML tile) can never be `row_free`.
Exception: with `given != nullptr` and `!is_axis(m)`, `row_free` returns true (S:417).

D3. CONFIRMED. S:415 `plain(m) = is_axis(m) && !corner(m-1) && !corner(m+1)`
Elements (joins and signals) are never adjacent to a bend. `candidate` (S:431-440) then rejects them. It also requires `tracks_at(tracks, piece.tile) == {its own track}` for m > 0 (S:434), so any ML tile also holding a row half cannot take an element. On a short S-bend leg (axis piece between two half pieces) no element fits, so at most the straight legs carry elements, and the CheckPrio rule "one join per block over the train length" (R:1437-1458) can become unsatisfiable.
For `given` rows, `plain` uses `corner()` only (toward-the-row-side bends), so a piece next to an outward bend counts as plain even though its row neighbour is an opp-half piece. SUSPECT that such a join is accepted by `try_leaf` and then fails A4 in CheckPrio.

D4. CONFIRMED. S:398-412 `row_of` for an axis piece: the tile at `side_of(piece)`, rejected when a neighbouring `corner` tile differs (S:408-411)
One row tile per ML piece, no opp-half. For given rows, S:400-405 matches `row_index` by the tile `piece.tile + side_of(piece)` and the same Track. A given row that passes beside an axis piece on a different tile (the row dipping into the ML's own half tile) returns nullopt and the leaf is lost.

D5. CONFIRMED. S:469-488 `try_leaf` draws: even elements `h1` on the ML tile plus `h2` or `{piece_axis, h2}` on `row_tile`; non-element axis pieces `plain(m)` get `{piece_axis}` on the row tile; non-axis pieces `drawn_here.emplace_back(row_tile, TrackBits{TrackdirToTrack(piece.trackdir)})` (corner tile)
For a half piece the row tile gets the same half track as the ML half. That is right only for the inner corner. It does not draw the opp-half for outward bends, and with `given` the half pieces are skipped (S:467 `continue`) and the row is drawn from the given row's own tracks.

D6. CONFIRMED. S:442-443 `coverage` by units over `at(++coverage)->units`; S:503-509 violation filter over `m = 0 .. max(coverage, pos.back())` using `at(m)->tile` and `row_of(m)`
The ownership filter attributes a violation to this prio only if its tile is an ML tile or `row_of(m)`. A violation on an opp-half row tile is the ML tile, so it is caught. A violation on an outward-bend row segment with `row_of == nullopt` and not on an ML tile is not attributed, and the leaf is accepted despite it. In practice the opp-half row cannot be proposed at all (D1/D2), so this is for the future opp-half row support.

D7. CONFIRMED. S:473-489 / signals_on S:422-427 / mark_entry S:308-321 match signals per (tile, track)
Per-track matching is correct for a shared ML half and opp-half tile. Fill removal by tile+facing is also per trackdir. No break found; this is the one place already written for several tracks per tile.

D8. SUSPECT. S:553-556 `if (plain(0) && row_free(0))` requires the merge piece's own row tile free and `plain(0)` (no corner at m = 1). A bend at the piece right before the merge tile aborts the whole search. Same as A4/A12.

## E. sigsim.py
src/junction/junction_rules.cpp:1542 names docs/junction-tool/tools/sigsim.py as the model for SettleSignals. Not read beyond its header, as asked. It has the same per-track branch if it is a port of ExploreSegment. Neither it nor SettleSignals models a foreign deck other than as a head-to-head wormhole (B6).

## Summary table of what a bendy prio breaks
| Area | Break |
|---|---|
| Pattern (S:163-168, S:169) | any bend, any non-empty row tile |
| Spans path (S:178-181, S:189) | no deferral, ML-along-X assumption |
| Deferred search (S:387-421, S:539) | row on opposite half; 2+ consecutive halves; outward turn; elements next to a bend |
| CheckPrio row (R:1361, R:1392) | opp-half row is not in `row`; signals on it are unchecked |
| CheckPrio join (R:1378-1383) | exact-track equality at the join tile and the row tile |
| CheckPrio signals (R:1488-1507) | one axis coordinate orders signals and joins |
| ComputeBlocks (R:1115) | tile-level node, signal tile dropped whole |
| G9b / SettleSignals | faithful per track; fails only when a third track shares the tile (B1) |
| Deck over ML and row | no break found (A13, B6) |
