#ifndef JOTJSON_PARSE_HPP
#define JOTJSON_PARSE_HPP

#include <jotjson/value.hpp>
#include<cstddef>
#include<string>

namespace jotjson{
// 解析选项。
//
// max_depth 用来防止恶意输入无限嵌套，
// 从而避免递归调用耗尽调用栈。

struct parse_options{
    std::size_t max_depth;

    // 默认拒绝 utf-8 bom
    bool allow_utf8_bom;

    parse_options():max_depth(512),allow_utf8_bom(false){}
};

// 解析 JSON，并复制输入内容。
//
// 这里复制输入的好处是：
// 调用方读取完文件或临时字符串后，可以立即释放原缓冲区，
// 不必担心 document 还引用外部内存。
document parse_copy(
    const char* data,
    std::size_t size,
    const parse_options& options = parse_options()
);

// 解析 JSON，并复制输入内容，不带长度参数，适用于以 '\0' 结尾的字符串。
document parse_copy(
    const std::string& data,
    const parse_options& options = parse_options()
);
}

#endif