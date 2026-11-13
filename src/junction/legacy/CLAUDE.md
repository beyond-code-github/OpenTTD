# Junction tool: legacy/ (superseded, not extended)

- The code here is superseded by the graph pipeline and is not extended (D201, D216, G1 §14.1).
- `junction_generator` is still called by `tool/junction_gui.cpp` (the window's library job calls `SizeJunctionPrio` and `BuildJunctionLibrary` live on every spec change, in `EnsureLibrary`) and by `console_cmds.cpp`; it is not deletable yet. After GS1 these still include `junction_generator.h`: `game/junction_game.h`, `game/junction_game.cpp`, `tool/junction_gui.cpp`, legacy's own `junction_nodes.h` and `junction_throughput.h`, and the tests `game/junction_game`, `game/junction_map`, `junction_compact`, `junction_rules`, `legacy/junction_design`, `legacy/junction_generator`, `legacy/junction_nodes`, `legacy/junction_throughput`, `topology/junction_chain`, `topology/junction_signal_pass`, `topology/junction_topology`.
- The live prio-length sizing (`SizeJunctionPrio`, called by tool/) still runs on the legacy node model.
- `junction_nodes` is still called by `game/junction_game.cpp` (`SearchJunctionNodes` in `GenerateJunctionNear`) and is also a test oracle; `junction_skeleton` and `junction_throughput` are called only by the generator.
- New code must not add callers.
