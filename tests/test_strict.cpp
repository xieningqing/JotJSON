// [阶段 3 新增]
//
// 这组测试关注“严格语义与安全边界”，而不是普通成功路径。
// 每个测试点都尽量同时检查错误码和位置，因为只检查“抛异常了”
// 无法发现错误位置漂移、诊断信息变差等问题。
#include <jotjson/jotjson.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

// 失败后继续运行，便于一次看到所有边界问题。
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

// 检查解析失败时的错误码、字节偏移、行号和列号。
//
// byte_offset 从 0 开始；
// line 和 column 从 1 开始。
void expect_parse_error_at(
    const std::string& input,
    jotjson::error_code expected_code,
    std::size_t expected_offset,
    std::size_t expected_line,
    std::size_t expected_column,
    const std::string& message) {
    try {
        jotjson::parse_copy(input);

        std::cerr
            << "FAIL: "
            << message
            << ": expected parse failure\n";

        ++failures;
    } catch (const jotjson::parse_error& error) {
        const jotjson::error_location location =
            error.location();

        const bool matches =
            error.code() == expected_code &&
            location.byte_offset == expected_offset &&
            location.line == expected_line &&
            location.column == expected_column;

        expect(matches, message);

        if (!matches) {
            std::cerr
                << "  expected offset="
                << expected_offset
                << " line="
                << expected_line
                << " column="
                << expected_column
                << ", got offset="
                << location.byte_offset
                << " line="
                << location.line
                << " column="
                << location.column
                << "\n";
        }
    } catch (const std::exception& error) {
        std::cerr
            << "FAIL: "
            << message
            << ": wrong exception: "
            << error.what()
            << "\n";

        ++failures;
    }
}

// 对于错误文本和错误位置都依赖实现细节的用例，
// 只检查稳定错误码和位置是否在输入范围内。
void expect_parse_error_code(
    const std::string& input,
    jotjson::error_code expected_code,
    const std::string& message) {
    try {
        jotjson::parse_copy(input);

        std::cerr
            << "FAIL: "
            << message
            << ": expected parse failure\n";

        ++failures;
    } catch (const jotjson::parse_error& error) {
        const jotjson::error_location location =
            error.location();

        const bool matches =
            error.code() == expected_code &&
            location.byte_offset <= input.size() &&
            location.line >= 1 &&
            location.column >= 1;

        expect(matches, message);
    } catch (const std::exception& error) {
        std::cerr
            << "FAIL: "
            << message
            << ": wrong exception: "
            << error.what()
            << "\n";

        ++failures;
    }
}

void expect_valid_utf8(
    const char* bytes,
    const std::string& message) {
    const std::string expected(bytes);

    try {
        // JSON 字符串中的非 ASCII 字节必须直接构成合法 UTF-8。
        const jotjson::document document =
            jotjson::parse_copy("\"" + expected + "\"");

        expect(
            document.root().as_string() == expected,
            message);
    } catch (const std::exception& error) {
        std::cerr
            << "FAIL: "
            << message
            << ": "
            << error.what()
            << "\n";

        ++failures;
    }
}

void expect_invalid_utf8(
    const char* bytes,
    const std::string& message) {
    const std::string input =
        "\"" + std::string(bytes) + "\"";

    // 非法序列的错误位置应指向序列起点：
    // 字节偏移 1，第 1 行第 2 列。
    expect_parse_error_at(
        input,
        jotjson::error_code::invalid_utf8,
        1,
        1,
        2,
        message);
}

void expect_int64_throws(
    const std::string& input,
    const std::string& message) {
    try {
        const jotjson::document document =
            jotjson::parse_copy(input);

        (void)document.root().as_int64();

        std::cerr
            << "FAIL: "
            << message
            << ": expected out_of_range\n";

        ++failures;
    } catch (const std::out_of_range&) {
        expect(true, message);
    } catch (const std::exception& error) {
        std::cerr
            << "FAIL: "
            << message
            << ": wrong exception: "
            << error.what()
            << "\n";

        ++failures;
    }
}

