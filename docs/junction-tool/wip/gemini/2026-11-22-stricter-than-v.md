# For Gemini: our keep-apart rules are stricter than a real expert build (follow-up)

## Since last time
- The elastic LP diagnosis works: about 50 ms per face, and it names the rows that stretch.
- It found a real bug: a zero-length flex step still imposed its heading's spacing on its corners. Fixed linearly: the step may have length only if its corner has room; on integer coordinates `len <= M*(d - base)` is sound.
- The pocket rule (anchoring an adjacent side past a step) did not help, and as a blanket rule it broke 34 faces. It stays off.
- Port lanes now carry their real positions (the expert's hub has uneven lane spacing at the box edge).

## The finding
- With the expert's real port positions, the whole hub is still infeasible. The elastic LP stretches mainly the port gap rows: the lanes on one side want about 22 tiles more than the expert used.
- **Decisive check:** the same happens when we draw the expert's own headings (his shape, plus our flex steps and cells), not our generated ones. So the drawing program's rules are stricter than what the expert actually built. It is not our heading choices.
- When we make port slack expensive (weight 1000), the stretch spreads thinly over about 159 rows: clearance rows per cell, crossing rows, dummy landing rows and node rows. There is no single culprit, so the LP cannot tell us which rule is over-strict; it stretches whatever is cheapest.

## Our rules (half-tile units)
- **Clearance:** a vertex keeps 2 from a diagonal line, or from an axis line parallel to its own piece, and 1 from an axis line when its own piece is diagonal or perpendicular. Measured along the line's normal (unnormalised: (1,0)/(0,1) for axis, (1,1)-type for diagonal). Applied per convex cell, by sides.
- **Crossing:** a crossing point sits at least `cross` from either end of each of the two pieces it lies on, and in event order along a piece.
- **Nodes:** the legs of a split share the node point. Clearance between the legs starts only after the node piece (a proof shows the tile rule allows the shared tile and stops clashing from piece index 2).
- **Pieces:** an axis piece is 2k half tiles with k >= 1 (a whole tile); a diagonal piece is >= 1. Flex pieces may be 0.
- **Spans:** a tunnel or bridge's separator runs straight across its crossings, with end runs at least `rules.end` before the first and after the last crosser.

## Our plan, and the question
- **Plan:** use the expert's actual drawing as an answer key. Pin the model's event points (crossings, nodes, port ends) to the save's coordinates, within a window of +-2 half tiles because tile-to-point mapping is approximate. Leave piece lengths free and run the elastic LP. Because events are pinned, conflicts can't drain into the ports. The stretched rows should then sit at the places where our rules are stricter than the expert's build, by family.

1. Is pinning events (with a small window) a sound way to localise over-strict rules? Or would the window itself hide or create spurious stretches? Would pinning full vertex coordinates (all bends) be clearly better, despite the harder mapping from the save's raw pieces to our pieces with flex inserts?
2. From your knowledge of OpenTTD track geometry: which of the rules above are likely stricter than the real tile rule, especially near tunnels, crossings at 45 degrees, and lanes one tile apart? Two candidates we suspect:
   - an axis piece needing a whole tile when it carries a crossing right after a bend;
   - our 2 half-tile spacing for parallel diagonals.
3. Any trap in treating the expert's build as ground truth? It is a real, working hub, but built by hand. Could it contain constructions our model deliberately forbids (for example, track sharing a tile in a legal but unusual way) that we should allow rather than flag?
