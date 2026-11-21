# For Gemini: review of the flex ratchet, measured

## Setup (recap from your last answer)
- We draw an OpenTTD rail hub as an LP on a half-tile grid (HiGHS). The shape layer fixes headings and where an optional sideways step (a flex insert, an axis piece of length >= 0) may go. The lengths layer decides every length.
- Curve spacing on an insert's bends is conditional (only if the insert is used). You confirmed there is no binary-free formulation. You recommended candidate 1 with a one-way ratchet:
  - discover insert use in a free LP;
  - commit the used inserts (length >= 1 lane, full curve rows) and hold the rest at 0;
  - at most one prune pass of the committed inserts whose curve rows still stretch.
- Keep-apart is pairwise separation with each pair's direction read from the previous drawing, re-picked over bounded LP rounds (slack only on separation rows) until slack is 0.

## Measured on the expert's hub (PSG 197; the expert's own drawing: box 101x124 half tiles, total track length 2207, about 50 steps)
| discovery | inserts committed | after commit | after prune | box | length |
|---|---|---|---|---|---|
| unpriced (inserts free) | 173 (>= 0.25 half tiles) | curve slack 34 on 7 inserts | 166 left, slack 0 | 239x275 | 5272 |
| insert length priced 0.5 per half tile | 5 (>= 1 lane) | slack 0, curve 0 | none needed | 114x181 | 2730 |
| priced 0.1 | 5 | slack 0, curve 0 | none needed | 109x175 | 2683 |

- Without curve rows on inserts, the free LP drawing is 82x93, length 1951. It sprinkles small jogs (173 used, most under a lane).
- All runs take 2-3 LP rounds of about 1.5-3.5 s each, with no cycling.
- The integer (whole-tile) final finds no incumbent in 60 s in every case. We have moved that to a later layer.

## Questions
1. **Steps versus length.** Unpriced discovery uses many jogs and becomes huge once they are made real; priced discovery uses very few steps and grows tall. The expert uses about 50. Is there a principled discovery objective that lands near the expert, rather than tuning one price? For example:
   - price per insert (fixed charge, which needs a binary) approximated by a concave or log penalty;
   - a lexicographic objective (separation slack, then box, then insert count via an L1 proxy);
   - pricing at the curve-spacing cost each insert would incur once real (2 bends need about g of room each).
2. **The ratchet's blind spot.** Commit can only keep or drop inserts the discovery used; it cannot add one the legal drawing needs. Is there a cheap, bounded way to add candidates? For example, a second discovery round with the committed set's curve rows in place and the other inserts free again, keeping only additions. Does that keep the no-cycling guarantee if additions are allowed once?
3. **The taller box (175 vs 124).** With few steps the drawing grows in one direction. Does that suggest the box objective (width + height) should be weighted against track length differently in discovery, or is it the missing families (the expert's hub has bays, coverage and corridors that we have only partly)?
4. **Anything unsound** in this pipeline (directions rounds, then flex discover, commit and prune) that you would test before we adopt it as the rule?
