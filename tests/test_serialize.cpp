// [阶段 2 新增]
#include <jotjson/jotjson.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

// 记录一次布尔测试。
// 失败后继续执行，便于一次看到所有问题。
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

// 比较两个值树是否完全相同。
//
// 数字比较 number_lexeme()，而不是比较 double。
// 这样测试可以发现 1e+10 被改写成 10000000000 之类的问题。
bool values_equal(
    const jotjson::value& left,
    const jotjson::value& right,
    const std::string& path,
    std::string& error) {
    if (left.type() != right.type()) {
        error = path + ": different value types";
        return false;
    }

    switch (left.type()) {
    case jotjson::value_type::nil:
        return true;

    case jotjson::value_type::boolean:
        if (left.as_bool() != right.as_bool()) {
            error = path + ": different boolean values";
            return false;
        }
        return true;

    case jotjson::value_type::number:
        if (left.number_lexeme() !=
            right.number_lexeme()) {
            error = path + ": different number lexemes";
            return false;
        }
        return true;

    case jotjson::value_type::string:
        if (left.as_string() != right.as_string()) {
            error = path + ": different string values";
            return false;
        }
        return true;

    case jotjson::value_type::array: {
        if (left.size() != right.size()) {
            error = path + ": different array sizes";
            return false;
        }

        for (std::size_t i = 0; i < left.size(); ++i) {
            if (!values_equal(
                    left.at(i),
                    right.at(i),
                    path + "[" + std::to_string(i) + "]",
                    error)) {
                return false;
            }
        }

        return true;
    }

    case jotjson::value_type::object: {
        if (left.size() != right.size()) {
            error = path + ": different object sizes";
            return false;
        }

        // JSON 对象在语义上不依赖成员顺序，
        // 但本库承诺保留输入顺序，因此测试也检查顺序。
        for (std::size_t i = 0; i < left.size(); ++i) {
            std::string left_key;
            std::string right_key;
            const jotjson::value* left_member = nullptr;
            const jotjson::value* right_member = nullptr;

            const bool left_ok =
                left.member_at(
                    i,
                    &left_key,
                    &left_member);

            const bool right_ok =
                right.member_at(
                    i,
                    &right_key,
                    &right_member);

            if (!left_ok ||
                !right_ok ||
                left_member == nullptr ||
                right_member == nullptr) {
                error = path + ": invalid object member";
                return false;
            }

            if (left_key != right_key) {
                error = path + ": different object keys";
                return false;
            }

            if (!values_equal(
                    *left_member,
                    *right_member,
                    path + "." + left_key,
                    error)) {
                return false;
            }
        }

        return true;
    }
    }

    error = path + ": unknown value type";
    return false;
}

// 序列化后重新解析，再与原始 DOM 比较。
void expect_round_trip(
    const std::string& input,
    const jotjson::serialize_options& options,
    const std::string& name) {
    try {
        const jotjson::document original =
            jotjson::parse_copy(input);

        const std::string encoded =
            jotjson::serialize_to_string(
                original.root(),
                options);

        const jotjson::document decoded =
            jotjson::parse_copy(encoded);

        std::string error;

        const bool same =
            values_equal(
                original.root(),
                decoded.root(),
                "root",
                error);

        expect(
            same,
            name + (same ? "" : ": " + error));
    } catch (const std::exception& error) {
        std::cerr
            << "FAIL: "
            << name
            << ": "
            << error.what()
            << "\n";

        ++failures;
    }
}

// 用自定义 writer 收集输出，验证公共 writer 接口可用。
class collecting_writer final : public jotjson::writer {
public:
    void write(
        const char* data,
        std::size_t size) override {
        if (size != 0) {
            if (data == nullptr) {
                throw std::invalid_argument(
                    "collecting_writer got null data");
            }

            output_.append(data, size);
        }
    }

    const std::string& output() const noexcept {
        return output_;
    }

private:
    std::string output_;
};

// 每次写入都失败，用于验证异常不会被 serializer 吞掉。
class failing_writer final : public jotjson::writer {
public:
    void write(
        const char*,
        std::size_t) override {
        throw std::runtime_error("write failed");
    }
};

} // namespace

