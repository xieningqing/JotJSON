// [阶段 1 补全]
#include <jotjson/jotjson.hpp>

#include <exception>
#include <iostream>
#include <string>

int main() {
    // 这份输入同时包含对象、字符串、数字和数组。
    const std::string input =
        "{\"name\":\"JotJSON\",\"version\":1,"
        "\"features\":[\"parse\",\"unicode\"]}";

    try {
        // parse_copy() 会复制输入，并返回拥有整棵 DOM 的 document。
        jotjson::document document =
            jotjson::parse_copy(input);

        const jotjson::value& root =
            document.root();

        // find() 查找不到时返回 nullptr，使用前必须检查。
        const jotjson::value* name =
            root.find("name");

        const jotjson::value* version =
            root.find("version");

        if (name == nullptr || version == nullptr) {
            std::cerr
                << "required fields are missing\n";

            return 1;
        }

        std::cout
            << name->as_string()
            << " version "
            << version->as_int64()
            << "\n";

        return 0;
    } catch (const std::exception& error) {
        // 库内部不吞掉错误，示例程序在 main 边界统一处理。
        std::cerr
            << "parse failed: "
            << error.what()
            << "\n";

        return 1;
    }
}
