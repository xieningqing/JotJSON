// [阶段 2 新增]
#include <jotjson/jotjson.hpp>

#include <exception>
#include <iostream>
#include <string>

int main() {
    // 原始 JSON 中包含：
    // - Unicode 转义；
    // - 换行和双引号转义；
    // - 带指数的数字和末尾带零的小数；
    // - 空对象。
    const std::string input =
        R"json({"name":"JotJSON","unicode":"\u4F60\u597D","message":"line\n\"quote\"","numbers":[1e+10,1.2300],"empty":{}})json";

    try {
        const jotjson::document document =
            jotjson::parse_copy(input);

        std::cout
            << "1. Original JSON\n"
            << input
            << "\n\n";

        // 默认 serialize_to_string() 使用紧凑格式。
        const std::string compact =
            jotjson::serialize_to_string(
                document.root());

        std::cout
            << "2. Compact output\n"
            << compact
            << "\n\n";

        // pretty 为 true 时使用可读的换行和缩进。
        const jotjson::serialize_options pretty_options(
            true,
            2);

        const std::string pretty =
            jotjson::serialize_to_string(
                document.root(),
                pretty_options);

        std::cout
            << "3. Pretty output\n"
            << pretty
            << "\n\n";

        // 再次解析序列化结果，证明输出仍然是合法 JSON。
        jotjson::parse_copy(compact);
        jotjson::parse_copy(pretty);

        std::cout
            << "4. Both outputs can be parsed again.\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr
            << "serialization example failed: "
            << error.what()
            << "\n";

        return 1;
    }
}
