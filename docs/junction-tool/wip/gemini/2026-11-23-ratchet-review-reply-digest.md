# Digest: Gemini on the measured flex ratchet (2026-11-23, orchestrator)

Prompt: 2026-11-23-ratchet-review.md.

## Taken
- **IRL1 for discovery** (Candes-Wakin-Boyd 2008): from round 2, price each insert at C / (previous length + eps). This is the binary-free stand-in for a fixed charge per step: dust collapses, real steps grow. Built as `flex_weights`.
- **Box pressure in discovery:** width and height had no cost in the program (`planar_draw.cpp` width/height variables). Built as `box_price`.
- **One-time additions pass** (second discovery with the committed set's curve rows on and the other inserts free, then commit the union, then one prune): monotone, so no cycling. Not built yet.

## Already in place
- Its soundness checks 1 and 2: the commit rounds re-pick directions each round, and pruned inserts are held at 0 (upper bound).

## Not taken
- Check 3 (at most one flex direction per piece): curve rows already keep two pairs on one piece legal; it is a length cost, not soundness.

## Result
- IRL1 0.1, box 0.01, commit >= 1: 10 steps, curve-legal at slack 0, **integer final Optimal in 17 s** (box 121x188, length 2956). See CARD-E5-flexstep.
