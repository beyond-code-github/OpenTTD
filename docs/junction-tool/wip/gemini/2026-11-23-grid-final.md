# For Gemini: the integer final was off the game grid; with the grid it finds nothing

## Correction to your last answer
You read the 13 s root cut loop as HiGHS rediscovering a lattice already implied by the model. It was not: the drawing program had no tile-grid rows at all. Counting the 17 s final's drawing (1858) directly: N/S piece ends at x even 180, odd 50; E/W piece ends at y even 102, odd 178. On the game grid each axis has one parity. So the 1858 drawing is not buildable, and the 17 s was for a relaxation.

## What we measured (PSG 197, same pipeline: LP discovery and commit rounds, then one integer final on the sides the last LP round proved)
- Grid rows: every point of an axis run of positive length is 2g + b across the run (N/S runs on x, E/W on y), g a new integer, b one free binary per axis (the frame's parity). A run is the pieces of one heading joined through inserts held at 0; positive when a piece has a least length or the run holds a whole flex group with every insert held at 0. 750 rows on 197.
- Without grid rows: root LP 1804.25 at 1.8 s, cut loop to 1857.5 at 14.8 s, rounding finds 1858 at 16.8 s, 1 node.
- With grid rows (b free): root LP 1804.25 at 1.7 s (unchanged, as expected: g is continuous in the LP). Cut loop: 1903 at 7.4 s, 1969 at 12.5 s, restart, 1996 at 36 s, 1999 at 60 s. No incumbent at 60 s, 3 nodes.
- With b fixed to 1: same picture, bound 1996 at 60 s, nothing found.
- With grid rows on single pieces of positive least length only (4 rows; under flex C almost every axis piece may be 0): no effect, 17.5 s.
- Our solve budget: a drawing not found in about 15 s is treated as infeasible in practice. We do not run solves past 60 s.

## The lattice, for reference
- Points are integer half tiles. An axis piece's length is 2k. A diagonal moves (+-1, +-1) per unit.
- On the game grid every point has x + y odd; N/S runs sit on one x parity, E/W runs on one y parity; each diagonal unit flips which of x or y is odd. Every bend is 45 degrees.
- What the LP rounds decide, before the final: the keep-apart side of every wall pair (one fixed row per pair, `sep`), which flex inserts are committed (>= 1 half tile, a real bend pair) and which are held at 0. The LP rounds are continuous and know nothing of the grid.

## Questions
1. The grid final's bound is at least 1999 against the LP's 1804 and the off-grid 1858. Is the likely cause that the LP rounds fixed sides and held-at-0 inserts that suit off-grid geometry, so the grid final is near-infeasible on those choices? How would you test that cheaply, within a 15 s budget per solve?
2. How do you get grid awareness into a continuous LP pipeline? Candidates:
   - write the LP rounds in tile-lattice coordinates (per run its centre-line coordinate in whole tiles, diagonals as differences), then round the LP's run positions to the lattice before the final;
   - an integer final that may still re-pick sides near the LP's choice (a few binaries), or release held inserts;
   - fix the parity of each diagonal (odd on a turn, even on an S-bend, known from the headings) and of each run's centre line before the LP rounds, so the LP works on 2h + p from the start.
3. Which would you try first to get a grid-legal drawing of 197 in about 5 s, and what would you expect it to cost in length against 1858?
