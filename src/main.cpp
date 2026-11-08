// [阶段 1 补全]
#include <jotjson/jotjson.hpp>

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

// 打印命令行帮助。
// 该函数只在当前源文件中使用，因此放在匿名命名空间。
void print_usage() {
    std::cout
        << "JotJSON 0.1.0\n"
        << "Usage:\n"
        << "  jotjson --version\n"
        << "  jotjson --help\n"
        << "  jotjson validate <file>\n"
        << "  jotjson validate-text <json>\n";
}

// 以二进制方式读取整个文件。
//
// 这里不依赖文件末尾的 '\0'，因为解析接口同时接收内容指针和长度。
std::string read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        throw std::runtime_error(
            std::string("cannot open file: ") + path);
    }

    std::string content(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    if (input.bad()) {
        throw std::runtime_error(
            std::string("cannot read file: ") + path);
    }

    return content;
}

// 把 JSON 错误打印成“文件:行:列: 说明”的格式。
void print_json_error(
    const char* path,
    const jotjson::json_error& error) {
    const jotjson::error_location location =
        error.location();

    std::cerr
        << path
        << ":"
        << location.line
        << ":"
        << location.column
        << ": "
        << error.what()
        << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 1) {
        print_usage();
        return 0;
    }

    const std::string command(argv[1]);

    if (command == "--version") {
        std::cout
            << jotjson::library_name()
            << " "
            << jotjson::version_string()
            << "\n";

        return 0;
    }

    if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }

    if (command == "validate") {
        if (argc != 3) {
            std::cerr
                << "validate requires exactly one file\n";

            return 2;
        }

        try {
            const std::string input =
                read_file(argv[2]);

            // 阶段 1 只检查输入能否成功解析。
            // document 离开作用域时会自动释放整棵 DOM。
            jotjson::parse_copy(input);

            std::cout
                << argv[2]
                << ": valid JSON\n";

            return 0;
        } catch (const jotjson::json_error& error) {
            print_json_error(argv[2], error);
            return 2;
        } catch (const std::exception& error) {
            std::cerr
                << argv[2]
                << ": "
                << error.what()
                << "\n";

            return 2;
        }
    }

    if (command == "validate-text") {
        if (argc != 3) {
            std::cerr
                << "validate-text requires exactly one JSON argument\n";

            return 2;
        }

        try {
            // 不经过文件读取，直接验证命令行中的 JSON 文本。
            jotjson::parse_copy(argv[2]);

            std::cout << "valid JSON\n";
            return 0;
        } catch (const jotjson::json_error& error) {
            const jotjson::error_location location =
                error.location();

            std::cerr
                << "<command line>:"
                << location.line
                << ":"
                << location.column
                << ": "
                << error.what()
                << "\n";

            return 2;
        } catch (const std::exception& error) {
            std::cerr
                << "<command line>: "
                << error.what()
                << "\n";

            return 2;
        }
    }

    std::cerr
        << "Unknown command: "
        << command
        << "\n";

    return 1;
}