void expect_double_throws(
    const std::string& input,
    const std::string& message) {
    try {
        const jotjson::document document =
            jotjson::parse_copy(input);

        (void)document.root().as_double();

        std::cerr
            << "FAIL: "
            << message
            << ": expected out_of_range\n";

        ++failures;
    } catch (const std::out_of_range&) {
        expect(true, message);
    } catch (const std::exception& error) {
        std::cerr
            << "FAIL: "
            << message
            << ": wrong exception: "
            << error.what()
            << "\n";

        ++failures;
    }
}

// 自定义 numpunct，模拟“逗号才是小数点”的区域设置。
//
// std::locale::classic() 应该不受这个设置影响，
// 因此 JSON 的 1.25 仍必须按点号解析。
class comma_decimal_point final : public std::numpunct<char> {
protected:
    char do_decimal_point() const override {
        return ',';
    }
};

// RAII：构造时切换全局 locale，析构时自动恢复。
// 这样即使测试中途抛异常，也不会污染后面的测试。
class locale_guard {
public:
    explicit locale_guard(const std::locale& value)
        : previous_(std::locale::global(value)) {}

    ~locale_guard() {
        std::locale::global(previous_);
    }

private:
    std::locale previous_;

    locale_guard(const locale_guard&) = delete;
    locale_guard& operator=(const locale_guard&) = delete;
};

} // namespace

