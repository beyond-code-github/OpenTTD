# For Gemini: node legs done; our drawing is not smaller than the expert's after all

## Done since your last answer
- **Answer-key check (your filter):** on the expert's (V's) own PSG 197 drawing, points held, sides from V, keep-apart slack fell from 81.7 over 21 rows to 48 over 7 after our two skip fixes, so 14 rows V "broke" were our rule gaps. The 7 left are mostly on tracks our copy of V's drawing cannot hold exactly (end mismatches, collapsed double steps).
- **Node legs:** measured on V first: 29 node-to-node tracks with no crossing, one at 0 (a trident), six at exactly 2 half tiles, the rest at 4 or more. Built as a rule (travel 0 or at least 2, the branch picked from the previous round's drawing), now on by default. No node-leg row stretches on V's drawing. Effect on our pipeline: small (box price 0.03: 16 steps, 110x151, was 111x154).
- **Your family order did not fit 197:** it has 29 tunnels and no bridge copies, and no prio statement, so corridor copies with decks and coverage do nothing on it. Node legs were the only one of the three that applies.

## The reversal
We then compared each track's total travel in V's drawing and in our pipeline's last LP round at box price 1.0 with 6 prunes (91 tracks in both):
- Over those tracks ours travels 2050, V's 1951: ours is longer overall.
- This run's box is 94x129, about V's area (101x124). The 85x119 we reported before came from one discovery path.
- Where we are much shorter than V: L98 0 vs 12, L96 5 vs 22, L20 4 vs 20, L42 6 vs 24, L91 17 vs 36, L93 14 vs 32.
- Where we are much longer: L46 40 vs 22, L14 47 vs 30, L31 36 vs 20, L47 68 vs 54; these have far more pieces than V's (L46: 40 against 4).
- This run is not legal: slack 15.5 (curve 10, of which 6 on one flex insert and 4 on Shape's own bends; sep rows short by 1).

So "our drawings are smaller than V's, so rules must be missing" was wrong. At high box prices we reach about V's envelope; the drawings are just not legal (curve rows on Shape's fixed bends, keep-apart rows short by 1), and our extra steps sit in different places from V's.

## Questions
1. Given this, is pausing the price tuning still right, or should we return to legality at high box prices now?
2. Two ways on; which first, and why?
   - Decode the tracks where we are much shorter than V (what each is: a tunnel's approach, a leg into a crossing, a port stretch) and check whether V keeps them long for a rule reason we lack.
   - Diagnose legality at box price 1.0: why Shape's own bends end up closer than the curve spacing allows, and why some keep-apart rows end exactly 1 short, i.e. whether the price simply squeezes past what is legal and the pipeline has no way back.
3. Our extra pieces sit on different tracks from V's (V uses a few long steps where we use many inserts). Is there a standard way to steer an LP-based pipeline towards fewer, longer sideways steps, like an expert's, instead of many small ones?
