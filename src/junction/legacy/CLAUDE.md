# Junction tool: legacy/ (superseded, not extended)

- The code here is superseded by the graph pipeline and is not extended.
- `junction_generator` is still called by `tool/junction_gui.cpp` and by `console_cmds.cpp`; it is not deletable yet. These still include `junction_generator.h`: `game/junction_game.h`, `game/junction_game.cpp`, `tool/junction_gui.cpp`, legacy's own `junction_nodes.h` and `junction_throughput.h`, and the tests `game/junction_game`, `game/junction_map`, `junction_compact`, `junction_rules`, `legacy/junction_design`, `legacy/junction_generator`, `legacy/junction_nodes`, `legacy/junction_throughput`, `topology/junction_chain`, `topology/junction_signal_pass`, `topology/junction_topology`.
- `SizeJunctionPrio` has no live caller; only legacy tests call it.
- `junction_nodes` is still called by `game/junction_game.cpp` (`SearchJunctionNodes` in `GenerateJunctionNear`) and is also a test oracle; `junction_skeleton` and `junction_throughput` are called only by the generator.
- New code must not add callers.
