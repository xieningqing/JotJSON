// [阶段 5 新增]
//
// 两种运行方式：
// 1. 普通构建：编译成可执行程序，传入单个文件或语料目录。
// 2. libFuzzer 构建：定义 JOTJSON_USE_LIBFUZZER，
//    由 fuzzer 运行时反复调用 LLVMFuzzerTestOneInput。
#include <jotjson/jotjson.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#if defined(JOTJSON_USE_LIBFUZZER)

extern "C" int LLVMFuzzerTestOneInput(
    const unsigned char* data,
    std::size_t size) {
    const char* input =
        reinterpret_cast<const char*>(data);

    try {
        const jotjson::document document =
            jotjson::parse_copy(input, size);

        (void)jotjson::serialize_to_string(
            document.root());
    } catch (const jotjson::parse_error&) {
        // 语法错误是预期结果，不是 fuzzer 缺陷。
    }

    return 0;
}

#else

#include <fstream>
#include <iterator>

namespace {

bool run_one(const char* path) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        std::cerr
            << "cannot open "
            << path
            << "\n";

        return false;
    }

    const std::string bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    try {
        const jotjson::document document =
            jotjson::parse_copy(
                bytes.data(),
                bytes.size());

        (void)jotjson::serialize_to_string(
            document.root());
    } catch (const jotjson::parse_error&) {
        // 非法输入被拒绝是正常路径。
    }

    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: fuzz_parse <file>\n";
        return EXIT_FAILURE;
    }

    try {
        if (!run_one(argv[1])) {
            return EXIT_FAILURE;
        }
    } catch (const std::exception& error) {
        std::cerr
            << "fuzz input failed unexpectedly: "
            << error.what()
            << "\n";

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#endif