int main() {
    // 1. 紧凑格式。
    {
        const jotjson::document document =
            jotjson::parse_copy(
                " { \"a\" : 1, \"b\" : [true,null,\"x\"] } ");

        expect(
            jotjson::serialize_to_string(
                document.root()) ==
                "{\"a\":1,\"b\":[true,null,\"x\"]}",
            "compact serialization");
    }

    // 2. 默认两空格美化格式。
    {
        const jotjson::document document =
            jotjson::parse_copy(
                "{\"a\":1,\"b\":[true,null,\"x\"]}");

        const std::string expected =
            "{\n"
            "  \"a\": 1,\n"
            "  \"b\": [\n"
            "    true,\n"
            "    null,\n"
            "    \"x\"\n"
            "  ]\n"
            "}";

        expect(
            jotjson::serialize_to_string(
                document.root(),
                jotjson::serialize_options(true, 2)) ==
                expected,
            "pretty serialization with two-space indent");
    }

    // 3. 四空格缩进。
    {
        const jotjson::document document =
            jotjson::parse_copy(
                "{\"outer\":{\"inner\":[1]}}");

        const std::string expected =
            "{\n"
            "    \"outer\": {\n"
            "        \"inner\": [\n"
            "            1\n"
            "        ]\n"
            "    }\n"
            "}";

        expect(
            jotjson::serialize_to_string(
                document.root(),
                jotjson::serialize_options(true, 4)) ==
                expected,
            "pretty serialization with four-space indent");
    }

    // 4. 空数组和空对象。
    {
        const jotjson::document array_document =
            jotjson::parse_copy("[]");

        const jotjson::document object_document =
            jotjson::parse_copy("{}");

        expect(
            jotjson::serialize_to_string(
                array_document.root()) == "[]",
            "empty array should stay compact");

        expect(
            jotjson::serialize_to_string(
                object_document.root()) == "{}",
            "empty object should stay compact");

        expect(
            jotjson::serialize_to_string(
                array_document.root(),
                jotjson::serialize_options(true, 4)) == "[]",
            "pretty empty array should not add newlines");

        expect(
            jotjson::serialize_to_string(
                object_document.root(),
                jotjson::serialize_options(true, 4)) == "{}",
            "pretty empty object should not add newlines");
    }

    // 5. 数字原始文本必须原样输出。
    {
        const std::string input =
            "[-0,1e+10,1.2300,"
            "9007199254740993,0.000001]";

        const jotjson::document document =
            jotjson::parse_copy(input);

        expect(
            jotjson::serialize_to_string(
                document.root()) == input,
            "number lexemes should be preserved");
    }

    // 6. 引号、反斜杠和控制字符。
    {
        const std::string input =
            "\"quote\\\" slash\\\\ backspace\\b "
            "formfeed\\f newline\\n return\\r tab\\t "
            "control\\u0001\"";

        const jotjson::document document =
            jotjson::parse_copy(input);

        expect(
            jotjson::serialize_to_string(
                document.root()) == input,
            "quotes, slashes and controls should escape");
    }

    // 7. 嵌入 NUL 必须输出为 \u0000。
    {
        const jotjson::document document =
            jotjson::parse_copy("\"\\u0000\"");

        expect(
            jotjson::serialize_to_string(
                document.root()) == "\"\\u0000\"",
            "embedded NUL should use a unicode escape");
    }

    // 8. 正斜杠默认不需要转义。
    {
        const jotjson::document document =
            jotjson::parse_copy("\"a\\/b\"");

        expect(
            jotjson::serialize_to_string(
                document.root()) == "\"a/b\"",
            "forward slash should not be escaped by default");
    }

    // 9. Unicode 应输出为 UTF-8，而不是强制写成 \uXXXX。
    {
        const jotjson::document document =
            jotjson::parse_copy("\"\\u4F60\"");

        const std::string expected =
            "\"" "\xE4\xBD\xA0" "\"";

        expect(
            jotjson::serialize_to_string(
                document.root()) == expected,
            "unicode should be emitted as UTF-8");
    }

    // 10. 对象键也需要转义。
    {
        const jotjson::document document =
            jotjson::parse_copy(
                "{\"line\\nkey\":1}");

        expect(
            jotjson::serialize_to_string(
                document.root()) ==
                "{\"line\\nkey\":1}",
            "object keys should be escaped");
    }

    // 11. 复杂文档往返。
    {
        const std::string input =
            R"json({
                "name": "JotJSON",
                "version": 2,
                "ok": true,
                "nothing": null,
                "items": [1, 2.5, -0, 1e+10, "text", [], {}],
                "nested": {
                    "text": "\u4F60\u597D",
                    "emoji": "\uD83D\uDE00"
                },
                "empty": []
            })json";

        expect_round_trip(
            input,
            jotjson::serialize_options(false, 0),
            "complex compact round trip");

        expect_round_trip(
            input,
            jotjson::serialize_options(true, 2),
            "complex pretty round trip");
    }

    // 12. 自定义 writer。
    {
        const jotjson::document document =
            jotjson::parse_copy(
                "{\"a\":[1,2],\"b\":\"x\"}");

        collecting_writer output;

        jotjson::serialize_pretty(
            document.root(),
            output,
            2);

        // 把 writer 收集到的文本重新解析，
        // 验证 serializer 产生的是完整合法 JSON。
        const jotjson::document reparsed =
            jotjson::parse_copy(output.output());

        expect(
            reparsed.root().find("a") != nullptr,
            "custom writer output should be valid JSON");
    }

    // 13. writer 写入失败必须向调用方传播。
    {
        const jotjson::document document =
            jotjson::parse_copy("[1,2,3]");

        failing_writer output;
        bool caught = false;

        try {
            jotjson::serialize_compact(
                document.root(),
                output);
        } catch (const std::runtime_error& error) {
            caught =
                std::string(error.what()) ==
                "write failed";
        }

        expect(
            caught,
            "writer exceptions should propagate");
    }

    // 14. string_writer 的 clear() 和 take_string()。
    {
        const jotjson::document document =
            jotjson::parse_copy("{\"a\":1}");

        jotjson::string_writer output;

        jotjson::serialize_compact(
            document.root(),
            output);

        expect(
            output.str() == "{\"a\":1}",
            "string_writer should collect output");

        output.clear();

        expect(
            output.str().empty(),
            "string_writer clear should remove old output");

        jotjson::serialize_compact(
            document.root(),
            output);

        const std::string taken =
            output.take_string();

        expect(
            taken == "{\"a\":1}" &&
                output.str().empty(),
            "string_writer take_string should move output");
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " serializer test(s) failed\n";

        return EXIT_FAILURE;
    }

    std::cout << "All serializer tests passed\n";
    return EXIT_SUCCESS;
}
