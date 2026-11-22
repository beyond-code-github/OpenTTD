# For Gemini: certificates are in. Is our real gap turn-regularity (kitty corners)?

## What the exact certificates showed (all exact: every keep-apart pair in the listed faces binary, flex inserts free, coordinate bound 256 so big-M is 1024; other faces' keep-apart left out, so Infeasible is a proof)
- **T3:** its crowded faces (75, 29, 66), singly and together, are Optimal in 0.3-3 s. A drawing exists; our pipeline's round-by-round side picks (sides read from earlier drawings, D418) miss it.
- **LL2:** face 33 is Infeasible (0.8 s; the LP relaxation is feasible, so there is no IIS, and the integer side choice is what fails).
  - Deletion filters over pairs and rows give a three-row core. Keep-apart D1#11 (east) against B1#1 (south-west), keep-apart D1#11 against B1#4 (west), and the closing row of F1's copy: a one-piece doubled track heading south, one lane beside F1, rejoining F1 by the forced diagonal step (-2, 2) at node 25. Curves and bays play no part.
  - In words: D1 bends north-east to east and B1 bends south-west to west, their corners facing each other half a tile apart, and F1's copy rejoins F1 inside that same face.
- **Prio rows are not LL2's cause:** without the prio statement its headings are identical (52/52) and the slack is unchanged.
- Facing wall pairs (the face walk monotone both ways, so each wall is in the other's inner half-plane) take their side from headings alone and always hold. Every pair that stays stretched is a corner pair at bends.

## The engineer's challenge and our answer
- I proposed an exact "repair round" in the pipeline for T3 (solve the faces holding slack with binaries). The engineer asked: why repair, shouldn't it be valid by construction? He is right: our layer stack forbids generate-and-check; Slots must write every keep-apart row from the track map and Shape's headings before the solve (D381).
- Our current reading, from memory and so needing your check:
  - In orthogonal topology-shape-metrics, two reflex corners facing each other across a face are **kitty corners**. A shape with none is **turn-regular**, and then every separation follows from the shape, so compaction needs no choices (Bridgeman, Di Battista, Didimo, Liotta, Tamassia and Vismara, around 2000). With kitty corners, compaction is NP-hard in general (Patrignani, around 2001).
  - The cure is for the shape step to resolve each kitty-corner pair (decide which corner goes first, e.g. by a dummy edge), giving a turn-regular shape.
- **Mapped to our history:**
  - D385 refined faces into convex cells with dummy edges: correct in spirit but stronger than needed (rectangular-style refinement), and it over-constrained the expert's PSG 197.
  - Pairwise keep-apart with drawing-read sides removed the over-strictness but left kitty corners decided heuristically.
  - T3 is kitty corners resolved badly where a resolution exists. LL2's face 33 is a kitty corner with a copy closing beside it, where no resolution exists.
- **Proposed design:**
  - Shape finds the kitty-corner pairs in each face from headings and resolves each, by rule or as a variant bit.
  - Its promise gains a clause: every face is turn-regular after resolution, with room for each resolution.
  - Slots then writes every keep-apart side from headings plus Shape's resolutions: no sides from drawings, no repair.
- **Our octilinear twist:** headings in 8 directions, 45-degree bends, diagonal walls, half-tile units. We found earlier that with diagonal walls a dummy edge's landing depends on lengths: a ray from a reflex corner can hit different walls depending on lengths. So the orthogonal theory may not carry over cleanly.

## Questions
1. Are the references right (turn-regularity, kitty corners, the polynomial compaction result for turn-regular orthogonal representations, NP-hardness of general orthogonal compaction)? Please say which you are sure of and which you are not.
2. Does turn-regularity, or an analogue, extend to octilinear (8-direction) representations? Is there literature on octilinear or "slanted" compaction with fixed angles? If it does not extend, what is the right generalisation of "kitty corner" for 45-degree corners and diagonal walls (e.g. any two reflex corners whose rotation between them along the face is exactly 180 degrees, counting 45-degree turns)?
3. In our octilinear setting, once every kitty-corner pair is resolved (one corner declared ahead of the other along a chosen direction), is every other wall pair's separation side then determined by the face walk alone? Or can non-kitty pairs still be ambiguous with diagonal walls?
4. How should Shape resolve a kitty corner: a dummy edge from one reflex corner to the other (a fixed-angle segment, length free), an ordering constraint between the two corners along one axis, or something else? Which keeps the lengths problem an LP with no binaries?
5. LL2's face 33: does "a copy rejoining its parent inside a face that holds a kitty corner" suggest a general rule for what Shape (or Structure, which places the copy's end) must not produce, or is it a one-off?
6. Anything in this plan that worries you, given that the expert's PSG 197 drew legally with pairwise sides from drawings (it may simply have few kitty corners)?
