cmake_minimum_required(VERSION 3.29)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

project(dxbc
    VERSION 2.7.0
    DESCRIPTION "DXVK's DXBC"
    LANGUAGES CXX)

file(GLOB SOURCES
    "src/spirv/*.cpp"
    "src/util/*.cpp"
)

add_library(lsfg-vk SHARED ${SOURCES})

# target
set_target_properties(lsfg-vk PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON)
target_include_directories(lsfg-vk
    PUBLIC include/spirv include/util
    PUBLIC include/windows)
set_target_properties(lsfg-vk PROPERTIES
    EXPORT_COMPILE_COMMANDS ON)
