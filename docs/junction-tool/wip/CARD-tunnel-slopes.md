# CARD: tunnel end runs on sloped ground (from V's 197, domain-39)

**Context.** Our model (D155, D365) puts every tunnel on flat ground: each end is a portal plus one ramp tile dug into the surface, so a tunnel's straight end run is 2 pieces. V's PSG 197 builds every examined tunnel head on a one-level slope: the portal is the slope tile itself and the ground outside is one level lower, so the end run is the portal alone (1 piece), with a curve or a crossing right beside the head (e.g. L80's heads at game (105,164) and (108,164); L48's far head at (93,174) with a diagonal at (93,175)). The domain's game truth judges V's build illegal at 13 span breaks (plus knock-ons) only because of the flat-ground assumption; the answer key now reads V's terrain (domain-40).

**Options.**
- A. Keep flat ground (today): end run 2. Simple; Heights never terraforms beyond the ramp. Hubs are 1 tile longer per tunnel end than V's; V's 197 cannot be reproduced tile for tile.
- B. Allow end run 1 where Heights lowers the ground outside the head by one level (V's practice): Slots gets a per-end choice (1 or 2), Heights owns the terrain that makes it true. More compact, matches the corpus; costs a terrain step in Heights and a binary (or rule) per tunnel end in Slots.
- C. B as a measured probe first: the domain and the suite take per-end `ends`; Slots/the drawing program take end run 1 behind a flag; measure 197 and T2 compactness before Heights work.

**Recommendation.** C (reversible; corpus authority says allow what V builds; the cost is measured before Heights commits to terraforming).

**Prior art checked.** D155, D365 (flat ground, ramp tile), D390 (tunnels cross by depth), corpus-authority rule; V's save terrain via tools/junction/corpus/load.py (triage-197.md).

**Reversible.** Yes (a flag and a per-end field).
