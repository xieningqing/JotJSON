# [阶段 1 修改]
add_executable(jotjson_tests
    test_main.cpp
)

target_link_libraries(jotjson_tests PRIVATE JotJSON::jotjson)

# 复用根 CMakeLists.txt 定义的警告选项，
# 避免库和测试使用两套不一致的编译规则。
target_compile_options(
    jotjson_tests
    PRIVATE
        ${JOTJSON_WARNING_OPTIONS}
)

# 测试覆盖解析、访问器和错误处理，不再只是工程冒烟检查。
add_test(
    NAME jotjson_tests
    COMMAND jotjson_tests
)

set_tests_properties(jotjson_tests PROPERTIES
    LABELS "json;parser"
    # 让 ctest 展开测试程序的 PASS/FAIL 行，
    # 这样不必每次额外添加 -V。
    VERBOSE 1
)
