cmake_minimum_required(VERSION 3.15)
project(dynamic_regex_h C)

set(CMAKE_C_STANDARD 11)

# Include directories for dependencies
include_directories(deps/dynamic_string.h)
include_directories(devdeps/unity)

# Add executable with all source files
add_executable(dynamic_regex_h 
    main.c
    regex.c
    devdeps/unity/unity.c
)
