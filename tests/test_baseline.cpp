// [阶段 4 新增]
//
// 这组测试建立阶段 4 的固定基线。它不修改 JSON 库的内部布局，
// 只通过公共接口解析语料，并记录解析期间的分配次数和峰值内存。
// 后续 Arena、字符串视图和数字缓存都应和这里的数据比较。
#include <jotjson/jotjson.hpp>

#include "allocation_probe.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

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

std::string read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        std::cerr
            << "FAIL: cannot open baseline file "
            << path
            << "\n";

        ++failures;
        return std::string();
    }

    const std::string content(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    if (input.bad()) {
        std::cerr
            << "FAIL: cannot read baseline file "
            << path
            << "\n";

        ++failures;
    }

    return content;
}

void print_measurement(
    const char* name,
    std::size_t input_bytes,
    const jotjson_test::allocation_snapshot& snapshot) {
    std::cout
        << "BASELINE "
        << name
        << " input_bytes="
        << input_bytes
        << " allocations="
        << snapshot.allocations
        << " deallocations="
        << snapshot.deallocations
        << " peak_allocations="
        << snapshot.peak_live_allocations
        << " allocated_bytes="
        << snapshot.allocated_bytes
        << " peak_bytes="
        << snapshot.peak_live_bytes
        << "\n";
}

void measure_small_object(bool probe_available) {
    const std::string input =
        read_file("data/baseline_small.json");

    if (input.empty()) {
        return;
    }

    jotjson_test::reset_allocation_probe();

    {
        const jotjson::document document =
            jotjson::parse_copy(input);

        const jotjson_test::allocation_snapshot parsed =
            jotjson_test::allocation_probe_snapshot();

        expect(
            document.root().is_object(),
            "baseline small input parses as object");

        expect(
            document.root().find("name") != nullptr &&
                document.root().find("name")->as_string() ==
                    "JotJSON",
            "baseline small object is readable through public API");

        if (probe_available) {
            expect(
                parsed.allocations > 0,
                "baseline small records parse allocations");
        }

        print_measurement(
            "small",
            input.size(),
            parsed);
    }

    const jotjson_test::allocation_snapshot released =
        jotjson_test::allocation_probe_snapshot();

    if (probe_available) {
        expect(
            released.deallocations > 0,
            "baseline small releases tracked allocations");
    }
}

void measure_medium_document(bool probe_available) {
    const std::string input =
        read_file("data/baseline_medium.json");

    if (input.empty()) {
        return;
    }

    jotjson_test::reset_allocation_probe();

    {
        const jotjson::document document =
            jotjson::parse_copy(input);

        const jotjson_test::allocation_snapshot parsed =
            jotjson_test::allocation_probe_snapshot();

        expect(
            document.root().is_object() &&
                document.root().size() == 4,
            "baseline medium document keeps its top-level shape");

        const jotjson::value* items =
            document.root().find("items");

        expect(
            items != nullptr &&
                items->is_array() &&
                items->size() == 32 &&
                items->at(31).as_int64() == 32,
            "baseline medium array keeps all elements");

        if (probe_available) {
            expect(
                parsed.allocations > 0,
                "baseline medium records parse allocations");
        }

        print_measurement(
            "medium",
            input.size(),
            parsed);
    }

    const jotjson_test::allocation_snapshot released =
        jotjson_test::allocation_probe_snapshot();

    if (probe_available) {
        expect(
            released.deallocations > 0,
            "baseline medium releases tracked allocations");
    }
}

void measure_large_object(bool probe_available) {
    const std::string input =
        read_file("data/baseline_large_object.json");

    if (input.empty()) {
        return;
    }

    jotjson_test::reset_allocation_probe();

    {
        const jotjson::document document =
            jotjson::parse_copy(input);

        const jotjson_test::allocation_snapshot parsed =
            jotjson_test::allocation_probe_snapshot();

        expect(
            document.root().is_object() &&
                document.root().size() == 32,
            "baseline large object keeps all members");

        const jotjson::value* last =
            document.root().find("key_31");

        expect(
            last != nullptr &&
                last->as_int64() == 31,
            "baseline large object can find its last member");

        if (probe_available) {
            expect(
                parsed.allocations > 0,
                "baseline large object records parse allocations");
        }

        print_measurement(
            "large_object",
            input.size(),
            parsed);
    }

    const jotjson_test::allocation_snapshot released =
        jotjson_test::allocation_probe_snapshot();

    if (probe_available) {
        expect(
            released.deallocations > 0,
            "baseline large object releases tracked allocations");
    }
}

void measure_escape_heavy_string(bool probe_available) {
    const std::string input =
        read_file("data/baseline_escape_heavy.json");

    if (input.empty()) {
        return;
    }

    jotjson_test::reset_allocation_probe();

    {
        const jotjson::document document =
            jotjson::parse_copy(input);

        const jotjson_test::allocation_snapshot parsed =
            jotjson_test::allocation_probe_snapshot();

        const jotjson::value* message =
            document.root().find("message");

        const std::string expected_prefix =
            "line\nquote\"slash\\tab\tunicode:";

        const std::string expected_unicode =
            "\xE4\xBD\xA0\xE5\xA5\xBD";

        expect(
            message != nullptr &&
                message->is_string() &&
                message->as_string().compare(
                    0,
                    expected_prefix.size(),
                    expected_prefix) == 0 &&
                message->as_string().find(expected_unicode) !=
                    std::string::npos,
            "baseline escape-heavy string is fully decoded");

        expect(
            message != nullptr &&
                message->as_string().find('\0') !=
                    std::string::npos,
            "baseline escape-heavy string preserves embedded NUL");

        if (probe_available) {
            expect(
                parsed.allocations > 0,
                "baseline escape-heavy records parse allocations");
        }

        print_measurement(
            "escape_heavy",
            input.size(),
            parsed);
    }

    const jotjson_test::allocation_snapshot released =
        jotjson_test::allocation_probe_snapshot();

    if (probe_available) {
        expect(
            released.deallocations > 0,
            "baseline string document releases tracked allocations");
    }
}

} // namespace

int main() {
    const bool probe_available =
        jotjson_test::probe_allocation_path();

    if (!probe_available) {
        std::cout
            << "Baseline allocation probe is not available; "
               "running correctness-only baseline\n";
    }

    measure_small_object(probe_available);
    measure_medium_document(probe_available);
    measure_large_object(probe_available);
    measure_escape_heavy_string(probe_available);

    if (failures != 0) {
        std::cerr
            << failures
            << " baseline test(s) failed\n";

        return EXIT_FAILURE;
    }

    std::cout << "All baseline tests passed\n";
    return EXIT_SUCCESS;
}
