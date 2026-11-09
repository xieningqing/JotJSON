#ifndef JOTJSON_SERIALIZE_HPP
#define JOTJSON_SERIALIZE_HPP

#include <jotjson/value.hpp>

#include <cstddef>
#include <string>

namespace jotjson {
// 序列化输出目标。
//
// writer 是抽象接口，serializer 只负责产生 JSON 字节，
// 不关心最终写文件、标准输出还是内存缓冲区。
class writer {
public:
    virtual ~writer() {}

    // 写入一段连续字节。
    //
    // 当 size 为 0 时，实现可以立即返回。
    // 如果写入失败，实现可以抛出异常，异常会传播给 serialize() 的调用方。
    virtual void write(const char* data, std::size_t size) = 0;
};

// 把序列化结果保存在内存中。
// 主要用于测试、命令行输出以及需要完整 JSON 字符串的调用方。
class string_writer final : public writer {
public:
    void write(const char* data, std::size_t size) override;

    const std::string& str() const noexcept;

    // 取出内部字符串。
    // 调用后 writer 仍可继续使用，旧缓冲区会被移走。
    std::string take_string();

    // 清空已经写入的内容，但保留 writer 对象本身。
    void clear() noexcept;

private:
    std::string buffer_;
};


// 序列化选项。
//
// 默认使用紧凑格式。
// pretty 为 true 时，数组和对象会换行并使用 indent 个空格缩进。
struct serialize_options {
    bool pretty;
    std::size_t indent;

    serialize_options() noexcept
        : pretty(false), indent(2) {}

    serialize_options(
        bool pretty_value,
        std::size_t indent_value) noexcept
        : pretty(pretty_value), indent(indent_value) {}
};

// 将 DOM 序列化到 writer。
//
// 数字直接输出解析时保存的原始词法片段，
// 因此不会因为再次格式化而改变精度或指数写法。
void serialize(
    const value& root,
    writer& output,
    const serialize_options& options = serialize_options());

// 紧凑格式，不输出任何不必要的空白。
void serialize_compact(
    const value& root,
    writer& output);

// 美化格式，默认使用两个空格缩进。
void serialize_pretty(
    const value& root,
    writer& output,
    std::size_t indent = 2);

// 序列化到新字符串，便于简单调用。
std::string serialize_to_string(
    const value& root,
    const serialize_options& options = serialize_options());

} // namespace jotjson

#endif
