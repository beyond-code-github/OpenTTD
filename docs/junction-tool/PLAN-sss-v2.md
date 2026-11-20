# PLAN: SSS v2 epics (D388; design DESIGN-sss-v2.md)

Each epic: goal, scope, exit criteria (tests at the layer's output, D367), and its first step. Every step inside an epic follows the working method: design and card in the orchestrator (proof first), then a brief to a Sonnet agent (test-first, outside-in, real code), oversight against the brief, STATUS updated. Rulings in DECISIONS.md; a question already ruled is never re-asked.

## Gates and kill criteria
- **Gate A (end of E3):** PSG 197 shaped at bundle level. Measure: channels, gadgets by type, free faces, cells, dummies, rigid cells, Shape time. Continue only if Shape shapes every handed-down 197 variant (H10) within 2 s per variant and every free face refines (H12, H16). Otherwise stop and rethink with the engineer, with the numbers.
- **Gate B (end of E5):** the feasibility oracle passes on T2, T3, LL2 and 197, and Slots solves 197's top variant within the seed budget (D297: 5 s) with zero clashes from the oracle. Otherwise stop and rethink with the engineer.

## E0 Consolidate (housekeeping, first)
- Goal: one branch with every live piece; the docs say v2.
- Scope: merge wt-slots and wt-cells into feature/junction-tool (resolve the planar CLAUDE.md conflict: D383's flex bullet vs H12b's cells bullet, rewritten for v2's Shape); planar CLAUDE "Architecture" names v2 and D388; SPEC-shape §8 scoped to free faces; mark SPEC-slots-lengths §8-§11 as v1 evidence pending E5's rewrite; the probe stays hidden (diagnosis only).
- Exit: feature/junction-tool gate green; CLAUDE.md files state only decided facts; pushed to the fork.

## E1 Structure for PSG 197 (D388 c)
- Goal: psg197.txt enters the pipeline; Structure produces variants with its promises.
- Scope: DESIGN-sss-v2 §3.1 items 1-6: authored crossings (notation and Structure), tunnel over tunnel, portal on a tunnel's middle tile, prio per merge, partial crossings of a bundle (per-lane crosser sets), lane-count changes (verify).
- Exit: `ParsePlanarTopology(psg197)` accepts the hub (psg197-gaps §1-2 closed); `BuildPlanarStructure` gives variants with P1-P8 and the new promises as tests; numbers recorded (tracks, channels, sites, crossings, structures, variants, time). T2/T3/LL2 unchanged (identity of their Structure output, or each change listed).
- First step: design card for the notation of authored crossings and per-lane crosser sets (reread D343, D344, psg197-gaps, psg197-decisions).

## E2 Rule fields from Structure
- Goal: no stand-ins: Structure hands down every lower bound and exact value Slots needs (DESIGN-sss-v2 §3.1 "Hands down").
- Scope: the fields of SPEC-slots-lengths §8.1 per variant, coverage exact (D384), from the spec and Structure's own model; tests per field.
- Exit: fields on `PlanarStructureVariant` with tests; values agree with the spec's helpers (`DeriveCurveSpacing`, `JunctionPrioTiles`, ...).

## E3 Shape v2 (bundles, gadgets, cells between bundles)
- Goal: Shape decides spines, gadgets, flex marks and free-face cells, with H1-H16.
- Scope: H14 spines (one heading list per channel); the gadget catalogue as data (C2-C9 per lane count, internal tile geometry, ports, parameters) with its own verification test against the real tile clash rule; H15 gadget instances; H11 per run; flex marks on spine diagonals; H12/H16 cells on free faces only (reuse `RefinePlanarShapeCells`).
- Exit: T2, T3, LL2 and 197 shaped, H1-H16 green; Gate A numbers recorded.
- First step: design the gadget catalogue's data form and its verification (DESIGN-components C2-C9; D342, D353, D110, D355, D374), card to the engineer.

## E4 Shape speed
- Goal: Shape within budget at 197's size (LL2 is 8.8 s today, D297 seed 5 s).
- Scope: measure first (which rows and variables dominate); bundle level should already shrink the program; then targeted changes with the same output (identity test).
- Exit: every hub's variants shaped within the Gate A budget.
- May run in parallel with E3's later steps only if it touches different code (one writer per worktree).

## E5 Slots v2
- Goal: Slots solves every handed-down variant with DESIGN-sss-v2 §3.3's rows; zero clashes by construction.
- Scope: rewrite SPEC-slots-lengths for v2 (rows by family, the G-planar argument's open points (a)-(d) closed for free faces); then build test-first: corridor rows, gadget ports, cell closure and landing, clearance, exact rows, D382 parity (port from the probe), objective. The feasibility oracle (D388 b) as a test over T2, T3, LL2, 197.
- Exit: Gate B.

## E6 Heights and Signals to a built hub
- Goal: T2, T3, LL2 and 197 built end to end in game, checker clean.
- Scope: Heights (D365; tunnel levels from Structure, tunnel over tunnel), Signals (including choice by free tunnel for 197, psg197-gaps §3), the hover (D370).
- Exit: each hub built in a rig company the engineer can join, checker clean; 197 recognisable against V's original.

## E7 Corpus
- Goal: any coop hub from first principles (ROADMAP corpus growth).
- Scope: more corpus topologies through the feasibility oracle and the build; every failure a named promise.
- Exit: agreed with the engineer at Gate B.

## Order
E0 -> E1 -> E2 -> E3 (Gate A) -> E4 -> E5 (Gate B) -> E6 -> E7. E2 may start once E1's Structure model is settled; E4 may overlap E3's later steps.
