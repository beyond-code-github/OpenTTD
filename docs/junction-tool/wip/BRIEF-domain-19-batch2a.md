# BRIEF domain-19: configurations batch 2a, a copy beside its separator (Y1) and two lanes turning a corner (L1)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall. Test code only (no gate). One Release run per configuration, rule set `span+lead+endnode+crosspiece+grid`. Record findings; fix no rule. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The domain suite (`src/tests/junction/planar/planar_domain_encoding.cpp`) is exact on X1, X2, M1 and K1 (contracted judging, D432; grid on). Batch 2a adds the bundle families behind the T3/LL2 failures: a copy (D351/D371; corridor rows D421: one lane from its parent, an exact one-lane opening and closing step) and sibling lanes through a corner with a crossing in the turn (keep-apart between parallel lanes through bends; D426 sibling lanes, off by default).
- These fixtures use Shape's own heading solve, `ShapePlanarStructureVariant(structure_variant, out, why)` (planar_shape_layer.h), because `LoadPlanarShapeVariant` does not take maps with copies. The base geometry comes from the drawing program itself (below), so the fixture needs no hand-placed points.

## 2. Fixtures (texts verbatim; a refusal by the parser, the structure loader or Shape is a stop: report its message)

**Y1, X1 with A1 doubled over its tunnel:**
- topology: X1's text unchanged.
- structure: `tunnel T1 A1: B1` / `copy A1 over T1` / `straight AC1: A1` / `straight BD1: B1`
- shape: `ShapePlanarStructureVariant` (Shape's own; `PlacePlanarShapeFlex` runs inside it).

**L1, two lanes from W, the left lanes turning north across the right lanes' straight run:**
- topology: `topology L1` / `port W in E: AB1, AB2` / `port N out N: A1, A2` / `port E out E: B1, B2` / `node AB1 -> A1, B1` / `node AB2 -> A2, B2` / `cross B1: A2<` / `cross A2: B1>`
- structure: `tunnel T1 B1: A2` / `straight AB1: B1` / `straight AB2: B2`
- shape: `ShapePlanarStructureVariant`.

**Base geometry for both:** one drawing by the program: `CheckPlanarDrawable(v, 15.0, o)` with the rule set's options plus `grid = true`, `tidy = true`, integer (not elastic). It must be Optimal; then the domain must accept its points (`PlanarDomainInputOf`). Either failing is a stop (report status, conflicts, or the domain's breaks): it is itself a finding.

## 3. Configurations (through the on-grid generator; a geometry varies pieces of the base drawing, others at base)
- **Y1a (the copy's opening):** the copy track's pieces from its split node to its tunnel's first end piece, and A1's pieces over the same stretch: each flex insert or step in {0,1,2} (axis even), each flank absorbing so the tunnel crossing stays where the base has it (skip a geometry that would move it).
- **Y1b (the copy's closing):** the same at the far side, from the tunnel's last end piece to the copy's merge node.
- **L1a (the corner):** every flex insert of A1 and A2 between their split and their N port in {0,1,2} (axis even); the run after the last insert absorbs so the N port end keeps its x.
- **L1b (the crossing in the turn):** A2's pieces within two of its crossing piece in {0..4} (diagonal any, axis even), B1 at base.

For each fixture: a fast test `"planar domain: <Y1|L1> loads and its base is legal"` and a slow `[.][planardomainenc]` test via `RunEncoding` (env `PLANAR_DOMAIN_REPORT_<Y1|L1>`). Report per configuration the lead-row count as now, and the number of corridor (`corridor`) rows in the read-out (Y1 must have some; 0 is a finding).

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5` (under 1 s Debug).
- Release once each: Y1 and L1, `span+lead+endnode+crosspiece+grid` (with `corridor = true` in the read-out: the pipeline turns it on, D421), reports `encoding-y1-...txt`, `encoding-l1-...txt`, timeout 300.
- One commit: `Add: Junction: domain configurations Y1 (a copy) and L1 (lanes turning a corner) (D428)`, Co-Authored-By Claude Sonnet 5.5.

## 5. Report (short)
Commit; Shape's headings for each track (one line each); the base drawing's status and size; per configuration: classes with group keys, counts and first examples; lead and corridor row counts; wall times; state. When you have reported, stop.
