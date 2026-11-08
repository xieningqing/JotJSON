cmake_minimum_required(VERSION 3.20)

project(
    JotJSON
    VERSION 0.1.0
    DESCRIPTION "A strict JSON library for learning modern C++11"
    LANGUAGES CXX
)

# Keep single-config generators deterministic while letting multi-config
# generators choose their own configuration.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS Debug Release
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(JOTJSON_BUILD_TESTS "Build JotJSON tests" ON)
option(JOTJSON_BUILD_EXAMPLES "Build JotJSON examples" ON)
option(JOTJSON_BUILD_CLI "Build the JotJSON command-line tool" ON)

set(JOTJSON_WARNING_OPTIONS
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)

# The library contains implementation files only. The CLI has its own target.
# [阶段 1 修改]
# 解析库由版本信息、DOM 访问器和 JSON 解析器组成。
add_library(jotjson STATIC
    src/version.cpp
    src/value.cpp
    src/parse.cpp
)

add_library(JotJSON::jotjson ALIAS jotjson)

target_include_directories(jotjson
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(jotjson PUBLIC cxx_std_11)
target_compile_options(jotjson PRIVATE ${JOTJSON_WARNING_OPTIONS})

if(JOTJSON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(JOTJSON_BUILD_EXAMPLES)
    add_executable(jotjson_example example/basic.cpp)
    target_link_libraries(jotjson_example PRIVATE JotJSON::jotjson)
    target_compile_options(jotjson_example PRIVATE ${JOTJSON_WARNING_OPTIONS})
endif()

if(JOTJSON_BUILD_CLI)
    add_executable(jotjson_cli src/main.cpp)
    set_target_properties(jotjson_cli PROPERTIES
        OUTPUT_NAME jotjson
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
    )
    target_link_libraries(jotjson_cli PRIVATE JotJSON::jotjson)
    target_compile_options(jotjson_cli PRIVATE ${JOTJSON_WARNING_OPTIONS})
endif()
