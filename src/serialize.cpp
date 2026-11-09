#include <jotjson/serialize.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

namespace jotjson {
    namespace {
        const std::size_t max_indent_width = 1024 * 1024;
        char hex_digit(unsigned int value) {
            static const char digits[]  = "0123456789ABCDEF";
            return digits[value & 0x0Fu];
        }

        // 实际执行递归序列化的内部实现。
        //
        // 该类不拥有 DOM，只借用 value；
        // 所有输出都通过 writer 完成。
        class serializer_impl {
        public:
            serializer_impl(
                writer& output,
                const serialize_options& options)
                : output_(output), options_(options) {}
            
            void write_root(const value& root) {
                write_value(root, 0);
            }
        
        private:
            writer& output_;
            serialize_options options_;

            void write_text(const char* data, std::size_t size) {
                if (size != 0) {
                    output_.write(data, size);
                }
            }

            void write_char(const char c) {
                output_.write(&c, 1);
            }

            // 写多个空格。
            void write_spaces(std::size_t count) {
                // 使用固定的小块重复写入，避免为一个超大的缩进字符串
                // 一次性分配过多内存。
                static const char spaces[] = "                                ";
                const std::size_t chunk_size = sizeof(spaces) - 1;

                while (count != 0) {
                    const std::size_t amount =
                        count < chunk_size ? count : chunk_size;

                    output_.write(spaces, amount);
                    count -= amount;
                }
            }

            // 按当前嵌套层数写入缩进。
            void write_indent(std::size_t depth) {
                if (!options_.pretty || depth == 0) {
                    return;
                }

                // depth * indent 必须不超过 size_t 的最大值。
                // 先做除法检查，避免乘法本身发生无符号回绕。
                if (options_.indent > max_indent_width || (options_.indent != 0 &&
                    depth >
                        std::numeric_limits<std::size_t>::max() /
                            options_.indent)) {
                    throw std::length_error(
                        "serialization indentation is too large");
                }

                write_spaces(depth * options_.indent);
            }

            // 输出一个 JSON 字符串，包括两侧双引号。
            void write_json_string(const std::string& text) {
                write_char('"');

                std::size_t run_start = 0;

                for (std::size_t i = 0; i < text.size(); i++) {
                    const unsigned char current = static_cast<unsigned char>(text[i]);

                    // 普通 ASCII 和 UTF-8 非 ASCII 字节可以直接输出。
                    // 只有双引号、反斜杠和控制字符需要转义。
                    if (current >= 0x20 &&
                        current != static_cast<unsigned char>('"') &&
                        current != static_cast<unsigned char>('\\')) {
                        continue;
                    }

                    write_text(text.data() + run_start, i - run_start);

                    switch (current) {
                        case '"':
                            write_text("\\\"", 2);
                            break;
                        case '\\':
                            write_text("\\\\", 2);
                            break;
                        case '\b':
                            write_text("\\b", 2);
                            break;
            
                        case '\f':
                            write_text("\\f", 2);
                            break;
            
                        case '\n':
                            write_text("\\n", 2);
                            break;
            
                        case '\r':
                            write_text("\\r", 2);
                            break;
            
                        case '\t':
                            write_text("\\t", 2);
                            break;
            
                        default: {
                            // 其他控制字符使用 \u00XX。
                            const char escaped[6] = {
                                '\\',
                                'u',
                                '0',
                                '0',
                                hex_digit(current >> 4),
                                hex_digit(current)
                            };
            
                            write_text(escaped, 6);
                            break;
                        }
    
                    }
                    run_start = i + 1;

                }
                write_text(text.data() + run_start, text.size() - run_start);
                write_char('"');
            }

            void write_array(const value& node, std::size_t depth) {
                write_char('[');

                const std::size_t count = node.size();

                if (count == 0) {
                    write_char(']');
                    return;
                }

                for (std::size_t i = 0; i < count; i++) {
                    if (options_.pretty) {
                        write_char('\n');
                        write_indent(depth + 1);
                    }

                    write_value(node.at(i), depth + 1);

                    if (i + 1 < count) {
                        write_char(',');
                    }
                }
                if (options_.pretty) {
                    write_char('\n');
                    write_indent(depth);
                }

                write_char(']');
            }

            void write_object(const value& node, std::size_t depth) {
                write_char('{');
                
                const std::size_t count = node.size();

                // 空对象必须直接输出 "{}"。
                // 否则美化模式会错误输出 "{\n}"。
                if (count == 0) {
                    write_char('}');
                    return;
                }

                for (std::size_t i = 0; i < count; i++) {
                    std::string key;
                    const value* member = nullptr;

                    // 成员顺序与解析时保持一致。
                    const bool member_ok =
                        node.member_at(i, &key, &member);

                    if (!member_ok || member == nullptr) {
                        throw std::logic_error(
                            "object member index is inconsistent");
                    }

                    if (options_.pretty) {
                        write_char('\n');
                        write_indent(depth + 1);
                    }

                    write_json_string(key);
                    write_char(':');

                    if (options_.pretty) {
                        write_char(' ');
                    }

                    write_value(*member, depth + 1);
                    if (i + 1 < count) {
                        write_char(',');
                    }
                }
                if (options_.pretty) {
                    write_char('\n');
                    write_indent(depth);
                }

                write_char('}');
            }

            void write_value(const value& node, std::size_t depth) {
                switch (node.type()) {
                    case value_type::nil:
                        write_text("null", 4);
                        return;
                    
                    case value_type::boolean:
                        if (node.as_bool()) {
                            write_text("true", 4);
                        } else {
                            write_text("false", 5);
                        }
                        return;
                    
                    case value_type::number: {
                        const std::string& lexeme = node.number_lexeme();
                        write_text(lexeme.data(), lexeme.size());
                        return;
                    }
                    case value_type::string: {
                        const std::string& text = node.as_string();

                        write_json_string(text);
                        return;
                    }

                    case value_type::array:
                        write_array(node, depth);
                        return;
                    
                    case value_type::object:
                        write_object(node, depth);
                        return;
                }
                throw std::logic_error("unknown JSON value type");
            }
        };
    } // namespace

    void string_writer::write(
        const char* data,
        std::size_t size) {
        if (size == 0) {
            return;
        }

        if (data == nullptr) {
            throw std::invalid_argument(
                "writer received null data with non-zero size");
        }
        buffer_.append(data, size);
    }

    const std::string& string_writer::str() const noexcept {
        return buffer_;
    }

    // 保证str 后buffer_为空
    std::string string_writer::take_string() {
        std::string result;
        result.swap(buffer_);
        return result;
    }

    void string_writer::clear() noexcept {
        buffer_.clear();
    }

    void serialize(
        const value& root,
        writer& output,
        const serialize_options& options) {
        serializer_impl serializer(output, options);
        serializer.write_root(root);
    }

    void serialize_compact(
        const value& root,
        writer& output) {
        serialize(root, output, serialize_options(false, 0));
    }

    void serialize_pretty(
        const value& root,
        writer& output,
        std::size_t indent) {
        serialize(root, output, serialize_options(true, indent));
    }

    std::string serialize_to_string(
        const value& root,
        const serialize_options& options) {
        string_writer output;
        serialize(root, output, options);
        return output.take_string();
    }
} // namespace jotjson
