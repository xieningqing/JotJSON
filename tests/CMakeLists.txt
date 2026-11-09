# [阶段 1 修改]
add_executable(jotjson_tests
    test_main.cpp
)

# [阶段 2 新增]
# 序列化测试与解析测试分开，失败时更容易定位。
add_executable(jotjson_serialize_tests
    test_serialize.cpp
)

# [阶段 3 新增]
# 严格语义测试单独成目标，避免语法、边界和序列化问题混在一起。
add_executable(jotjson_strict_tests
    test_strict.cpp
)

# [阶段 4 新增]
# 基线测试单独链接分配统计实现。
# 统计只作用于该测试可执行文件，不影响库和其他测试。
add_executable(jotjson_baseline_tests
    test_baseline.cpp
    allocation_probe.cpp
)

# [阶段 5 新增]
# 固定合法/非法语料测试。
add_executable(jotjson_stage5_tests
    test_stage5.cpp
)

target_link_libraries(jotjson_tests PRIVATE JotJSON::jotjson)
target_link_libraries(
    jotjson_serialize_tests
    PRIVATE
        JotJSON::jotjson
)
target_link_libraries(
    jotjson_strict_tests
    PRIVATE
        JotJSON::jotjson
)

target_link_libraries(
    jotjson_baseline_tests
    PRIVATE
        JotJSON::jotjson
)

target_link_libraries(
    jotjson_stage5_tests
    PRIVATE
        JotJSON::jotjson
)

# 复用根 CMakeLists.txt 定义的警告选项，
# 避免库和测试使用两套不一致的编译规则。
target_compile_options(
    jotjson_tests
    PRIVATE
        ${JOTJSON_WARNING_OPTIONS}
)

target_compile_options(
    jotjson_serialize_tests
    PRIVATE
        ${JOTJSON_WARNING_OPTIONS}
)

target_compile_options(
    jotjson_strict_tests
    PRIVATE
        ${JOTJSON_WARNING_OPTIONS}
)

target_compile_options(
    jotjson_baseline_tests
    PRIVATE
        ${JOTJSON_WARNING_OPTIONS}
)

target_compile_options(
    jotjson_stage5_tests
    PRIVATE
        ${JOTJSON_WARNING_OPTIONS}
)

# [阶段 4 新增]
# 在 MinGW 下通过链接器包装 malloc 家族统计分配。
# 不替换 new/delete，避免探针内存与运行库内存管理器不匹配。
if(MINGW)
    target_link_options(
        jotjson_baseline_tests
        PRIVATE
            -Wl,--wrap=malloc
            -Wl,--wrap=free
            -Wl,--wrap=calloc
            -Wl,--wrap=realloc
            -Wl,--wrap=_aligned_malloc
            -Wl,--wrap=_aligned_free
    )
endif()

# 测试覆盖解析、访问器和错误处理，不再只是工程冒烟检查。
add_test(
    NAME jotjson_tests
    COMMAND jotjson_tests
)

add_test(
    NAME jotjson_serialize_tests
    COMMAND jotjson_serialize_tests
)

add_test(
    NAME jotjson_strict_tests
    COMMAND jotjson_strict_tests
)

add_test(
    NAME jotjson_baseline_tests
    COMMAND jotjson_baseline_tests
)

add_test(
    NAME jotjson_stage5_tests
    COMMAND jotjson_stage5_tests
)

set_tests_properties(jotjson_tests PROPERTIES
    LABELS "json;parser"
    # 让 ctest 展开测试程序的 PASS/FAIL 行，
    # 这样不必每次额外添加 -V。
    VERBOSE 1
)

set_tests_properties(jotjson_serialize_tests PROPERTIES
    LABELS "json;serializer"
    VERBOSE 1
)

set_tests_properties(jotjson_strict_tests PROPERTIES
    LABELS "json;strict"
    VERBOSE 1
)

set_tests_properties(jotjson_baseline_tests PROPERTIES
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}
    LABELS "json;baseline"
    VERBOSE 1
)

set_tests_properties(jotjson_stage5_tests PROPERTIES
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}
    LABELS "json;stage5;corpus"
    VERBOSE 1
)

# [阶段 5 新增]
if(JOTJSON_ENABLE_SANITIZERS AND
   CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(sanitizer_target
        jotjson_tests
        jotjson_serialize_tests
        jotjson_strict_tests
        jotjson_baseline_tests
        jotjson_stage5_tests)
        target_compile_options(
            ${sanitizer_target}
            PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
        )

        target_link_options(
            ${sanitizer_target}
            PRIVATE
                -fsanitize=address,undefined
        )
    endforeach()
endif()
