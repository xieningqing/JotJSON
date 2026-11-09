// [阶段 5 新增]
#ifndef JOTJSON_BENCH_COMMON_HPP
#define JOTJSON_BENCH_COMMON_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace jotjson_bench {

typedef std::chrono::steady_clock clock_type;

inline std::string read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        throw std::runtime_error(
            std::string("cannot open benchmark file: ") + path);
    }

    const std::string content(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    if (input.bad()) {
        throw std::runtime_error(
            std::string("cannot read benchmark file: ") + path);
    }

    return content;
}

inline std::size_t parse_iterations(const char* text) {
    if (text == nullptr || *text == '\0') {
        throw std::invalid_argument(
            "iteration count cannot be empty");
    }

    errno = 0;
    char* end = nullptr;

    const unsigned long long parsed =
        std::strtoull(text, &end, 10);

    if (errno == ERANGE ||
        end == text ||
        *end != '\0' ||
        parsed == 0) {
        throw std::invalid_argument(
            "iteration count must be a positive integer");
    }

    const std::size_t max_size =
        std::numeric_limits<std::size_t>::max();

    if (parsed >
        static_cast<unsigned long long>(max_size)) {
        throw std::out_of_range(
            "iteration count is too large");
    }

    return static_cast<std::size_t>(parsed);
}

inline double elapsed_seconds(
    const clock_type::time_point& start,
    const clock_type::time_point& finish) {
    return std::chrono::duration_cast<
        std::chrono::duration<double> >(
            finish - start).count();
}

inline double throughput_mb_per_second(
    std::size_t bytes,
    double seconds) {
    if (seconds <= 0.0) {
        return 0.0;
    }

    return static_cast<double>(bytes) /
        (1024.0 * 1024.0) /
        seconds;
}

inline void print_result(
    const char* benchmark,
    const char* path,
    std::size_t input_bytes,
    std::size_t iterations,
    double seconds) {
    if (iterations == 0 ||
        input_bytes >
            (std::numeric_limits<std::size_t>::max)() / iterations) {
        throw std::overflow_error(
            "benchmark byte count is too large");
    }

    const std::size_t total_bytes = input_bytes * iterations;
    const double throughput =
        throughput_mb_per_second(total_bytes, seconds);

    std::cout
        << "BENCH"
        << " name=" << benchmark
        << " file=" << path
        << " input_bytes=" << input_bytes
        << " total_bytes=" << total_bytes
        << " iterations=" << iterations
        << " total_seconds=" << seconds
        << " throughput_mb_s=" << throughput
        << "\n";
}

} // namespace jotjson_bench

#endif
