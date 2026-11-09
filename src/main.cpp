// [阶段 1 补全，阶段 2 扩展]
#include <jotjson/jotjson.hpp>

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

// 打印命令行帮助。
// 该函数只在当前源文件中使用，因此放在匿名命名空间。
void print_usage() {
    std::cout
        << jotjson::library_name()
        << " "
        << jotjson::version_string()
        << "\n"
        << "Usage:\n"
        << "  jotjson --version\n"
        << "  jotjson --help\n"
        << "  jotjson validate <file>\n"
        << "  jotjson validate-text <json>\n"
        << "  jotjson format [--indent N] <file>\n";
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

// [阶段 2 新增]
// 严格解析 --indent 后面的十进制整数。
//
// 这里不直接使用 atoi()，因为它会接受空文本、错误尾随字符，
// 还可能把超出范围的结果静默截断。
std::size_t parse_indent(const std::string& text) {
    if (text.empty()) {
        throw std::invalid_argument(
            "indent value cannot be empty");
    }

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] < '0' || text[i] > '9') {
            throw std::invalid_argument(
                "indent value must be a non-negative integer");
        }
    }

    errno = 0;
    char* end = nullptr;

    const unsigned long long parsed =
        std::strtoull(text.c_str(), &end, 10);

    if (errno == ERANGE ||
        end == text.c_str() ||
        *end != '\0') {
        throw std::out_of_range(
            "indent value is too large");
    }

    const std::size_t max_size =
        std::numeric_limits<std::size_t>::max();

    if (parsed >
        static_cast<unsigned long long>(max_size)) {
        throw std::out_of_range(
            "indent value is too large");
    }

    return static_cast<std::size_t>(parsed);
}

struct format_arguments {
    std::size_t indent;
    const char* path;

    format_arguments() noexcept
        : indent(2), path(nullptr) {}
};

// [阶段 2 新增]
// 解析下列三种写法：
//   format <file>
//   format --indent N <file>
//   format --indent=N <file>
//
// 选项和文件名的先后顺序不固定，但文件名只能出现一次。
format_arguments parse_format_arguments(
    int argc,
    char* argv[]) {
    format_arguments result;

    for (int i = 2; i < argc; ++i) {
        const std::string argument(argv[i]);

        if (argument == "--indent") {
            if (i + 1 >= argc) {
                throw std::invalid_argument(
                    "--indent requires a value");
            }

            ++i;
            result.indent = parse_indent(argv[i]);
            continue;
        }

        if (argument.size() >= 9 &&
            argument.compare(0, 9, "--indent=") == 0) {
            result.indent =
                parse_indent(argument.substr(9));
            continue;
        }

        if (!argument.empty() && argument[0] == '-') {
            throw std::invalid_argument(
                std::string("unknown format option: ") +
                argument);
        }

        if (result.path != nullptr) {
            throw std::invalid_argument(
                "format requires exactly one file");
        }

        result.path = argv[i];
    }

    if (result.path == nullptr) {
        throw std::invalid_argument(
            "format requires exactly one file");
    }

    return result;
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

    // [阶段 2 新增]
    // 读取文件、解析为 DOM，再将同一种 DOM 输出为美化 JSON。
    if (command == "format") {
        format_arguments arguments;

        try {
            arguments =
                parse_format_arguments(argc, argv);
        } catch (const std::exception& error) {
            std::cerr
                << error.what()
                << "\n";

            return 2;
        }

        try {
            const std::string input =
                read_file(arguments.path);

            const jotjson::document document =
                jotjson::parse_copy(input);

            const jotjson::serialize_options options(
                true,
                arguments.indent);

            const std::string output =
                jotjson::serialize_to_string(
                    document.root(),
                    options);

            // 格式化结果额外补一个换行，方便在终端和管道中使用。
            std::cout << output << "\n";

            if (!std::cout) {
                throw std::runtime_error(
                    "cannot write standard output");
            }

            return 0;
        } catch (const jotjson::json_error& error) {
            print_json_error(arguments.path, error);
            return 2;
        } catch (const std::exception& error) {
            std::cerr
                << arguments.path
                << ": "
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
