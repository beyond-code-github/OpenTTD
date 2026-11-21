# For Gemini: curve spacing on optional bends

## Setup (recap)
- We draw an OpenTTD rail hub as an LP/MIP on a half-tile grid (HiGHS). Topology and shape come first: per track a heading list in 8 directions (each bend is 45 degrees). The program then decides only lengths: an axis piece is 2k half tiles, a diagonal piece moves (+-1, +-1) per unit.
- **Curve spacing (a game rule for train speed):** along every route (a path of tracks), consecutive bends must be at least a distance apart, measured as travel along the route:
  - same hand: >= g (5-7 half tiles, depending on train length);
  - opposite hands: >= 2;
  - bends 2 and 3 apart: >= w2 and >= w3 (windows).
  For the shape's own bends this is linear: a bend's position is a sum of length variables. On the expert's hub (PSG 197) all 430 such rows hold on the expert's real drawing.
- **Optional bends (flex points):** on a free stretch, a straight piece of heading h may take a sideways step: h -> h+1 -> h, and/or h -> h-1 -> h. Each insert is an axis piece of length >= 0, where 0 means no step and its two bends do not exist. On 197 there are 654 inserts; the LP drawing uses about 170 of them.
- **Keep-apart** (tracks not overlapping) is pairwise separation per face. Each pair's direction is read from an earlier drawing (bounded LP rounds), so the program has no binaries. This works: the whole hub draws at LP level in seconds.

## The problem
- A used insert's bends must obey curve spacing against each other and against the neighbouring bends. That rule is conditional: it applies only if the insert's length is > 0.
- What we know:
  - Unconditional rows force every insert, used or not, to leave room, which over-constrains badly.
  - Binaries per insert mean 654 big-M rows. The integer solve is already too slow without them, and in the LP relaxation the rows are nearly vacuous.
  - Today nothing enforces it, so a drawing can use an insert too close to a bend.

## Candidates we see
1. Decide insert use before the solve, from the previous LP drawing, as we do for keep-apart directions: unused inserts are fixed at 0, used ones get unconditional curve rows. Linear and exact once decided; the risk is a poor choice locked in.
2. Binaries per insert with indicator rows, in the integer step only.
3. Restrict where inserts may sit so the rule holds by construction. For example, an insert only where the flanking parts are long enough, written as part >= g * (insert used)... which is the same conditional again.
4. A reformulation we are missing: e.g. curve spacing as a minimum distance between consecutive bend points along the track, with a bend's position a variable that coincides with its neighbour when the insert is 0.

## Questions
1. Is there a known linear (binary-free) formulation for minimum spacing between optional bends on an octilinear path, from metro-map layout (Noellenburg-Wolff's MIP and its successors), orthogonal compaction with optional bends, or VLSI wire jogs?
2. If not, which of 1-4 would you choose? Is candidate 1's "decide from the previous drawing" sound in a bounded number of rounds, i.e. does re-reading insert use from a drawing that obeys the curve rows of the previous choice converge or cycle?
3. Candidate 4: can a zero-length insert's two bends be made "the same point as the bend before" so that the spacing row degenerates harmlessly? The spacing row between bend i and an insert's first bend would then read p_ins - p_i >= g even when the insert is 0, unless the insert's first bend is placed at the next real bend. Is there a known trick?
