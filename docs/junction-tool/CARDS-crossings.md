# Cards for the engineer: crossings D223 does not cover (CX1 step 0, 2026-11-13)
**CLOSED by D227 (2026-11-13): C1 (b), C2 (a), C3 (a) are the defaults, kept as future toggles.**

Source: CX1's dump of LL_RR x LL_RR (scratchpad/dump.txt). Today's rule with exactly one branch joiner in a crossing: the joiner is carried over on a single bridge. D223: main x main: the straight-through lane over, the turn under ("for a compact footprint"); joiner x joiner: the one farther from the main line over.

## C1. Turning x turning (pairing any: 3 crossings)
Context: two main-to-branch exit legs cross, e.g. W->S leg x E->S leg near the branch port; neither is straight through.
Options: (a) the one whose turn comes later in its travel (crossing nearer its own exit split) goes over; (b) the one from the far side of the main line (crossing more lanes) goes over; (c) the one with the longer remaining run to the port goes under (it has room to dive).
Recommendation: (b): it mirrors D223's joiner rule (farther from the main line goes over), so one rule covers both exit and join sides.
Reversible: yes.

## C2. A shared edge carrying both a through and a turning path (pairing keep: 2 crossings)
Context: before the exit split, one edge carries both the W->E through traffic and the W->S turn; it crosses a turning leg.
Options: (a) the edge counts as through if any of its paths is through (it goes over); (b) counts as turning.
Recommendation: (a): through traffic never dips (D190/D223 spirit).
Reversible: yes.

## C3. Joiner x joiner: what "farther from the main line" means (any: 3, keep: 1)
Context: joiner edges run between band levels (e.g. L8 -> L4 and L8 -> L8); an edge has no single distance from the main line.
Options: (a) the end nearer the main line (the higher band/row end) decides: the edge whose near end is farther goes over; (b) the far end decides; (c) the order of the two edges at the crossing within the band.
Recommendation: (a): it is where the joiner meets the main line, which is what "farther from the main line" means for a joiner.
Reversible: yes.
