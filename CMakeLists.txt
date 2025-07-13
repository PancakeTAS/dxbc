cmake_minimum_required(VERSION 3.29)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

project(dxbc
    VERSION 2.7.0
    DESCRIPTION "DXVK's DXBC"
    LANGUAGES CXX)

file(GLOB SOURCES
    "src/dxbc/*.cpp"
    "src/spirv/*.cpp"
    "src/util/*.cpp"
)

add_library(dxbc STATIC ${SOURCES})

# target
set_target_properties(dxbc PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    EXPORT_COMPILE_COMMANDS ON)
target_include_directories(dxbc
    PUBLIC include/dxbc
    PUBLIC include/spirv include/util include/dxvk)
add_compile_options(dxbc
    -fPIC)
