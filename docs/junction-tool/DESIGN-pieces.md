# DESIGN-pieces: first-class diagonals, pieces by construction

Status: draft for the engineer's review (2026-11-17). It sets the direction for the mapped composer's geometry, under D326 (any hub), D327 (channels with width profiles), D330 (atomic primitives), D333 and D334 (doubled width, the grid in the geometry layer).

## 1. Why

What LL² taught us on 2026-11-17:
- **Speculative pieces.** The composer adds pieces that *might* be needed (fans, jogs, an optional first piece after a split, exit steps) and lets the solver switch them off. Each switch is a binary with a big-M bound.
- **Invented caps.** The bound was a raw 8-tile cap (`OPTMAX`) that no rule produces. LL² o49t0f0 was "impossible" because a detour needed more than 8 tiles, found by stripping the model down to its 42 conflicting constraints. The cap also quietly sped the search up: without it, T2 takes 15.4 s instead of 4.6 s and T3 20.9 s instead of 9.9 s, the same areas but more overlap rounds and weaker estimates.
- **Rigid blocks.** The doubled bridge carries its own fixed sideways steps (one tile out and back) outside the channel's lanes, which nothing reserves.
- **Second-class diagonals.** Straight (axis) pieces are the real pieces. Diagonals appear as connectors: fixed-length steps, switchable fans, detours the path step inserts, grid rules written for axis pieces only.

Spike (2026-11-17), fixing every switch of T2's and T3's best layouts to the value the solver chose:

| Variant | Switches free | Switches fixed | Area |
|---|---|---|---|
| T2 o0t2f1 | 2.5 s solving | 0.2 s | 917.25 both |
| T3 o0t1f1 | 5.6 s solving | 0.2 s | 1941.25 both |

Of T3's 13 switch groups:
- 7 are already fixed by the width profile.
- 5 are exit jogs whose on/off follows from the joins' widths (a prio join is wider than its port).
- 1 is a genuine choice: B's jog before its core, on in T3 and off in T2.

The solver's time goes on deciding which pieces exist, and nearly all of that is knowable in advance.

## 2. Pieces

- **A piece** is a straight stretch of track in one of 8 headings with a free length. Diagonals and axis pieces are the same kind of thing. The differences are rules on the piece:
  - **Diagonals:** no deck, no ramp, no climb (game rules).
  - **Every piece:** its own grid pattern (below).
- **A strand** (one track) is a sequence of pieces. Consecutive pieces differ by exactly 45 degrees, so every boundary is a bend, left or right, at a known place in the sequence.
- **Every piece is required.** Its minimum length comes from a rule: the curve gap, deck heads, prio coverage, or a block's coop constraint. No piece has length zero, so there are no vanishing pieces, no switches and no big-M bounds.
- **The grid (D334)** is owned by the geometry layer:
  - Axis track runs on tile-row or column middles, and axis lengths are whole tiles.
  - Every track point satisfies one invariant (half-tile x + y odd).
  - The diagonal between two axis pieces has a length parity fixed by their orientations: even between parallel axis pieces, odd between perpendicular ones.
  - The bend helpers emit these rules. Blocks never write half-tile numbers.

## 3. Heading sequences by construction

A channel's heading sequence (shared by its lanes, lanes offset across it) is built before the solve from three sources only:
1. **Route turns.** The channel's in and out port headings give its turn: F goes east to south, so east, south-east, south.
2. **Width changes (D327).** A sideways step exists exactly where the channel's width changes: a fray opening, a prio row's spread, a join closing up, and a doubled bridge's copy entering and leaving its reserved line (D333). The width profile is known from the topology, the line shares (D331) and the components.
3. **Crossing angles.** The path step gives each crossing a piece on each channel (over: an axis piece; non-parallel to the partner), built so that the sequence is correct by construction. When no sequence exists, the path step fails with the reason instead of emitting a contradictory model.

Anything else is a genuine choice and becomes explicit (section 5), never a switchable piece.

## 4. Curve rules as gap sums

The game penalises a train covering two or more 45-degree bends in the same direction (Train::GetCurveSpeedLimit). A left and a right alone are exempt. With the bend sequence fixed, every rule is linear:
- For any two same-direction bends within a train length of each other along the strand, the pieces between them add up to at least the required gap. That gap is the existing JunctionCurveGaps values: the consecutive gap and the k-bend gaps.
- Opposite-direction pairs alone need nothing.
- Rules run along whole routes, through nodes and through both copies of every doubled bridge, as now (D313).

No rule depends on whether a bend exists, because every bend exists.

## 5. Explicit choices

These stay discrete, each either a variant or decided by the path step:
- over or under at an open crossing,
- tie merges, crossing triangles,
- which piece a crossing sits on (path step),
- shape choices with two legitimate heading sequences, such as T3's B jog before its core.

LL²'s variant count is cut separately (rotational symmetry; later, over/under in-model).

## 6. Blocks under this model

- **Blocks are compositions of pieces with coop constraints only.**
- **Doubled bridge (D312, D314, D333):**
  - The copy is a track of its channel, on its reserved line.
  - The copy's steps are the width change into and out of that line.
  - Fixed are only equal bridges, the one-tile stagger and tight heads at both ends.
- **Fray and join:**
  - Their fans and close-ups are width changes.
  - The prio row's spread is part of the width profile.
- **Removed:**
  - `OPTMAX` and every step cap,
  - optional segments and their switches (`group_z`, the '+', '-' and 'j' opt groups),
  - the optional bend pair (D321 lever 1),
  - the fixed step shapes inside blocks,
  - the path step's inserted detours (their job moves into construction).

## 7. Plan (test-first, outside-in)

1. **Geometry layer.**
   - Pieces with grid rules for all 8 headings, bends with their parity rule, gap-sum curve rows.
   - Unit tests per piece and bend: legal in, legal out.
2. **Heading-sequence builder** for one channel from route turns, width changes and crossing pieces. Tests on T2's channels against the sequences the solver chose in the spike dumps.
3. **T2 mapped** through the new geometry: area at most 917.25, each variant under 1 s.
4. **T3 mapped:** at most 1941.25, with the B jog as an explicit choice.
5. **LL²:** the variants build, and the failing ones fail with a stated reason, not "impossible".

## 8. Answers (2026-11-17)

1. **B's jog: the solver decides.** "Surely a jog being useful or not comes out from the solver naturally now?" Measured on T3 o0t1f1 with every other switch fixed: jog fixed 0.27 s, jog left to the solver as an in-model switch 3.7 s (same result). A piece that may or may not exist is an either/or the solver can only settle by search, and it is the costly part. So the solver still decides, by solving both shapes and keeping the better (two solves, about 0.5 s). The heading-sequence builder lists such genuine choices itself; nobody picks them per hub. If the count of choices grows with hub size, revisit (an in-model switch bounded by a rule, not a cap).
2. **Diagonal lanes:** "Nothing wrong with diagonals running close together." Today's spacing rule stays.
