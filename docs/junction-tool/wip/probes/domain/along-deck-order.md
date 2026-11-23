# domain-52: deck 1, deck 20, order L57 0 on V's 197 (diagnosis, no code change)

Run: HEAD 284286b181, `[.][planardomain197]` with PLANAR_DOMAIN_JUDGE_197. Refusals after D438/D439: deck 1, deck 20, order L57 0 (plus grid x2, node 47/61, leg zero L101: not this brief).

## Rows
- deck 1 and order L57 0 are one inequality on L57 (structure 1, piece 0): crossing 77 (L40xL57, parameter 13) and crossing 87 (L46xL57, parameter 12); event order wants 87 >= 77, V has 87 half a tile before 77 (sum -1).
- deck 20: structure 20 (separator L46), crossers L40 (crossing 74) and L57 (crossing 87), both at parameter 3; deck gap 1 needed (axis vs diagonal crosser), V has 0.

## V's tiles (our raster of V's points, dump of the same run)
- L57 runs down column x=95 as Y pieces (tunnel, structure 1).
- L40 reaches the column at (95,168) Left, runs Y on (95,167), leaves at (95,166) Right: along mode at crossing 77 (L40 over L57), crossing 87 is not along, 74 is L40xL46.
- L46 (tunnel, structure 20) runs X along row y=167 through (92..96,167): it passes under tile (95,167), the very tile where L40 runs along L57. So L46 crosses L57 at (95,167), inside L40's along stretch; and at L46 the crossers L40 and L57 are the same line (parameter 3 for both).
- V's geometry is legal (the domain accepts the tiles, no break there).

## Class: (c) other, stop
Not (a): the brief's fix (measure the along crossing to the nearer end of its stretch) cannot make these rows hold. Crossing 87 lies inside L40's stretch (same tile (95,167)), not beyond either end of it, so "gap to the nearer end" is still violated (L46 would have to be at least 1 half tile past the stretch's end in event order; V has it in the middle). Deck 20 is the mirror: both crossers of L46 coincide on the along stretch, so any gap between them is violated by construction.
The real question is a rule: another tunnel (L46) may pass under the tile where a surface line runs along a tunnel (L57), i.e. a crossing of the separator may fall inside an along crosser's stretch, and an along crosser and its separator cross a third structure at the same point. Both are a disjunction (inside the stretch, or kept apart by the gap) or a waiver of deck/order between an along crossing and a crossing within its stretch, and the stretch's extent is itself a drawn variable. That is a design choice beyond this brief.
Options for a ruling: (1) waive deck/order between an along crossing and any other crossing of the same separator (simplest; leaks nothing the domain forbids, since the crossings are on different levels); (2) waive only when the other crossing's tile is within the along stretch (needs the stretch's extent as rows); (3) for deck 20, treat an along crosser and its separator as one crosser of a third structure.
