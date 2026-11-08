// [阶段 1 补全]
#include <jotjson/jotjson.hpp>

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

// 失败计数。
// 断言失败后继续运行，方便一次发现多个问题。
int failures = 0;

// 输出一条断言结果。
//
// 成功也打印 PASS，这样 ctest -V 能够看到具体测试点。
// 失败写入 stderr，成功写入 stdout。
void expect(
    bool condition,
    const std::string& message) {
    if (condition) {
        std::cout
            << "PASS: "
            << message
            << "\n";
        return;
    }

    std::cerr
        << "FAIL: "
        << message
        << "\n";

    ++failures;
}

// 断言输入必须解析失败，并检查错误码和错误位置。
void expect_parse_error(
    const std::string& input,
    jotjson::error_code expected_code,
    const std::string& name) {
    try {
        jotjson::parse_copy(input);

        std::cerr
            << "FAIL: "
            << name
            << ": expected parse failure\n";

        ++failures;
    } catch (const jotjson::parse_error& error) {
        // 一组非法输入只输出一个测试点。
        // 错误码、偏移和行列号共同决定这一项是否通过。
        const bool code_ok =
            error.code() == expected_code;

        const bool offset_ok =
            error.location().byte_offset <= input.size();

        const bool location_ok =
            error.location().line >= 1 &&
            error.location().column >= 1;

        if (code_ok && offset_ok && location_ok) {
            std::cout
                << "PASS: "
                << name
                << "\n";
            return;
        }

        std::cerr
            << "FAIL: "
            << name
            << "\n";

        if (!code_ok) {
            std::cerr
                << "  expected error code, got: "
                << static_cast<int>(error.code())
                << "\n";
        }

        if (!offset_ok) {
            std::cerr
                << "  error byte offset is outside input\n";
        }

        if (!location_ok) {
            std::cerr
                << "  error line or column is invalid\n";
        }

        ++failures;
    } catch (const std::exception& error) {
        std::cerr
            << "FAIL: "
            << name
            << ": wrong exception type: "
            << error.what()
            << "\n";

        ++failures;
    }
}

} // namespace

