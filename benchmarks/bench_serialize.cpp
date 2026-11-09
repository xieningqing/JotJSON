// [阶段 5 新增]
#include <jotjson/jotjson.hpp>

#include "bench_common.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

const std::size_t default_iterations = 2000;

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 3) {
        std::cerr
            << "usage: jotjson_bench_serialize <file> [iterations]\n";

        return EXIT_FAILURE;
    }

    try {
        const std::string input =
            jotjson_bench::read_file(argv[1]);

        const std::size_t iterations =
            argc == 3
                ? jotjson_bench::parse_iterations(argv[2])
                : default_iterations;

        // DOM 构建不计入序列化时间。
        const jotjson::document document =
            jotjson::parse_copy(input);

        (void)jotjson::serialize_to_string(
            document.root());

        const jotjson_bench::clock_type::time_point start =
            jotjson_bench::clock_type::now();

        for (std::size_t i = 0;
             i < iterations;
             ++i) {
            const std::string output =
                jotjson::serialize_to_string(
                    document.root());

            if (output.empty()) {
                throw std::runtime_error(
                    "serializer returned empty output");
            }
        }

        const jotjson_bench::clock_type::time_point finish =
            jotjson_bench::clock_type::now();

        jotjson_bench::print_result(
            "serialize",
            argv[1],
            input.size(),
            iterations,
            jotjson_bench::elapsed_seconds(start, finish));
    } catch (const std::exception& error) {
        std::cerr
            << "benchmark failed: "
            << error.what()
            << "\n";

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
