cmake_minimum_required(VERSION 3.15)
project(dynamic_regex_h C)

set(CMAKE_C_STANDARD 11)

# Include directories for dependencies
include_directories(deps/dynamic_string.h)
include_directories(devdeps/unity)

# Add executable with all test files
add_executable(dynamic_regex_h 
    tests/test_runner.c
    tests/test_basic.c
    tests/test_anchors.c
    tests/test_charsets.c
    tests/test_escapes.c
    tests/test_quantifiers.c
    tests/test_groups.c
    tests/test_alternation.c
    tests/test_flags.c
    tests/test_api.c
    tests/test_boundaries.c
    tests/test_complex.c
    tests/test_edge_cases.c
    tests/test_performance.c
    regex.c
    int_stack.c
    devdeps/unity/unity.c
)
