# DESIGN-tracks: the solve on tracks, structure from the topology

Status: draft for the engineer (2026-11-17). Builds on DESIGN-pieces (required pieces, gap sums) and D335 (gaps between channels).

## 1. Why
Three fixes on 2026-11-17 had one root:
- **Jogs:** the model had no gap between neighbouring channels; the channel graph knew it (D335).
- **Clash sides:** the model guesses which side two tracks keep, round by round (LL²: 7-10 rounds, 14-17 s a variant, a skewed layout locked in by early picks); planarity knows the left-to-right order everywhere.
- **Rules by position:** blocks emit fixed fragments and rules pick "the second piece of strand X" (the fan rule broke on 2c; the 2e merge steps probably likewise).
The model is tracks plus patched relations. The topology is a subdivision of the hub into regions between neighbouring lines; the model ignores it.

Engineer, 2026-11-17: "Should we just be keeping the channels, blocks etc concepts higher order and just letting the solve loose on tracks"; and earlier "I'm sure it's something to do with how we're modelling channels, either they're too rigid or they don't change size".

## 2. Layers
1. **Topology and channel graph** decide what is decidable and list the genuine choices:
   - each track's heading sequence (route turns, width changes, gap steps, crossing pieces; DESIGN-pieces §3);
   - the regions and their boundary lines (below);
   - the choices (over/under, ties, flips, candidate steps, region choices the graph leaves open) as variants.
2. **Track solve:** every track a sequence of required pieces (heading fixed, length free, grid rules); linear constraints only. It places; it does not choose shapes or sides.
3. **Blocks** (fray, join, prio row, doubled bridge, crossing stack) contribute only their coop constraints on named pieces (by role, never by index): spacing, coverage, equal bridges, the stagger, tight heads.

## 3. Regions
- A region is the space between two neighbouring lines along a stretch: its left line, its right line, where it opens (a split, a box edge between ports) and where it closes (a merge, a box edge, a crossing where it meets another region over a bridge).
- Non-overlap is "every region is at least the spacing wide" at every piece boundary of its two lines: linear, no sides to guess, no lazy rounds.
- A jog is where a region pinned narrow at the edge must hold a band inside (D335's gap rule falls out).
- A crossing is two regions meeting at a bridge: the under line passes from one region side to the other; over/under stays a variant.
- Widths are measured across the travel direction of the stretch; a stretch where the two lines turn differently is split at the turn.

## 3a. The width profile (2026-11-17, after the merge-step fit)
Engineer: "I can't shake the feeling that the mere concept of stepping merges is a smell. It should just figure it out without a special condition"; "doesn't that stop us building truly diagonal hubs? The diagonals are still a special case".
- Steps are not per-block slots with conditions (fan, gap step, merge step). Before the solve, the topology and the variant give, per stretch, the lanes side by side and their spacing across the stretch's travel; a lane gets a sideways excursion exactly where its position across travel must change between stretches. Fans, gap steps and merge steps are all cases of this one rule.
- Headings are first-class: a stretch has any of 8 headings; width is measured across its own travel; an excursion is a 45-degree turn away from the stretch heading and back (diagonal pieces on an axis stretch, axis pieces on a diagonal stretch). Diagonal hubs are blocked only by the box (axis ports on axis edges, D313), a separate constraint.
- No rule names a joiner, fan, merge or gap. Per channel and piece, the corridor holds an ordered list of tracks: the channel's lanes, each doubled lane's copy (two slots, D333), and the lines entering or leaving the corridor at a node in that piece (a merge's joiner before it, a split's leg after it) on the side the node's leg order gives. Each track gets a slot (lane spacing units) per piece, packed in order from the pinned side (port edge, bundle offsets). A track's slot changing between consecutive pieces is its excursion (sign = direction); nothing else creates one.
- Engineer, on "a lane steps when a joiner lands between it and its neighbour": "that's a special case rule ... Surely a joiner already has an effect on the channel?" Yes: the joiner is a track in the corridor before the merge, and the step falls out of the slots.
- Not decided by slots: a step the room does not force (LL² EO2/WO2/SO2: the bridge copy fits the port gap; the free solver steps for area, 55/56). Zero-length pieces are out (D335); such steps are candidate steps (variant bits, as B's) or absent.
- Evidence so far: the merge rule fitted to one LL² variant matched it exactly and lost area on 182 of 189 others (05e0159b09); wip/ANALYSIS-merge-steps.md tests whether the free solver's choices follow "the lane's position across travel had to change".

## 4. Plan (riskiest first)
1. **Region check on T2 (no solver change):** build the region graph from the channel graph alone; for the best T2 layouts, compare its implied separations with every clash side the lazy loop chose and the gap steps. Pass: every chosen side is implied, none contradicted. Failure cases name what regions miss.
2. Same on T3 and LL² (LL² o49t0f1s0: does the region graph imply the stack the probe found, or a flatter order?).
3. Region widths replace the lazy loop and the hand-written side rules on T2/T3 (`pv check`: no variant worse; times).
4. Blocks rewritten as constraint contributors on role-tagged pieces.
5. LL².

## 5. Open
- How regions meet at a doubled bridge (four tracks wide, D333).
- Whether one-lane merges' stacking order (LL²: D's merges) is fixed by the graph or a variant.