int main() {
    // 1. UTF-8 合法边界。
    //
    // 这些字节覆盖了 2、3、4 字节序列的最小值和最大值。
    // 解码后的字符串必须原样保存，不能错误地转成其他字节。
    {
        expect_valid_utf8(
            "\xC2\x80",
            "UTF-8 accepts U+0080");

        expect_valid_utf8(
            "\xDF\xBF",
            "UTF-8 accepts U+07FF");

        expect_valid_utf8(
            "\xE0\xA0\x80",
            "UTF-8 accepts U+0800");

        expect_valid_utf8(
            "\xED\x9F\xBF",
            "UTF-8 accepts U+D7FF");

        expect_valid_utf8(
            "\xEE\x80\x80",
            "UTF-8 accepts U+E000");

        expect_valid_utf8(
            "\xEF\xBF\xBF",
            "UTF-8 accepts U+FFFF");

        expect_valid_utf8(
            "\xF0\x90\x80\x80",
            "UTF-8 accepts U+10000");

        expect_valid_utf8(
            "\xF4\x8F\xBF\xBF",
            "UTF-8 accepts U+10FFFF");
    }

    // 2. UTF-8 非法边界。
    //
    // C0/C1 首字节、代理项编码、超过 U+10FFFF 和过长编码
    // 都属于“看起来像 Unicode，但 RFC 8259 不允许”的输入。
    {
        expect_invalid_utf8(
            "\x80",
            "UTF-8 rejects stray continuation byte");

        expect_invalid_utf8(
            "\xC0\xAF",
            "UTF-8 rejects overlong two-byte sequence");

        expect_invalid_utf8(
            "\xC2",
            "UTF-8 rejects truncated two-byte sequence");

        expect_invalid_utf8(
            "\xE0\x80\xAF",
            "UTF-8 rejects overlong three-byte sequence");

        expect_invalid_utf8(
            "\xE2\x28\xA1",
            "UTF-8 rejects invalid continuation byte");

        expect_invalid_utf8(
            "\xED\xA0\x80",
            "UTF-8 rejects encoded surrogate");

        expect_invalid_utf8(
            "\xF0\x8F\xBF\xBF",
            "UTF-8 rejects overlong four-byte sequence");

        expect_invalid_utf8(
            "\xF4\x90\x80\x80",
            "UTF-8 rejects code point above U+10FFFF");

        expect_invalid_utf8(
            "\xF5\x80\x80\x80",
            "UTF-8 rejects invalid leading byte");
    }

    // 3. 未转义控制字符。
    //
    // JSON 字符串中的 0x00 到 0x1F 必须写成转义，
    // 例如换行要写成 \n，不能直接放一个换行字节。
    for (int value = 0; value < 0x20; ++value) {
        std::string input("\"");
        input.push_back(static_cast<char>(value));
        input.push_back('"');

        expect_parse_error_at(
            input,
            jotjson::error_code::parse,
            1,
            1,
            2,
            "unescaped control byte "
                + std::to_string(value));
    }

    // 4. 代理对边界。
    //
    // D800/DC00 是合法最小值，DBFF/DFFF 是合法最大值。
    // 两个 \u 转义合起来应编码成 4 字节 UTF-8。
    {
        const jotjson::document document =
            jotjson::parse_copy("\"\\uD800\\uDC00\"");

        expect(
            document.root().as_string() ==
                "\xF0\x90\x80\x80",
            "minimum surrogate pair becomes UTF-8");
    }

    {
        const jotjson::document document =
            jotjson::parse_copy("\"\\uDBFF\\uDFFF\"");

        expect(
            document.root().as_string() ==
                "\xF4\x8F\xBF\xBF",
            "maximum surrogate pair becomes UTF-8");
    }

    expect_parse_error_code(
        "\"\\uD800\"",
        jotjson::error_code::parse,
        "unpaired high surrogate is rejected");

    expect_parse_error_code(
        "\"\\uDFFF\"",
        jotjson::error_code::parse,
        "unpaired low surrogate is rejected");

    expect_parse_error_code(
        "\"\\uD800\\uD800\"",
        jotjson::error_code::parse,
        "two high surrogates are rejected");

    expect_parse_error_code(
        "\"\\uD800x\"",
        jotjson::error_code::parse,
        "high surrogate followed by text is rejected");

    // 5. 重复键。
    //
    // 第二个键 "a" 从字节偏移 7 开始，所以报告列为 8。
    // 错误应指向重复键的引号，而不是它的值。
    expect_parse_error_at(
        "{\"a\":1,\"a\":2}",
        jotjson::error_code::parse,
        7,
        1,
        8,
        "duplicate plain object key is rejected");

    // \u0061 解码后仍然是 "a"，属于同一键。
    expect_parse_error_at(
        "{\"a\":1,\"\\u0061\":2}",
        jotjson::error_code::parse,
        7,
        1,
        8,
        "duplicate key after escape decoding is rejected");

    // 嵌入 NUL 的键也不能被 c_str() 截断。
    expect_parse_error_code(
        "{\"\\u0000\":1,\"\\u0000\":2}",
        jotjson::error_code::parse,
        "duplicate key containing NUL is rejected");

    expect_parse_error_code(
        "{\"\\/\":1,\"/\":2}",
        jotjson::error_code::parse,
        "duplicate key after slash escape is rejected");

    // 6. 对象键可以包含解码后的 NUL。
    {
        const jotjson::document document =
            jotjson::parse_copy("{\"\\u0000\":1}");

        // std::string(ptr, size) 明确保留一个 NUL 字节，
        // 不能写成 std::string("\0")，那样会得到空字符串。
        expect(
            document.root().find(std::string("\0", 1)) != nullptr,
            "object key may contain decoded NUL");
    }

    // 7. BOM 策略。
    //
    // 默认拒绝，错误必须指向第 0 个字节。
    // 显式允许后，BOM 只被跳过一次。
    {
        const std::string bom = "\xEF\xBB\xBF";

        expect_parse_error_at(
            bom + "{}",
            jotjson::error_code::parse,
            0,
            1,
            1,
            "UTF-8 BOM is rejected by default");

        jotjson::parse_options options;
        options.allow_utf8_bom = true;

        const jotjson::document document =
            jotjson::parse_copy(
                bom + "\n[1]",
                options);

        expect(
            document.root().at(0).as_int64() == 1,
            "UTF-8 BOM can be explicitly allowed");
    }

    // 8. 空输入、纯空白和尾随垃圾。
    //
    // 纯空白在跳过空格后到达输入末尾，错误偏移应为 3。
    expect_parse_error_at(
        "",
        jotjson::error_code::parse,
        0,
        1,
        1,
        "empty input is rejected");

    expect_parse_error_at(
        " \n\t",
        jotjson::error_code::parse,
        3,
        2,
        2,
        "whitespace-only input is rejected");

    expect_parse_error_at(
        "null true",
        jotjson::error_code::parse,
        5,
        1,
        6,
        "trailing garbage is rejected");

    // 9. 深度限制。
    //
    // 根节点为第 0 层，数组元素为第 1 层。
    // max_depth == 2 时，[[1]] 合法，[[[1]]] 超限。
    {
        jotjson::parse_options options;
        options.max_depth = 2;

        const jotjson::document document =
            jotjson::parse_copy("[[1]]", options);

        expect(
            document.root().at(0).at(0).as_int64() == 1,
            "depth limit boundary is accepted");

        bool depth_limited = false;

        try {
            jotjson::parse_copy("[[[1]]]", options);
        } catch (const jotjson::parse_error& error) {
            depth_limited =
                error.code() ==
                jotjson::error_code::depth_limit;
        }

        expect(
            depth_limited,
            "depth limit rejects one level beyond boundary");
    }

    {
        jotjson::parse_options options;
        options.max_depth = 0;

        bool invalid_argument = false;

        try {
            jotjson::parse_copy("null", options);
        } catch (const std::invalid_argument&) {
            invalid_argument = true;
        }

        expect(
            invalid_argument,
            "max_depth zero is an invalid argument");
    }

    // 10. int64 上下界。
    //
    // 9223372036854775807 是 INT64_MAX。
    // -9223372036854775808 是 INT64_MIN，不能走“先转正数”的路径。
    {
        const jotjson::document document =
            jotjson::parse_copy("9223372036854775807");

        expect(
            document.root().as_int64() ==
                std::numeric_limits<std::int64_t>::max(),
            "INT64_MAX is accepted");
    }

    {
        const jotjson::document document =
            jotjson::parse_copy("-9223372036854775808");

        expect(
            document.root().as_int64() ==
                std::numeric_limits<std::int64_t>::min(),
            "INT64_MIN is accepted");
    }

    expect_int64_throws(
        "9223372036854775808",
        "INT64_MAX plus one is rejected");

    expect_int64_throws(
        "-9223372036854775809",
        "INT64_MIN minus one is rejected");

    expect_int64_throws(
        "1.0",
        "fraction is rejected by as_int64");

    expect_int64_throws(
        "1e2",
        "exponent is rejected by as_int64");

    {
        const jotjson::document document =
            jotjson::parse_copy("-0");

        expect(
            document.root().as_int64() == 0,
            "negative zero converts to zero");
    }

    // 11. double 基本值和溢出。
    //
    // 1e400 在 JSON 词法上是合法数字，
    // 但转换成 double 会溢出，因此访问器应抛 out_of_range。
    {
        const jotjson::document document =
            jotjson::parse_copy("1.25");

        expect(
            document.root().as_double() == 1.25,
            "as_double reads decimal fraction");
    }

    {
        const jotjson::document document =
            jotjson::parse_copy("1e+10");

        expect(
            document.root().as_double() == 10000000000.0,
            "as_double reads exponent notation");
    }

    expect_double_throws(
        "1e400",
        "double overflow is rejected");

    // 12. 区域设置无关性。
    //
    // 这个测试会在本进程临时把小数点改成逗号。
    // parse_double_lexeme 使用 classic locale，因此仍应把 1.25 读对。
    {
        const std::locale comma_locale(
            std::locale::classic(),
            new comma_decimal_point);

        const locale_guard guard(comma_locale);

        expect(
            std::use_facet<std::numpunct<char> >(
                std::locale()).decimal_point() == ',',
            "comma decimal point locale is active");

        const jotjson::document document =
            jotjson::parse_copy("1.25");

        expect(
            document.root().as_double() == 1.25,
            "as_double is independent of process locale");
    }

    // 13. 列号按 Unicode 码点计算，而不是按 UTF-8 字节数计算。
    //
    // 输入是 "你"x：
    // 字节偏移：0 是引号，1..3 是“你”，4 是结束引号，5 是 x。
    // 可见列：1 是引号，2 是“你”，3 是结束引号，4 是 x。
    {
        const std::string input =
            std::string("\"")
            + "\xE4\xBD\xA0"
            + "\"x";

        expect_parse_error_at(
            input,
            jotjson::error_code::parse,
            5,
            1,
            4,
            "column counts code points, not bytes");
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " strict semantics test(s) failed\n";

        return EXIT_FAILURE;
    }

    std::cout << "All strict semantics tests passed\n";
    return EXIT_SUCCESS;
}