int main() {
    // 1. 库入口。
    expect(
        std::string(jotjson::library_name()) ==
            "JotJSON",
        "library_name() should return JotJSON");

    expect(
        std::string(jotjson::version_string()) ==
            "0.1.0",
        "version_string() should return 0.1.0");

    // 2. null。
    {
        jotjson::document document =
            jotjson::parse_copy("null");

        expect(
            document.root().is_null(),
            "null should parse");

        expect(
            document.root().type() ==
                jotjson::value_type::nil,
            "null type should be value_type::nil");
    }

    // 3. 布尔值。
    {
        jotjson::document document =
            jotjson::parse_copy("true");

        expect(
            document.root().is_bool() &&
                document.root().as_bool(),
            "true should parse");
    }

    {
        jotjson::document document =
            jotjson::parse_copy("false");

        expect(
            document.root().is_bool() &&
                !document.root().as_bool(),
            "false should parse");
    }

    // 4. 整数。
    {
        jotjson::document document =
            jotjson::parse_copy("0");

        expect(
            document.root().as_int64() == 0,
            "zero should parse");
    }

    {
        jotjson::document document =
            jotjson::parse_copy("123");

        expect(
            document.root().as_int64() == 123,
            "positive integer should parse");
    }

    {
        jotjson::document document =
            jotjson::parse_copy("-42");

        expect(
            document.root().as_int64() == -42,
            "negative integer should parse");
    }

    {
        jotjson::document document =
            jotjson::parse_copy("9007199254740993");

        expect(
            document.root().as_int64() ==
                static_cast<std::int64_t>(
                    9007199254740993LL),
            "large int64 should stay exact");
    }

    // 5. 浮点数。
    {
        jotjson::document document =
            jotjson::parse_copy("1.25");

        const double value =
            document.root().as_double();

        expect(
            value > 1.24 && value < 1.26,
            "fraction should parse as double");
    }

    {
        jotjson::document document =
            jotjson::parse_copy("1e3");

        const double value =
            document.root().as_double();

        expect(
            value > 999.0 && value < 1001.0,
            "exponent should parse as double");
    }

    // 6. 字符串。
    {
        jotjson::document document =
            jotjson::parse_copy("\"hello\"");

        expect(
            document.root().as_string() == "hello",
            "plain string should parse");

        expect(
            document.root().size() == 5,
            "string size should be byte count");
    }

    // 7. 普通转义。
    {
        jotjson::document document =
            jotjson::parse_copy(
                "\"line\\nquote\\\"slash\\\\\"");

        expect(
            document.root().as_string() ==
                "line\nquote\"slash\\",
            "escape sequences should decode");
    }

    // 8. Unicode 转 UTF-8。
    {
        jotjson::document document =
            jotjson::parse_copy(
                "\"\\u4F60\\u597D\"");

        const std::string expected =
            "\xE4\xBD\xA0\xE5\xA5\xBD";

        expect(
            document.root().as_string() == expected,
            "unicode escapes should become UTF-8");
    }

    // 9. UTF-16 代理对。
    {
        jotjson::document document =
            jotjson::parse_copy(
                "\"\\uD83D\\uDE00\"");

        const std::string expected =
            "\xF0\x9F\x98\x80";

        expect(
            document.root().as_string() == expected,
            "surrogate pair should become UTF-8");
    }

    // 10. 数组。
    {
        jotjson::document document =
            jotjson::parse_copy("[1,2,3]");

        const jotjson::value& root =
            document.root();

        expect(
            root.is_array() && root.size() == 3,
            "array should have three elements");

        expect(
            root.at(1).as_int64() == 2,
            "array index 1 should be 2");
    }

    // 11. 嵌套数组。
    {
        jotjson::document document =
            jotjson::parse_copy("[[1],[2,3]]");

        const jotjson::value& root =
            document.root();

        expect(
            root.at(0).at(0).as_int64() == 1 &&
                root.at(1).at(1).as_int64() == 3,
            "nested arrays should be readable");
    }

    // 12. 对象。
    {
        jotjson::document document =
            jotjson::parse_copy(
                "{\"name\":\"JotJSON\","
                "\"items\":[1,2,3],"
                "\"enabled\":true}");

        const jotjson::value& root =
            document.root();

        const jotjson::value* name =
            root.find("name");

        const jotjson::value* items =
            root.find("items");

        const jotjson::value* enabled =
            root.find("enabled");

        expect(
            root.is_object() && root.size() == 3,
            "object should have three members");

        expect(
            name != nullptr &&
                name->as_string() == "JotJSON",
            "object string member should be readable");

        expect(
            items != nullptr &&
                items->at(2).as_int64() == 3,
            "object array member should be readable");

        expect(
            enabled != nullptr &&
                enabled->as_bool(),
            "object boolean member should be readable");
    }

    // 13. 按索引读取对象成员。
    {
        jotjson::document document =
            jotjson::parse_copy(
                "{\"a\":1,\"b\":2}");

        std::string key;
        const jotjson::value* member = nullptr;

        expect(
            document.root().member_at(
                1,
                &key,
                &member),
            "member_at should find second member");

        expect(
            key == "b" &&
                member != nullptr &&
                member->as_int64() == 2,
            "member_at should return key and value");
    }

    // 14. 不存在的对象键。
    {
        jotjson::document document =
            jotjson::parse_copy("{\"a\":1}");

        expect(
            document.root().find("missing") == nullptr,
            "missing key should return nullptr");
    }

    // 15. 类型错误。
    {
        jotjson::document document =
            jotjson::parse_copy("[1,2]");

        bool caught = false;

        try {
            document.root().as_string();
        } catch (const jotjson::type_error&) {
            caught = true;
        }

        expect(
            caught,
            "wrong type access should throw type_error");
    }

    // 16. 数组越界。
    {
        jotjson::document document =
            jotjson::parse_copy("[1]");

        bool caught = false;

        try {
            document.root().at(1);
        } catch (const std::out_of_range&) {
            caught = true;
        }

        expect(
            caught,
            "array out-of-range should throw");
    }

    // 17. 基础语法错误。
    expect_parse_error(
        "",
        jotjson::error_code::parse,
        "empty input");

    expect_parse_error(
        "true false",
        jotjson::error_code::parse,
        "trailing characters");

    expect_parse_error(
        "tru",
        jotjson::error_code::parse,
        "truncated literal");

    // 18. 数字语法错误。
    expect_parse_error(
        "01",
        jotjson::error_code::parse,
        "leading zero");

    expect_parse_error(
        "1.",
        jotjson::error_code::parse,
        "missing fraction digits");

    expect_parse_error(
        "1e",
        jotjson::error_code::parse,
        "missing exponent digits");

    expect_parse_error(
        "--1",
        jotjson::error_code::parse,
        "double minus");

    // 19. 字符串语法错误。
    expect_parse_error(
        "\"unterminated",
        jotjson::error_code::parse,
        "unterminated string");

    expect_parse_error(
        "\"bad\\q\"",
        jotjson::error_code::parse,
        "invalid escape");

    expect_parse_error(
        "\"\\uD800\"",
        jotjson::error_code::parse,
        "unpaired high surrogate");

    expect_parse_error(
        "\"\\uDC00\"",
        jotjson::error_code::parse,
        "unpaired low surrogate");

    // 20. 非法 UTF-8。
    {
        const std::string invalid_utf8 =
            std::string("\"") +
            "\xC3\x28" +
            "\"";

        expect_parse_error(
            invalid_utf8,
            jotjson::error_code::invalid_utf8,
            "invalid UTF-8");
    }

    // 21. 容器语法错误。
    expect_parse_error(
        "[1,2,]",
        jotjson::error_code::parse,
        "array trailing comma");

    expect_parse_error(
        "{\"a\":1,}",
        jotjson::error_code::parse,
        "object trailing comma");

    expect_parse_error(
        "{\"a\":1,\"a\":2}",
        jotjson::error_code::parse,
        "duplicate object key");

    expect_parse_error(
        "{a:1}",
        jotjson::error_code::parse,
        "object key must be a string");

    // 22. 深度限制。
    {
        jotjson::parse_options options;
        options.max_depth = 2;

        bool caught = false;

        try {
            jotjson::parse_copy("[[[1]]]", options);
        } catch (const jotjson::parse_error& error) {
            caught =
                error.code() ==
                jotjson::error_code::depth_limit;
        }

        expect(
            caught,
            "deep nesting should hit depth limit");
    }

    // 23. 超出 int64 范围。
    {
        jotjson::document document =
            jotjson::parse_copy(
                "999999999999999999999999999");

        bool caught = false;

        try {
            document.root().as_int64();
        } catch (const std::out_of_range&) {
            caught = true;
        }

        expect(
            caught,
            "huge integer should fail int64 conversion");
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " JotJSON test(s) failed\n";

        return EXIT_FAILURE;
    }

    std::cout << "All JotJSON tests passed\n";
    return EXIT_SUCCESS;
}
