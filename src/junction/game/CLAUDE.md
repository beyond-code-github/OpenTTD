# Junction tool: game/ (game seam, estimate, build, pricing)

- `junction_game` is the game seam (spec and pricing), `junction_build` the build sequencer, `junction_map_export` the map export (D112).

## Estimate, build and pricing
- A solved plan is built by posting ordinary commands in sequence; a failed command stops the sequence and reports what was and was not built (D7).
- The estimate is the exact sum of test-only runs of every command, re-checked just before building; building is refused when unaffordable (D9).
- Estimate and build cover terraform steps first; after a build an information message states `Spent X, quoted Y` (CN1, QL2, IG2).
- Building is not network-aware beyond the game's own commands; no new network command exists (D136(c), D69).
- Terraform costs: `CmdTerraformLand` and `CmdLevelLand` prices plus clearing (D95).
