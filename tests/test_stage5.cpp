// [阶段 5 新增]
//
// 固定语料测试只关心“可用性”和“错误是否被稳定拒绝”：
// - 合法文件必须解析成功，并能完成序列化往返。
// - 非法文件必须抛出 parse_error，不能崩溃或返回半成品。
#include <jotjson/jotjson.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

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

bool read_file(
    const std::string& path,
    std::string& output) {
    std::ifstream input(path.c_str(), std::ios::binary);

    if (!input) {
        std::cerr
            << "FAIL: cannot open corpus file "
            << path
            << "\n";

        ++failures;
        return false;
    }

    const std::string content(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    if (input.bad()) {
        std::cerr
            << "FAIL: cannot read corpus file "
            << path
            << "\n";

        ++failures;
        return false;
    }

    output = content;
    return true;
}

void expect_valid_file(const std::string& path) {
    std::string input;

    if (!read_file(path, input)) {
        return;
    }

    try {
        const jotjson::document document =
            jotjson::parse_copy(input);

        const std::string encoded =
            jotjson::serialize_to_string(
                document.root());

        const jotjson::document reparsed =
            jotjson::parse_copy(encoded);

        (void)reparsed.root().type();

        expect(
            true,
            "valid corpus parses and round-trips: " + path);
    } catch (const std::exception& error) {
        std::cerr
            << "FAIL: valid corpus failed: "
            << path
            << ": "
            << error.what()
            << "\n";

        ++failures;
    }
}

void expect_invalid_file(const std::string& path) {
    std::string input;

    if (!read_file(path, input)) {
        return;
    }

    try {
        jotjson::parse_copy(input);

        std::cerr
            << "FAIL: invalid corpus was accepted: "
            << path
            << "\n";

        ++failures;
    } catch (const jotjson::parse_error& error) {
        const jotjson::error_location location =
            error.location();

        expect(
            location.byte_offset <= input.size() &&
                location.line >= 1 &&
                location.column >= 1,
            "invalid corpus rejected with a valid location: " +
                path);
    } catch (const std::exception& error) {
        std::cerr
            << "FAIL: invalid corpus raised wrong exception: "
            << path
            << ": "
            << error.what()
            << "\n";

        ++failures;
    }
}

} // namespace

int main() {
    // 安全边界：nullptr 且 size 大于 0 必须是参数错误。
    try {
        jotjson::parse_copy(nullptr, 1);
        expect(false, "null input with non-zero size is rejected");
    } catch (const std::invalid_argument&) {
        expect(true, "null input with non-zero size is rejected");
    } catch (const std::exception& error) {
        std::cerr
            << "FAIL: null input raised wrong exception: "
            << error.what()
            << "\n";

        ++failures;
    }

    // nullptr + 0 表示空输入，应按 JSON 语法错误处理。
    try {
        jotjson::parse_copy(nullptr, 0);
        expect(false, "null input with zero size is a parse error");
    } catch (const jotjson::parse_error&) {
        expect(true, "null input with zero size is a parse error");
    } catch (const std::exception& error) {
        std::cerr
            << "FAIL: empty null input raised wrong exception: "
            << error.what()
            << "\n";

        ++failures;
    }

    const char* valid_files[] = {
        "data/valid/empty_object.json",
        "data/valid/nested.json",
        "data/valid/unicode.json",
        "data/valid/numbers.json",
        "data/valid/large_object.json"
    };

    const char* invalid_files[] = {
        "data/invalid/trailing_garbage.json",
        "data/invalid/leading_zero.json",
        "data/invalid/bad_escape.json",
        "data/invalid/unterminated_string.json",
        "data/invalid/invalid_utf8.bin",
        "data/invalid/duplicate_key.json",
        "data/invalid/empty.json",
        "data/invalid/whitespace.json",
        "data/invalid/depth_limit.json"
    };

    for (std::size_t i = 0;
         i < sizeof(valid_files) / sizeof(valid_files[0]);
         ++i) {
        expect_valid_file(valid_files[i]);
    }

    for (std::size_t i = 0;
         i < sizeof(invalid_files) / sizeof(invalid_files[0]);
         ++i) {
        expect_invalid_file(invalid_files[i]);
    }

    if (failures != 0) {
        std::cerr
            << failures
            << " stage 5 corpus test(s) failed\n";

        return EXIT_FAILURE;
    }

    std::cout << "All stage 5 corpus tests passed\n";
    return EXIT_SUCCESS;
}
