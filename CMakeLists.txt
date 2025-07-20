cmake_minimum_required(VERSION 3.29)

project(dxbc
    VERSION 2.6.2
    DESCRIPTION "DXVK's DXBC"
    LANGUAGES CXX)

file(GLOB SOURCES
    "src/dxbc/*.cpp"
    "src/spirv/*.cpp"
    "src/util/*.cpp"
)

add_library(dxbc STATIC ${SOURCES})

# target
add_subdirectory(spirv)
set_target_properties(dxbc PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    EXPORT_COMPILE_COMMANDS ON)
target_include_directories(dxbc SYSTEM
    PUBLIC include/dxbc
    PUBLIC include/spirv include/util include/dxvk)
target_link_libraries(dxbc
    SPIRV-Headers)
target_compile_options(dxbc PRIVATE
    -fPIC)
