# CARD: the domain of legal track geometry, and a rule test suite against it

Status: design card for the engineer, 2026-11-22. Engineer: "Surely we should be able to reason about all of these rules offline with some kind of unit test suite"; "This is essentially our 'domain'".

## Context
- Slots' keep-apart rows (pairwise sides, skip rules, zero contraction, span exemptions, crossing flanks) are an encoding of one question: is this track geometry legal in the game? Today the only check of that encoding has been whole hubs. A leak shows up as a clash on some hub, and a fix tuned on one hub breaks another. This session: three skip rules and three cuts of the flank rule, each found from one picture.
- Example: the broad flank rule made T2 clean but made T3 face 38 provably undrawable; the parallel-only version keeps T3 drawable and lets T2's case through.

## The design
1. **The domain: one legality predicate, independent of the solver.**
   - Input: a set of tracks as point lists (half tiles), with their crossings and structures (which track is the separator, bridge or tunnel) and their nodes.
   - Output: legal or not, and why (the tiles and tracks that break).
   - Rule source: the game, not our rows. The tracks are rasterised to tile pieces (`RasterizeJunctionRoute`), and the tile rule `JunctionTracksMayShareTile` is applied, with structure middles exempt for their listed crossers only. Node tiles are judged by the node rule (D400). Curve spacing follows `JunctionCurveGaps`.
   - It lives in its own small module with its own tests (hand cases from the game: a legal switch, the X|Y diamond, a deck over a crosser, two decks on one tile).
2. **The encoding check: our rows against the domain, on enumerated local configurations.**
   - Configurations, one per rule family:
     - two legs at a node, and two nodes close together;
     - a crossing with an S-bend just before or after it, on either track;
     - a crosser beside or under a deck, and two crossers on one deck;
     - two lanes of one bundle turning a corner, with and without a crossing in the turn;
     - a track end near another track's middle;
     - a copy opening or closing beside its parent;
     - a prio row beside its lane.
   - Each is a tiny topology of two to four tracks. Every heading combination Shape allows is enumerated, with lengths 0 to 4 half tiles at grid parities: thousands of geometries per configuration, milliseconds each.
   - Each geometry is judged twice: our rows (points pinned with `fix_points`, window 0, elastic: any slack means rejected) and the domain predicate.
   - Report every disagreement. Rows reject but the domain accepts means a rule is too strict. Rows accept but the domain rejects means a rule is too loose. Each comes with the configuration and the geometry, as a ready-made regression test.
3. **How rules change from now on.** A rule change lands only with the suite green on every configuration: no disagreement, or each one named and ruled. Hubs are the integration test, no longer the place rules are discovered.

## What it settles
- The skip rules and the flank rule become general statements, proven on every small case, instead of tuned on T2 and T3.
- "Clean" means the domain predicate passes on the whole drawing, not only the clash script (which checks overlaps and touches, not spacing).
- The predicate is the first half of the hand-off to Heights and Signals (rasterising to tiles), so it is not throw-away.

## Steps (each one agent, test-first)
1. The domain predicate and its hand cases (C++, in `src/junction/planar/`, with `[planardomain]` tests).
2. The enumerator and the encoding report for two configurations (S-bend at a crossing, legs at a node), using today's rows with all three probes on.
3. The remaining configurations.
4. Fix the rules from the report, one general rule at a time; then run the hubs.

## Options
- **A (recommended):** build it as above, the predicate first.
- **B:** keep tuning rules on hubs with the clash script. Faster per step, but it is what produced this session's back and forth.

## Prior art
D400 and its `[planarnode]` harness (node legs on the tile grid against the real tile rule) is this pattern for one configuration. D361 (no later layer repairs) and D381 (keep-apart rows by construction) are what this tests.

Reversible: yes.
