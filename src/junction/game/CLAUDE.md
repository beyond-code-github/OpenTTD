# Junction tool: game/ (game seam, estimate, build, pricing)

- `junction_game` is the game seam (spec and pricing), `junction_build` the build sequencer, `junction_map_export` the map export.

## Estimate, build and pricing
- A solved plan is built by posting ordinary commands in sequence; a failed command stops the sequence and reports what was and was not built.
- The estimate test-runs the rail pieces on unplanned tiles, the bridges and tunnels whose heads and span touch no planned tile, and every station; rail pieces on planned tiles are priced from the planned slope's foundation, bridges touching a planned tile from the bridge price, signals at the flat signal cost, tunnels always by test run, and the terraform cost is added on top; the estimate is re-checked just before building, which is refused when unaffordable.
- Estimate and build cover terraform steps first; after a build an information message states `Spent X, quoted Y`.
- Building is not network-aware beyond the game's own commands; no new network command exists.
- Terraform costs: `CmdTerraformLand` and `CmdLevelLand` prices plus clearing.
