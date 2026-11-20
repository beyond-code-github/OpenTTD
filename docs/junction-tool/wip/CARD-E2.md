# CARD E2: rule fields from Structure (orchestrator, 2026-11-21)

Implements PLAN-sss-v2 E2 and DESIGN-sss-v2 §3.1 "Hands down": Slots reads every lower bound and exact value from the variant, never from the spec (D366), as it already reads `limit` and `side_gap`. Values: SPEC-slots-lengths §8.1 (half-tile units, the model's).

## The fields (`PlanarRuleFields`, on `PlanarStructureVariant::rules`)
| Field | Value | §8.1 row |
|---|---|---|
| `tl` | spec.tl (tiles) | curve, bay |
| `cl` | `DeriveCurveSpacing(reference_speed, curve_speed, tl)` | curve |
| `curve_same` | min(2 tl - 1, max(2, cl)) | curve: consecutive same hand |
| `curve_opposite` | 2 | curve: consecutive opposite hands |
| `curve_window` | {min(2 tl - 1, 2 cl), min(2 tl - 1, 3 cl)} (k = 2, 3) | curve: k apart |
| `bay` | 2 tl + 4 | bay |
| `coverage` | 2 prio_tiles + 4, exact (D384) | coverage |
| `node_leg` | 2 | node_leg (D110's trident exception is Slots' rule, not a value) |
| `cross` | 1 | cross |
| `deck_gap` | 2 | deck gap |
| `sd` | spec.sd (tiles) | landing: Lengths derives the landing from the signal spacing (D375) |
Not fields: piece and parity (game geometry, Slots' own), `clear` and landing's base (derived from Shape's `end_run`).

## Mechanism
- `PlanarRuleFields PlanarRuleFieldsOf(const JunctionSpec &spec)` beside `PlanarSideGaps`; set wherever `side_gap` is set.
- P9 in `CheckPlanarStructurePromise`: the variant's rules equal `PlanarRuleFieldsOf(spec)`; one message naming the first field that differs.
- Tests: P9 red by tampering (as P8's test); T2 and 197 variants carry the spec's values; on the test spec tl 4, bay 12, coverage 32.
