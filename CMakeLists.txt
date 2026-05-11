cmake_minimum_required(VERSION 3.14)
project(RosettaUnit
    VERSION 0.1.0
    DESCRIPTION "Lightweight i18n library for C++ and beyond"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Hide non-exported symbols by default so the C ABI surface is tight.
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# ---- Library targets -------------------------------------------------------

# Static C++ library — for embedding in C++ projects.
add_library(rosetta_unit STATIC
    src/rosetta_unit.cpp
)
target_include_directories(rosetta_unit PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Shared library exposing the C ABI — for Python/C#/Rust bindings.
add_library(rosetta_unit_c SHARED
    src/rosetta_unit.cpp
    src/rosetta_unit_c.cpp
)
target_include_directories(rosetta_unit_c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_definitions(rosetta_unit_c PRIVATE ROSETTA_UNIT_BUILD)
set_target_properties(rosetta_unit_c PROPERTIES
    OUTPUT_NAME "rosetta_unit"
)

# ---- Examples & tests ------------------------------------------------------

option(ROSETTA_UNIT_BUILD_EXAMPLES "Build examples" ON)
option(ROSETTA_UNIT_BUILD_TESTS    "Build tests"    ON)

if(ROSETTA_UNIT_BUILD_EXAMPLES)
    add_executable(hello_example examples/hello.cpp)
    target_link_libraries(hello_example PRIVATE rosetta_unit)
endif()

if(ROSETTA_UNIT_BUILD_TESTS)
    enable_testing()
    add_executable(rosetta_tests tests/test_basic.cpp)
    target_link_libraries(rosetta_tests PRIVATE rosetta_unit)
    add_test(NAME rosetta_tests COMMAND rosetta_tests)
endif()

# ---- Install ---------------------------------------------------------------

include(GNUInstallDirs)
install(TARGETS rosetta_unit rosetta_unit_c
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
