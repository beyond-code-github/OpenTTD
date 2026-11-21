# For Gemini: answer-key update (the expert's hub as ground truth)

## Progress since your last reply
- To pin the expert's event points, we needed the save's own extraction of the hub to load in our C++ model. That surfaced extractor bugs, now fixed:
  - **Lane directions:** came from one majority signal vote per connected network; now from each lane's first signal inward.
  - **Rings of corner pieces joined through 90-degree turns:** these were the expert's small signalled prio / pathfinder-trap rectangles beside lanes, which we now ignore. With pieces joined only through moves a train can make (`forbid_90_deg`), the extraction matches our hand-authored model on all 122 crossings.
  - **Straight legs next to one-tile node pieces:** now found.
  - **Lanes with no track at all** (a split on the box-edge tile itself): now get a heading.
- **One representation mismatch left (being fixed):** the extractor put a switch's diverging 45-degree piece inside the node tile, so the diverging leg's first piece came out at the trunk heading. Our model treats a node as a point and wants the turn as the leg's first piece. The fix adds the switch-tile piece to each leg's raw headings.

## First over-strict spot found
- The expert has a split right on the box-edge tile, so the in-port lane between the port and the split has length 0. Our model demands every axis piece is at least one whole tile, so a port lane must run at least a tile before its first node.

## Questions
1. In OpenTTD, a switch tile holds the straight bit and the diverging diagonal bit of one tile. When our model makes a node a point with each leg's first piece starting there, is it right to count the switch tile as the first half tile of the diverging leg? Or should the node be a whole-tile object in the drawing (both legs share the tile, and clearance between the legs starts at the next tile edge)? Which representation makes "node at the box edge" (zero-length port lane) legal without allowing illegal tile sharing elsewhere?
2. For the event-pinned elastic run: pin crossings, nodes and port ends to the save within +-2 half tiles. Should node points be pinned to the switch tile's centre, or to the edge midpoint where the diverging piece leaves the tile, given the model's node is a point at a piece boundary?
3. Anything else you would check in a hand-built coop hub before trusting it as ground truth for spacing rules (signal-gap filler track, sidings, depots inside the box)?
