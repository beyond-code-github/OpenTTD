# Digest: Gemini on curve spacing for optional bends (2026-11-23, orchestrator)

Prompt: 2026-11-23-optional-bend-curves.md.

## Taken
- **No binary-free formulation exists.** "0 when the insert is 0, >= g when it is > 0" is discontinuous at 0, and no LP enforces it. Prior art (Noellenburg-Wolff metro maps, VLSI jog insertion) uses a binary per bend or decides bend presence upstream. Matches SPEC §10.3's reason for leaving inserts out.
- **Candidate 1 with a one-way ratchet** (Gemini's pick, the orchestrator's option 3). Three steps:
  - discover insert use in the free LP;
  - commit inserts above a threshold: length >= 1 lane and full curve rows; lock the rest at 0;
  - at most one prune pass, which locks to 0 the committed inserts whose curve rows still stretch.
  The active set only shrinks, so it cannot cycle. Same bounded-rounds pattern as D418.
- **Candidate 4 (collapse a zero insert onto a neighbouring bend) fails**: a same-hand neighbour still needs g, so it needs a binary too.

## To watch
- Directions and flex interact: after the commit, directions are re-picked in sep-only rounds to slack 0 (bounded), then the prune pass.
- Curve rows on committed inserts need tags naming the insert (track, piece), so that the prune pass can find them.
