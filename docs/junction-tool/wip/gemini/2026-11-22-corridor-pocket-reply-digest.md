# Gemini's reply on the corridor pocket: digest (orchestrator, 2026-11-22)

Reply pasted by Pete in session; this is the digest, not the text.

- **Q1 (pocket, linear):** yes when the order along h is known. Replace "keep from a's line" by two rows: a longitudinal row `(v - insert_end) . u_h >= margin` and clearance from b's line only. Sound: beyond the insert's far end along h, S is only b.
  - Orchestrator's reading. Apply it per (side S, adjacent side R): R's vertices at S's end keep from the lines of S's pieces after S's LAST insert, plus the longitudinal row past that insert's far end; the side before S, mirrored, uses S's FIRST insert. Non-adjacent sides keep the strip rule (they span the step).
  - Not weaker than today, only different: it forces the step to sit away from the adjacent side's footprint, which is what V does in the fan faces. Needs a probe.
  - Margin from Lemma 2, not Gemini's "spacing". The separating line is perpendicular to h through the insert's end (a track vertex, so an edge midpoint). For an axis h it is a tile-edge line, which separates with 0 on both sides. For a diagonal h it is a diagonal grid line, which needs RungSpacing on one side. To be written into §8.8.
- **Q2 (corridor subsystem):** Gemini advises against a separate corridor engine for these faces; the Q1 rows give the corridor behaviour in the face model. Agreed for the 9 fan faces. The corridor family is still needed for copies and prio rows (T1/T2 clearance), which is a separate matter.
- **Q3 (spacing across a dummy):** one-sided is sound (linearity plus Lemma 2), provided the dummy's line is on the grid. It is: a dummy starts at a track vertex on one of 8 headings, and an axis dummy perpendicular to its edge lies on a tile edge, which needs 0 on both sides. Which side gets the spacing is a cost choice; Gemini's rule 1 is moot, since the dummy's end vertices are exempt anyway.
- **Q4:** node legs near the node tile (known: "node legs not yet in" the drawing program; D400's index-2 result); max spacing at bend vertices (already so). Nothing new.
