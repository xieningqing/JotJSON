#ifndef JOTJSON_ERROR_HPP
#define JOTJSON_ERROR_HPP

#include<cstddef>  
#include<string>
#include <stdexcept>

namespace jotjson{
    enum class error_code{
        invalid_argument, // 传入的参数无效
        parse,  // 解析错误
        type_mismatch,  // 类型不匹配
        out_of_range,  // 越界访问
        invalid_utf8,  // 无效的 UTF-8 编码
        depth_limit, // 超过最大嵌套深度限制
        out_of_memory, // 内存不足
        io  // I/O 错误
    };


// 记录错误在输入文本中的位置。
//
// byte_offset 从 0 开始；
// line 和 column 从 1 开始，方便直接显示给用户。
struct error_location {
    std::size_t byte_offset; // 字节偏移量
    std::size_t line;        // 行号，从 1 开始
    std::size_t column;      // 列号，从 1 开始

    error_location() noexcept
        : byte_offset(0), line(1), column(1) {}

    error_location(std::size_t byte_offset, std::size_t line, std::size_t column) noexcept
        : byte_offset(byte_offset), line(line), column(column) {}
};

// 所有 JSON 错误的基类。
// 继承 std::runtime_error，因此可以使用 what() 获取错误文本。
class json_error : public std::runtime_error {
    public:
        json_error(error_code code, error_location location, const std::string& message)
            : std::runtime_error(message), code_(code), location_(location) {}

        error_code code() const noexcept { return code_; }
        const error_location& location() const noexcept { return location_; }

    private:
        error_code code_;
        error_location location_;
};

// JSON 解析失败时抛出的异常。
class parse_error : public json_error{
    public:
    parse_error(error_code code,error_location location,const std::string& message):json_error(code,location,message){};;
};

// 访问 JSON 值时类型不匹配。
// 例如对数组调用 as_string()。
class type_error : public std::logic_error{
    public:
    explicit type_error(const std::string& message):std::logic_error(message){};
};

}

#endif
