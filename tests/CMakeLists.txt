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
