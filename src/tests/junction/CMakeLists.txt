add_subdirectory(game)
add_subdirectory(legacy)
add_subdirectory(line)
add_subdirectory(placement)
add_subdirectory(station)
add_subdirectory(terrain)
add_subdirectory(tool)
add_subdirectory(topology)

add_test_files(
    junction_compact.cpp
    junction_compactness.cpp
    junction_compactness.h
    junction_layout.cpp
    junction_route.cpp
    junction_rules.cpp
    junction_train_model.cpp
)
