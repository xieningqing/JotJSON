#ifndef  JOTJSON_VALUE_HPP
#define  JOTJSON_VALUE_HPP

#include<jotjson/error.hpp>
#include <cstddef>
#include<cstdint>
#include<memory>
#include<string>
#include<vector>

namespace jotjson{
// parser 是内部解析器。
// value 和 document 需要允许 parser 构造它们，因此提前声明。
class parser;

enum class value_type{
    nil, // null
    boolean, // true/false
    number, // 数字
    string, // 字符串
    array, // 数组
    object // 对象
};

// 只读 JSON 值。
//
// 阶段 1 的数据结构：
// - 字符串保存在 std::string 中；
// - 数组中的子节点由 std::unique_ptr<value> 拥有；
// - 对象中的键和值分别保存在两个 vector 中；
// - 调用方只能获得 const value& 或 const value*。
//
// 这样做的好处是所有权非常明确：
// document 析构时，root_ 自动析构，整棵子树随之递归释放。
class value{
public:
    // 查询当前节点类型
    value_type type() const noexcept;

    // 类型判断函数。
    // 使用这些函数可以避免直接比较 enum。
    bool is_null() const noexcept;
    bool is_bool() const noexcept;
    bool is_number() const noexcept;
    bool is_string() const noexcept;
    bool is_array() const noexcept;
    bool is_object() const noexcept;

    // 读取布尔值。
    // 如果当前节点不是 boolean，会抛出 type_error。
    bool as_bool() const;

    const std::string& as_string() const;

    const std::string& number_lexeme() const;
    
    std::int64_t as_int64() const;

    double as_double() const;

    //返回长度
    std::size_t size() const;
    
    // 数组访问。
    // 当前节点不是数组时抛出 type_error；
    // 下标越界时抛出 std::out_of_range。
    const value& at(std::size_t index) const;

    // 对象
    const value& at(const std::string& key) const;

    const value* find(const std::string& key) const; //不存在返回null

     // 按位置读取对象成员。
    //
    // index 表示第几个成员，从 0 开始。
    // 成功返回 true；
    // 当前节点不是对象、或 index 越界时返回 false。
    //
    // key 和 out_value 可以为 nullptr，表示不需要该输出。
    bool member_at(std::size_t index,std::string* key,const value** out_value) const;

private:
    explicit value(value_type type);

    // JSON 值树不允许复制。
    // 这能避免两个 document 意外共享同一批子节点。
    value(const value&) = delete;

    value& operator=(const value&) = delete;

    // 以下函数由 parser 在构建 DOM 时调用。
    void set_bool(bool value);
    void set_string(std::string&& value);
    void set_number(std::string&& lexeme);
    void append_array(std::unique_ptr<value> child);
    void append_object(std::string&& key,std::unique_ptr<value> value);

    friend class parser;

    value_type type_; // store the type of the value

    bool bool_value_; // store the boolean value if type_ is boolean

    std::string string_value_;

    std::string number_lexeme_;

    std::vector<std::unique_ptr<value>>array_values_;

    // object 类型分别保存键和值。
    // 两个 vector 的下标一一对应。
    std::vector<std::string>object_keys_;
    std::vector<std::unique_ptr<value>>object_values_;
};

// 一个 JSON 文档拥有整棵 DOM 树。
//
// document 可以移动，但不能复制。
// 移动后，原 document 仍然可以析构，但不应再访问它的 root()。
class document{
public:
    document(document&& other) noexcept;
    document& operator=(document&& other) noexcept;
    ~document();

    // 返回根节点。
    const value& root() const ;

private:
    explicit document(std::unique_ptr<value> root);

    friend class parser;

    std::unique_ptr<value> root_;
};


}
#endif
