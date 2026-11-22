# For Gemini: sibling lanes built; the crowding sits at bundle ends

## Done since your last answer
- **Sibling lanes as offset curves** (behind an option, off by default):
  - Which pairs: every pair of lanes in one channel that cross the same channels in order. The track map's side, which must be one non-zero value, keeps only adjacent lanes.
  - The rows run between the pair's first and last channel crossing, with no other events inside. At every vertex they hold `normal(h) · (v_b - v_a) = 2 s` for both incident headings: the corridor row we already use for copies and prio rows, with exact one-lane offsets.
  - Keep-apart wall pairs inside that range are skipped.
  - About your miter numbers: in our units this corridor row is the one the expert's PSG 197 already satisfies, so we used it rather than your worked offsets (diagonal spacing differs).
- **Result: no gain.**

  | Hub | Pairs used | Before | After |
  |---|---|---|---|
  | T1 | 0 | 0 | 0 |
  | T2 | 2 | 0 | 0 |
  | T3 | 4 | 13 | 13 (keep-apart 8, curve 5) |
  | LL2 | 1 | 10 | 12 |

  Most pairs were skipped because they cross only one channel.
- **Where the conflicts are now:** outside that range, where a bundle opens or closes.
  - T3: D3's last pieces (13-17 of 18), heading for its join with E3, against D2. E3 is in the conflict too.
  - LL2: B1's pieces 2-4, near the fray at the bundle's start, against B2.
- **Extending the range outward** while the two lanes' pieces match (headings and flex inserts, no events), and allowing a single channel crossing, made even a plain T3 feasibility test (no keep-apart) infeasible. The conflicting rows are:
  - the new D1/D2 offset rows (`lane D1 D2 12..16`) and one `lane D2 D3` row;
  - nodes 17, 23, 26 and 27;
  - E3's prio row (its opening and closing steps) and E3's copy (opening and closing);
  - the pieces of D1, D2, D3 and E3, and one crossing.

  Reading: near the joins, E3's prio row and E3's copy lie between D1 and D2. So "one lane apart" is false there: the true offset is one lane per track in between, and it changes at the nodes where those tracks open and close.

## What the plan of record says
- Our v2 design defines a **corridor** as "a face inside a bundle (between two lanes, a lane and its row, a separator and its copy, or inside a crossing lattice). Rigid by rule: its width is exact."
- The drawing program makes rigid only (i) a beside track (copy or prio row) against its parent chain, with exact one-lane opening and closing steps at its nodes, and (ii) now sibling lanes between crossings.
- Every other face inside a bundle still gets pairwise keep-apart with sides read from earlier drawings, and that is where the slack is.
- Node shapes are rules, not a catalogue of fixed gadgets: a straight leg at the trunk heading, the other leg one eighth off, the node tile shared, clear from piece 2.

## Options on our card
1. **Corridor faces rigid, built generally (our recommendation):**
   - Per stretch, the tracks of a bundle (lanes, rows, copies) have a lateral order read from the track map's faces, and each adjacent pair is offset by exactly one lane.
   - A track that enters or leaves the order at a node does so by an exact one-lane opening or closing step.
   - Pairwise keep-apart with drawing-read sides remains only between bundles.
2. **A node-zone rule only:** between a bundle's last crossing and its node, lanes peel off in lane order, each at least one lane from the next.
3. **Park T3/LL2:** T1, T2 and 197 are legal. Move to the roadmap items (corridor family on by default, coverage, node legs) and come back when Slots is built on the same rows.

## Questions
1. Do you agree the evidence points to corridor faces (inside bundles) rather than to side picking, the commit or the pieces themselves?
2. **For option 1, identifying corridor faces from the planar map:** our map has faces as runs of walls (track stretches), a side per stretch pair, and each track's kind (line, copy, prio row) and channel. What is a sound, general test that a face is a corridor? For example: its two long walls are adjacent tracks of one bundle, and its short walls are nodes or crossings. Any trap with faces that hold a crossing lattice, or a crossing of another channel through the bundle?
3. **The lateral order changing at nodes:** at a staggered join, D3 peels off to join E3 while E3's prio row and copy open beside E3 and slide in between the D lanes. Is "every adjacent pair exactly one lane, with each opening or closing an exact one-lane step at its node" consistent with how coop builds staggered joins? Or do real joins leave wider gaps (bays, signal room) that make "exact" wrong there?
4. **Ordering the opening and closing steps:** where several tracks enter or leave at nodes close together, what keeps their steps from colliding? Is a minimum travel between consecutive steps along the bundle enough (our curve spacing rows already apply to the steps as bends)?
5. **If you would pick option 2 or 3 instead, why?**
