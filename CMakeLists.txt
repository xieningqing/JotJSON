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

# [阶段 5 新增]
option(
    JOTJSON_BUILD_BENCHMARKS
    "Build JotJSON benchmarks"
    OFF
)

option(
    JOTJSON_BUILD_FUZZER
    "Build the JotJSON fuzz harness"
    OFF
)

# [阶段 5 新增]
option(
    JOTJSON_USE_LIBFUZZER
    "Compile the fuzz target for a libFuzzer runtime"
    OFF
)

option(
    JOTJSON_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)

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
    src/serialize.cpp
)

add_library(JotJSON::jotjson ALIAS jotjson)

target_include_directories(jotjson
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(jotjson PUBLIC cxx_std_11)
target_compile_options(jotjson PRIVATE ${JOTJSON_WARNING_OPTIONS})

# [阶段 5 新增]
# Sanitizer 只挂在项目目标上，不污染调用方。
if(JOTJSON_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(JOTJSON_SANITIZER_COMPILE_FLAGS
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )

        set(JOTJSON_SANITIZER_LINK_FLAGS
            -fsanitize=address,undefined
        )

        target_compile_options(
            jotjson
            PRIVATE
                ${JOTJSON_SANITIZER_COMPILE_FLAGS}
        )

        target_link_options(
            jotjson
            PRIVATE
                ${JOTJSON_SANITIZER_LINK_FLAGS}
        )
    else()
        message(WARNING
            "JOTJSON_ENABLE_SANITIZERS was requested, "
            "but this compiler is not GCC or Clang")
    endif()
endif()

if(JOTJSON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(JOTJSON_BUILD_EXAMPLES)
    add_executable(jotjson_example example/basic.cpp)
    target_link_libraries(jotjson_example PRIVATE JotJSON::jotjson)
    target_compile_options(jotjson_example PRIVATE ${JOTJSON_WARNING_OPTIONS})

    # [阶段 2 新增]
    # 直接展示紧凑序列化和美化序列化的实际输出。
    add_executable(jotjson_serialize_example example/serialize.cpp)
    target_link_libraries(
        jotjson_serialize_example
        PRIVATE
            JotJSON::jotjson
    )
    target_compile_options(
        jotjson_serialize_example
        PRIVATE
            ${JOTJSON_WARNING_OPTIONS}
    )
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

# [阶段 5 新增]
if(JOTJSON_BUILD_BENCHMARKS)
    add_executable(
        jotjson_bench_parse
        benchmarks/bench_parse.cpp
    )

    add_executable(
        jotjson_bench_serialize
        benchmarks/bench_serialize.cpp
    )

    target_link_libraries(
        jotjson_bench_parse
        PRIVATE
            JotJSON::jotjson
    )

    target_link_libraries(
        jotjson_bench_serialize
        PRIVATE
            JotJSON::jotjson
    )

    target_compile_options(
        jotjson_bench_parse
        PRIVATE
            ${JOTJSON_WARNING_OPTIONS}
    )

    target_compile_options(
        jotjson_bench_serialize
        PRIVATE
            ${JOTJSON_WARNING_OPTIONS}
    )

    if(JOTJSON_ENABLE_SANITIZERS AND
       CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            jotjson_bench_parse
            PRIVATE
                ${JOTJSON_SANITIZER_COMPILE_FLAGS}
        )

        target_compile_options(
            jotjson_bench_serialize
            PRIVATE
                ${JOTJSON_SANITIZER_COMPILE_FLAGS}
        )

        target_link_options(
            jotjson_bench_parse
            PRIVATE
                ${JOTJSON_SANITIZER_LINK_FLAGS}
        )

        target_link_options(
            jotjson_bench_serialize
            PRIVATE
                ${JOTJSON_SANITIZER_LINK_FLAGS}
        )
    endif()
endif()

# [阶段 5 新增]
if(JOTJSON_BUILD_FUZZER)
    add_executable(
        jotjson_fuzz_parse
        fuzz/fuzz_parse.cpp
    )

    target_link_libraries(
        jotjson_fuzz_parse
        PRIVATE
            JotJSON::jotjson
    )

    target_compile_options(
        jotjson_fuzz_parse
        PRIVATE
            ${JOTJSON_WARNING_OPTIONS}
    )

    set_target_properties(
        jotjson_fuzz_parse
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY
                "${PROJECT_BINARY_DIR}/bin"
    )

    if(JOTJSON_ENABLE_SANITIZERS AND
       CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            jotjson_fuzz_parse
            PRIVATE
                ${JOTJSON_SANITIZER_COMPILE_FLAGS}
        )

        target_link_options(
            jotjson_fuzz_parse
            PRIVATE
                ${JOTJSON_SANITIZER_LINK_FLAGS}
        )
    endif()

    if(JOTJSON_USE_LIBFUZZER AND
       CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_definitions(
            jotjson_fuzz_parse
            PRIVATE
                JOTJSON_USE_LIBFUZZER=1
        )

        target_compile_options(
            jotjson_fuzz_parse
            PRIVATE
                -fsanitize=fuzzer
        )

        target_link_options(
            jotjson_fuzz_parse
            PRIVATE
                -fsanitize=fuzzer
        )
    elseif(JOTJSON_USE_LIBFUZZER)
        message(WARNING
            "JOTJSON_USE_LIBFUZZER was requested, "
            "but this compiler is not Clang")
    endif()
endif()
