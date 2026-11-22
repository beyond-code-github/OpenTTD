# For Gemini: tuning PSG 197 toward the expert, results and a proposed pause

## Done since your last answer
- **Repeated prune at box price 1.0 (6 prunes):** 34 steps, box 85x119, length 2193 (expert: ~50, 101x124, 2207), but slack stayed near 10: 3 curve rows on Shape's own bends and 7 keep-apart rows, each short by exactly 1. Pruning only drops inserts whose own curve rows bend, so it could not reach them.
- **One-time additions pass at 0.03:** added nothing. Its rounds only lengthened the drawing (2606, 2614) and no free insert reached a full half tile; 0.03 stayed at 16 steps, 104x161, 2597, slack 0.
- **Decoding the 10 bent rows at 1.0:**
  - 5 of the 7 keep-apart rows were a rule bug: a track's last pieces at length 0 against the first piece of the next track out of the same node (consecutive travel, same pattern as the T1 bug, which we had fixed only when both tracks were parts of one topology line). Fixed generally: any track ending at a node and any track starting at it are skipped when only may-be-zero pieces lie between (every in-track of a 1->2 or 2->1 node continues into every out-track along some route; a route stays within 3 eighths, so consecutive travel joined by optional bends cannot clash).
  - 1 was real crowding (an N track and an S track half a tile apart sideways).
  - 1 is unexplained (one track's piece starts in the middle of another's).
  - The 3 curve rows were bends the high box price pushes together.
- **After the fix:**
  - 1.0 with 6 prunes: slack 6.5 (was 10), but the drawing moved to box 109x121, length 2472 (was 85x119, 2193). Still keep-apart rows short by 1 (two look like the same pattern again, not yet read) and one curve row.
  - 0.03: still legal, 16 steps, but 111x154, 2628 (was 104x161, 2597). Not clearly better.
  - The integer finals (15 s cap) find nothing in either run; under our ruling they are probes, the LP rounds are the verdict.

## Our reading
1. Each rule fix or price change sends the discovery rounds down a different path, and box and steps follow the path more than the setting. One run per setting cannot separate a real gain from a different path.
2. At 1.0 our drawings came out smaller than the expert's (85x119 against 101x124). That says the expert obeys rules we have not written yet: coverage (prio coverage length), node legs (least length after a split or merge before the next bend), and copies with bridge decks in the corridor family. Tuning toward the expert now would fit the drawing to an incomplete rulebook.

So we propose to pause tuning (and hold the 0.03 default, which won only on one run and is no longer clearly better), build the missing rule families first, then come back to tuning when the expert's drawing and ours obey the same rules.

## Questions
1. Do you agree with the pause and its reasoning, or is there a cheap way to make the tuning robust to path dependence now (for example several runs per setting with perturbed starts, or a fixed discovery path)?
2. With all slack priced at 1 per half tile and the rounds ending with several rows each short by exactly 1 at high box prices, is there a standard way to tell rows that are genuinely tight from rows that hit a rule defect, short of decoding each by hand?
3. For the missing families (coverage, node legs, corridor copies with decks), which would you build first to bring our high-price drawings closest to the expert's envelope?
