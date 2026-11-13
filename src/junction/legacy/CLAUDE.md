# Junction tool: legacy/ (superseded, not extended)

- The code here is superseded by the graph pipeline and is not extended (D201, D216, G1 §14.1).
- `junction_generator` is still called by `tool/junction_gui.cpp` (an unreachable non-projection branch) and by `console_cmds.cpp`; `junction_generator.h` types such as `JunctionSpec` are used by many headers, so it is not deletable yet.
- `junction_nodes` is still called by `game/junction_game.cpp` (`SearchJunctionNodes` in `GenerateJunctionNear`) and is also a test oracle; `junction_skeleton` and `junction_throughput` are called only by the generator.
- New code must not add callers.
