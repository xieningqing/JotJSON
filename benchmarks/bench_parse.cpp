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
            << "usage: jotjson_bench_parse <file> [iterations]\n";

        return EXIT_FAILURE;
    }

    try {
        const std::string input =
            jotjson_bench::read_file(argv[1]);

        const std::size_t iterations =
            argc == 3
                ? jotjson_bench::parse_iterations(argv[2])
                : default_iterations;

        // 预热一次，避免首次文件读取和分配器初始化影响测量。
        {
            const jotjson::document warmup =
                jotjson::parse_copy(input);

            (void)warmup.root().type();
        }

        const jotjson_bench::clock_type::time_point start =
            jotjson_bench::clock_type::now();

        for (std::size_t i = 0;
             i < iterations;
             ++i) {
            const jotjson::document document =
                jotjson::parse_copy(input);

            (void)document.root().type();
        }

        const jotjson_bench::clock_type::time_point finish =
            jotjson_bench::clock_type::now();

        jotjson_bench::print_result(
            "parse",
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
