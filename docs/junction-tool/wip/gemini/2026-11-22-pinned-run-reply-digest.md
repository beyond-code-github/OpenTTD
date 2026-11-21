# Gemini on the pinned run: digest (orchestrator, 2026-11-22)

- **Q1, parity at crossings:** an axis x axis crossing sits at a tile centre (odd, odd), so each axis piece's distance from its end to the crossing is odd (2k + 1). An axis x diagonal crossing sits on a tile-edge midpoint, so the axis distance is even and the diagonal distance any integer.
  - Plausible geometry, but its diagnosis ("you constrained the split pieces to 2k") is a guess: the drawing program has no parity rows yet (D382), and a crossing parameter is a free integer. So the integer infeasibility must be found in the model (the L95/L11/L12/L40 pieces), checked against this table.
- **Q2, cheaper decisive test:** fix the pairwise binaries from the expert's geometry (each wall pair's separating side is known from the save), giving a pure LP. Same as the orchestrator's idea. Do this first: zero slack means the spacing rules match, and any slack names the exact wall pair.
- **Q3, dummies:** no linear formula can pick a landing wall (a disjunction). One binary per dummy over its candidate walls is sound, and only for the dummies in faces that stretch (about 10-20), within K-E5.
