# JotJSON C++11 实现手册

> 文档版本：v0.3  
> 项目语言：C++11  
> 主要目标：通过实现一个严格、可测试、性能良好的 JSON 库学习现代 C++ 工程实践  
> 文档状态：设计基线。阶段实现中允许调整，但每次重要调整都应更新本手册。

## 1. 文档用途

本手册说明 JotJSON 的目标、技术取舍、模块边界、阶段计划和验收标准。

它不是完整的 API 文档，也不是逐行教程。具体代码会在进入对应阶段后单独提供，并标注文件路径和新增位置。

本手册遵循以下原则：

- 项目使用 C++11，不把 C 工程直接套上 `.cpp` 后缀。
- 每个阶段结束时，项目必须能够编译和测试。
- 每个阶段都必须交付可被外部使用的功能，不允许只交付空壳和测试设施。
- 先保证行为和所有权正确，再使用基准数据优化。
- 公共接口不暴露不必要的内部结构。
- 不因追求性能而牺牲明确的错误处理和资源安全。

## 2. 项目定位

JotJSON 是一个内存内 JSON 文档库，提供以下能力：

- 严格按照 RFC 8259 解析 JSON。
- 构建只读 DOM 树。
- 通过类型安全接口读取数组、对象和标量。
- 将 DOM 序列化为紧凑或美化 JSON。
- 检查 UTF-8、重复键、嵌套深度和数字语法。
- 通过命令行工具完成文件校验和格式化。
- 通过测试、基准和可选的模糊测试验证质量。

### 2.1 学习目标

- 使用 C++11 编写模块化代码。
- 理解值语义、移动语义、RAII 和对象生命周期。
- 使用智能指针和容器表达所有权。
- 使用异常传达无法在局部处理的解析错误。
- 使用递归下降实现语法解析。
- 理解动态容器、哈希索引、UTF-8 和边界检查。
- 使用 CMake、CTest、编译警告、Sanitizer 和基准开展工程验证。

### 2.2 非目标

第一版不支持：

- JSON5、注释、尾逗号、单引号和无引号键。
- `NaN`、`Infinity` 等非标准数字。
- 流式 SAX 解析。
- 多线程解析。
- 在线修改 DOM。
- 手写 SIMD。
- 将超大 JSON 文件分块驻留处理。
- 运行时第三方依赖。

## 3. 语言与标准库边界

项目明确使用 **C++11**，不以 C++14、C++17 或 C++20 特性作为实现前提。

可以使用：

- `std::unique_ptr`、`std::shared_ptr` 和 `std::make_shared`。
- 移动构造、移动赋值和右值引用。
- `override`、`final`、`nullptr`、`enum class`。
- lambda、`std::function`。
- `std::unordered_map`、`std::vector`、`std::string`。
- `std::runtime_error` 及其派生类。
- 基础 `type_traits`、`static_assert`。

第一版不依赖：

- `std::variant`、`std::optional`、`std::string_view`。
- `std::filesystem`。
- `if constexpr`、概念、协程、范围库。
- `std::from_chars` 的浮点重载。

阶段 1 先使用 `std::string` 和 `std::vector`。字符串视图和数值视图属于阶段 4 的性能实现，届时在库内自行实现轻量接口。

### 3.1 分期使用标准库

第一阶段优先让 API 和解析行为正确，直接使用 C++11 标准库：

- 字符串使用 `std::string`。
- 子节点使用 `std::unique_ptr<value>` 表达单一所有权。
- 数组使用 `std::vector<std::unique_ptr<value>>`。
- 对象成员使用连续 `std::vector`，值为 `std::unique_ptr<value>`，保留输入顺序并线性查找。
- 文档通过 RAII 管理资源。

第一阶段不实现 Arena、零复制字符串视图、大对象哈希索引或延迟数字转换。这些结构放入性能阶段，通过替换内部实现完成，不能改变已经稳定的公共访问方式。

这样安排的原因：

- 第一阶段结束时就能解析真实 JSON 文件。
- 新手先掌握树、递归和所有权，再学习分配器与哈希表。
- 性能优化有真实的正确性基线，而不是被前置基础设施拖住。

## 4. 已定技术决策

| 编号 | 决策 | 原因 |
|---|---|---|
| D001 | 使用 C++11 | 明确标准边界，便于学习现代 C++ 基础 |
| D002 | 使用 CMake + Ninja + CTest | 支持可复现构建、测试和后续安装导出 |
| D003 | 公共 JSON 值保持不可变 | 读取安全、生命周期简单 |
| D004 | 文档拥有整棵 DOM | 所有权清晰，避免节点级引用计数 |
| D005 | 第一阶段使用标准容器，性能阶段再引入 Arena | 先交付可用解析器，再基于基准优化 |
| D006 | 单遍递归下降解析 | 不生成 token 数组，避免重复遍历 |
| D007 | 第一阶段字符串直接保存为 `std::string` | 行为清晰，便于新手掌握解码 |
| D008 | 性能阶段再引入无转义字符串视图 | 在正确性稳定后降低复制开销 |
| D009 | 第一阶段保留数字原始文本并同时按需访问 | 兼顾正确性、调试和后续无损回写 |
| D010 | 对象成员使用连续数组保留顺序 | 输出稳定，遍历局部性好 |
| D011 | 性能阶段为大对象建立哈希索引 | 先使用简单线性查找建立正确基线 |
| D012 | 重复键默认报错 | 避免静默覆盖和歧义 |
| D013 | 默认最大嵌套深度为 512 | 限制递归解析的栈消耗 |
| D014 | 只接受 JSON 空白字符 | 严格遵守 RFC 8259 |
| D015 | 解析错误使用专用异常类型 | 将成功路径和错误路径分开 |
| D016 | 公共解析入口同时提供异常版和 `noexcept` 结果版 | 兼顾易用性和性能敏感调用方 |
| D017 | 第一版不手写 SIMD | 先建立正确基线和真实基准 |
| D018 | 阶段编号从“可解析真实 JSON”开始 | 避免无用户价值的空壳阶段 |

## 5. 总体架构

```text
调用方 / CLI
    |
    +-- 公共 API
    |     parse / value / serialize
    |
    +-- 解析层
    |     cursor / grammar / depth / error location
    |
    +-- DOM 层
    |     scalar / array / object
    |
    +-- 基础阶段
    |     std::string / std::vector / utf8 / number
    |
    +-- 性能阶段
          arena / string view / hash index
```

解析器扫描输入时直接构造 DOM。顶层解析完成后，文档拥有所有节点和附属数据。值对象由文档借用给调用方，其生命周期不能超过文档。公共 API 不绑定内部使用标准容器还是 Arena，因此性能阶段可以替换存储方式而不改写调用方。

## 6. 目录结构

```text
JotJSON/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ .gitignore
├─ .clang-format
├─ README.md
├─ include/jotjson/
│  ├─ json.hpp
│  ├─ value.hpp
│  ├─ parse.hpp
│  ├─ serialize.hpp
│  └─ error.hpp
├─ src/
│  ├─ value.cpp
│  ├─ parse.cpp
│  ├─ parse_internal.hpp
│  ├─ serialize.cpp
│  ├─ error.cpp
│  └─ internal/
│     ├─ arena.hpp
│     ├─ arena.cpp
│     ├─ hash.hpp
│     ├─ number.hpp
│     ├─ number.cpp
│     ├─ utf8.hpp
│     └─ utf8.cpp
├─ apps/jotjson/
│  └─ main.cpp
├─ tests/
│  ├─ CMakeLists.txt
│  ├─ test_support.hpp
│  ├─ test_support.cpp
│  ├─ test_value.cpp
│  ├─ test_parse.cpp
│  ├─ test_serialize.cpp
│  └─ data/
│     ├─ valid/
│     └─ invalid/
├─ examples/
│  └─ basic.cpp
├─ benchmarks/
│  ├─ bench_parse.cpp
│  ├─ bench_serialize.cpp
│  └─ data/
├─ fuzz/
│  └─ fuzz_parse.cpp
├─ docs/
│  ├─ implementation-manual.zh-CN.md
│  └─ design.md
└─ cmake/
   └─ JotJSONConfig.cmake.in
```

`docs/design.md` 用于记录实际实现与最初设计的差异。目录只在实际需要时创建，避免堆放空文件。

## 7. 所有权与生命周期

### 7.1 核心规则

- `jotjson::document` 拥有整棵 DOM。阶段 1 的字符串和容器由标准库 RAII 管理；阶段 4 再加入 Arena 和输入副本策略。
- 文档通过 RAII 管理资源，析构函数负责回收。
- `jotjson::value` 的引用只在文档存活期间有效。
- 调用方不能单独销毁 DOM 节点。
- 阶段 1 的 `parse_copy()` 复制输入，文档不借用调用方缓冲区。
- 阶段 4 再提供零复制的 `parse_view()`；其输入必须比文档存活更久。
- 阶段 4 的字符串视图和数字词法视图由文档借用。

### 7.2 基本用法

```cpp
#include <jotjson/json.hpp>

int main() {
    const std::string input = R"({"name":"JotJSON","version":1})";

    jotjson::document doc = jotjson::parse_copy(input);
    const jotjson::value& root = doc.root();

    if (root.is_object()) {
        const jotjson::value* name = root.find("name");
        if (name != nullptr && name->is_string()) {
            const std::string& text = name->as_string();
            // text 只在 doc 存活期间有效
        }
    }
}
```

### 7.3 失败行为

阶段 1 的 `parse_copy()` 在失败时抛出 `jotjson::parse_error`。异常对象包含：

- 错误码。
- 字节偏移。
- 行号和列号。
- 静态错误说明。

失败时不会返回半构造的文档。已经创建的局部资源由 RAII 自动回收。

为需要避免异常的调用方提供结果版：

```cpp
jotjson::parse_result result = jotjson::parse_copy_noexcept(input);
if (result.ok()) {
    jotjson::document doc = std::move(result.document);
}
```

`parse_result` 在 C++11 中可以使用 `std::unique_ptr<document>`、状态码和错误信息的组合实现，不依赖 `std::optional`。

## 8. 存储演进与 Arena 设计

本节的 Arena、字符串视图和哈希索引属于阶段 4 的性能实现，不是阶段 1 的交付前提。

### 8.1 第一条可用路径

阶段 1 使用 C++11 标准容器：

- 标量字符串保存为 `std::string`。
- 子节点使用 `std::unique_ptr<value>` 表达单一所有权。
- 数组保存为 `std::vector<std::unique_ptr<value>>`。
- 对象保存为连续成员数组，值为 `std::unique_ptr<value>`。
- `document` 通过 RAII 管理整棵 DOM。
- 对象查找使用线性扫描。

关键行为必须在此期间稳定：

- 六种 JSON 类型。
- 只读访问。
- 字符串内容和长度正确。
- 对象成员顺序稳定。
- 重复键检测和嵌套深度限制。

阶段 4 替换存储时，仍要满足这些外部行为。

### 8.2 Arena 目标

- 将节点级分配改成文档级分配。
- 简化析构和失败回滚。
- 降低频繁 `new` 带来的开销和碎片。
- 为测试记录分配次数和峰值内存。

### 8.3 实现约束

- Arena 由多个内存块组成。
- 每个返回地址满足 `alignof(std::max_align_t)`。
- 第一版只支持标准对齐要求，不支持用户自定义超对齐类型。
- 大对象可以直接申请专用块。
- Arena 不单独释放一个节点，只支持整体销毁。
- 静态或只读数据不进入 Arena。

### 8.4 对象构造

Arena 只分配原始内存，节点的构造和析构由文档统一管理。若节点内部包含 `std::string`、`std::vector` 等拥有资源的成员，必须保证其析构函数被调用。

第一版建议采用两类存储：

- 生命周期与文档完全相同的简单节点，直接放在 Arena 中。
- 拥有复杂成员的容器或对象，由文档登记并统一析构。

不能只释放 Arena 原始内存而跳过非平凡析构，否则会造成资源泄漏。

### 8.5 容器扩容

JSON 数组和对象的最终大小在解析前未知，因此容器需要扩容。

采用以下策略：

- 初始容量较小，按几何增长扩大。
- 若当前缓冲区是 Arena 最近一次分配，优先原地扩展。
- 无法原地扩展时，从 Arena 分配新缓冲区并复制旧内容。
- 旧缓冲区在文档析构时统一回收。
- 扩容失败时不修改原容器，异常向上传播。

阶段 5 的基准需要记录“输入大小 / 峰值内存”比例，必要时再改为分块或两遍统计方案。

## 9. DOM 数据模型

### 9.1 支持类型

```cpp
enum class value_type {
    null,
    boolean,
    number,
    string,
    array,
    object
};
```

### 9.2 表示方案

C++11 没有 `std::variant`，第一版采用以下结构：

- 基类 `value` 保存类型标签和公共访问接口。
- 派生类表示标量、数组和对象。
- 阶段 1 的节点由文档通过标准容器和 RAII 管理；阶段 4 再评估 Arena 布局。
- 调用方只持有 `const value&` 或 `const value*`。

备选方案是带 `union` 的受控标签结构。该方案需要手写复杂的生命周期管理，对新手不友好，因此阶段 1 不采用。

### 9.3 字符串表示

阶段 1 使用 `std::string` 保存解码后的字符串。这样可以先确保：

- 转义解码正确。
- 字符串可以包含嵌入 NUL。
- 生命周期由文档自动管理。

阶段 4 再引入库内 `string_view`，用于无转义字符串零复制和只读访问。公共 API 的返回方式在阶段 1 固定，内部存储方式在阶段 4 替换。

性能阶段的字符串视图接口可以设计为：

```cpp
class string_view {
public:
    const char* data() const;
    std::size_t size() const;
    bool empty() const;

private:
    const char* data_;
    std::size_t size_;
};
```

字符串使用指针和长度表示，因为 JSON 字符串经过 `\u0000` 转义后可以包含嵌入 NUL 字节。所有比较和哈希必须使用长度，禁止依赖 C 字符串函数。

### 9.4 对象成员

对象成员按输入顺序连续保存。阶段 1 使用连续 `std::vector` 和线性查找，成员包含：

- 键的 `std::string`。
- 值的 `std::unique_ptr<value>`。

阶段 1 的指针树允许容器扩容而不复制子树。`std::unique_ptr` 表达单一所有权；性能阶段可以改为 Arena 中的节点和索引，但外部访问方式保持不变。

阶段 4 再增加开放寻址哈希索引。小对象仍使用线性扫描；大对象建立索引后，哈希表只记录成员下标，不复制键。

重复键检测必须比较解码后的键内容。`"a"` 与 `"\u0061"` 属于重复键。

### 9.5 C++11 下的节点操作

包含 `std::unique_ptr` 的树默认不可复制，这是有意的设计：

- `document` 可以移动，不能复制。
- `value` 的构造在解析阶段完成。
- 对外只暴露 `const value&` 或 `const value*`。
- 不要求用户复制、移动或单独构造 DOM 子树。

如果确实需要在解析过程中转移子节点，应使用显式 `std::move()`，并保证移动后容器中的元素处于有效但未指定状态。阶段 1 的推荐实现是直接在目标容器中构造节点，减少这类状态。

## 10. 解析器设计

### 10.1 输入游标

解析上下文至少包含：

- 输入起始地址和总长度。
- 当前字节偏移。
- 当前行号和列号。
- 最大嵌套深度和当前深度。
- 当前文档构造状态。
- 错误位置。

解析函数只使用显式传入的上下文，不使用全局变量。

### 10.2 语法函数

内部函数按 JSON 语法拆分：

1. 跳过空白。
2. 解析任意值。
3. 解析 `null`、`true`、`false`。
4. 解析数字。
5. 解析字符串。
6. 解析数组。
7. 解析对象。

解析成功后必须确认剩余输入只有合法空白。顶层允许任意 JSON 值，不只允许对象或数组。

阶段 1 的推荐实施检查点：

1. 建立 CMake 工程和完整的公共访问接口。
2. 实现 `document`、`value` 和只读 DOM。
3. 实现游标、空白跳过和错误位置。
4. 实现标量、字符串、数字。
5. 实现数组、对象和递归下降。
6. 实现 `jotjson validate <file>`。
7. 用真实 JSON 文件完成端到端验收。

这些检查点构成一个连续可用功能，不拆成独立空壳阶段。

### 10.3 数字语法

严格接受：

```text
-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
```

必须拒绝：

- 前导零，例如 `01`。
- 缺少整数部分，例如 `.5`。
- 缺少小数部分，例如 `1.`。
- 非法指数，例如 `1e`、`1e+`。
- 正号前缀，例如 `+1`。
- `NaN`、`Infinity`。
- 数字后紧跟非法字符，例如 `1a`。

解析阶段只检查语法并保存原始文本，不立即转换所有数字。

### 10.4 字符串和 Unicode

字符串规则：

- 普通字节先执行快速扫描。
- 未转义控制字符 `0x00` 到 `0x1F` 非法。
- 合法转义包括 `\"`、`\\`、`\/`、`\b`、`\f`、`\n`、`\r`、`\t`。
- `\uXXXX` 必须包含四位十六进制数字。
- 高代理项必须与合法低代理项组成代理对。
- 单独的高代理项或低代理项非法。
- 解码后的 Unicode 标量值编码为 UTF-8。
- 允许 `\u0000`，结果字符串可以包含嵌入 NUL。

处理流程：

1. 扫描字符串，找到结束引号并记录是否出现转义。
2. 阶段 1 将解码后的内容写入 `std::string`。
3. 若存在转义，按规则解码到结果字符串。
4. 非 ASCII 内容执行严格 UTF-8 校验。
5. 阶段 4 再把无转义路径优化为借用视图，并把解码结果放入 Arena。

不使用 `strlen()` 扫描输入，因为输入可能包含 NUL 且不要求以 NUL 结尾。

### 10.5 数组和对象

- 数组元素解析后追加到动态容器。
- 对象先解析键，再解析冒号和值。
- 对象插入前检查重复键。
- 任意值解析前检查嵌套深度。
- 超过最大深度抛出 `depth_limit_error`。
- 解析失败后不返回部分容器。

### 10.6 错误定位

错误类型至少包含：

```cpp
enum class error_code {
    invalid_argument,
    parse,
    type_mismatch,
    out_of_range,
    invalid_utf8,
    depth_limit,
    out_of_memory,
    io
};

struct error_location {
    std::size_t byte_offset;
    std::size_t line;
    std::size_t column;
};
```

约束：

- 字节偏移从 0 开始。
- 行号和列号从 1 开始。
- 使用字节偏移表示位置，不把 UTF-8 多字节字符误计为多个可见列。
- `what()` 返回稳定的静态文本。
- 异常对象不得持有指向临时输入内容的指针。

## 11. 公共 API 草案

该 API 是实现基线，阶段 1 到阶段 3 可以调整命名，但不应随意改变所有权语义。阶段 1 先采用标准容器实现。

```cpp
namespace jotjson {

enum class error_code;

class parse_error : public std::runtime_error {
public:
    error_code code() const noexcept;
    error_location location() const noexcept;
};

class document {
public:
    document(document&& other) noexcept;
    document& operator=(document&& other) noexcept;
    ~document();

    document(const document&) = delete;
    document& operator=(const document&) = delete;

    const value& root() const noexcept;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

struct parse_options {
    std::size_t max_depth;
    parse_options();
};

document parse_copy(
    const char* data,
    std::size_t size,
    const parse_options& options = parse_options());

document parse_copy(
    const std::string& data,
    const parse_options& options = parse_options());

} // namespace jotjson
```

访问接口按类别提供：

```cpp
value_type type() const noexcept;
bool is_null() const noexcept;
bool is_bool() const noexcept;
bool is_number() const noexcept;
bool is_string() const noexcept;
bool is_array() const noexcept;
bool is_object() const noexcept;

bool as_bool() const;
const std::string& as_string() const;
const std::string& number_lexeme() const;
std::int64_t as_int64() const;
double as_double() const;

std::size_t size() const noexcept;
const value& at(std::size_t index) const;
const value* find(const std::string& key) const noexcept;
const value& at(const std::string& key) const;
```

访问接口必须区分以下情况：

- 值类型不匹配。
- 数组索引越界。
- 对象键不存在。
- 数字格式正确但超出 `int64_t` 或 `double` 范围。

建议语义：

- `at(index)` 越界抛出 `std::out_of_range`。
- `at(key)` 键不存在抛出 `std::out_of_range`。
- `find(key)` 键不存在返回 `nullptr`。
- 类型不匹配抛出 `jotjson::type_error`。

阶段 4 可以在不改变上述语义的前提下增加零复制输入入口：

```cpp
document parse_view(
    const char* data,
    std::size_t size,
    const parse_options& options = parse_options());

document parse_view(
    const std::string& data,
    const parse_options& options = parse_options());
```

`parse_view()` 要求输入在文档存活期间保持有效。它是性能接口，不是阶段 1 的必需交付。

## 12. 数字转换

### 12.1 整数

整数访问必须：

- 不经过浮点数中转。
- 正确处理 `INT64_MIN`。
- 检测十进制累加溢出。
- 拒绝带小数部分或指数部分的数字。

例如 `9007199254740993` 可以精确读取为 `std::int64_t`，但转成 `double` 可能无法精确表示。

### 12.2 浮点数

解析阶段不做 `double` 转换。首次调用 `as_double()` 时才转换原始数字文本。

转换层必须隔离 libc 的区域设置差异。实现时不能直接在非 `"C"` 区域设置下把含 `.` 的切片交给 `strtod()` 并假设一定成功。可采用以下方式：

- 将数字复制到有界临时缓冲区并补 NUL。
- 根据当前区域设置正确处理小数点。
- 检查 `ERANGE`、溢出和非法尾字符。
- 对转换行为编写独立测试。

若后续发现 libc 转换无法满足可移植性或并发要求，可以替换为独立的十进制转二进制算法，而不改变公共 API。

### 12.3 序列化

序列化数字时优先原样输出合法词法片段，避免 `1e+10` 被改写为其他格式，也避免不必要的精度损失。

## 13. 对象查找与重复键

### 13.1 小对象

成员数量小于阈值时线性扫描：

- 比较长度。
- 长度相同时使用 `std::memcmp()`。
- 不依赖 NUL 结尾。

### 13.2 大对象

超过阈值后建立开放寻址哈希索引：

- 哈希基于解码后的键字节和长度。
- 哈希值在成员插入时计算一次。
- 负载因子超过上限时扩容并重新插入成员下标。
- 空槽使用独立标记，不用 `0` 同时表示空槽和下标 0。

### 13.3 重复键

- 插入前执行查找。
- 已存在相同键时抛出解析错误。
- 错误位置指向重复键的起始位置。
- 键比较使用解码后的内容，因此转义形式相同也算重复。

## 14. 序列化设计

### 14.1 输出接口

序列化采用流式 writer，而不是每次创建临时字符串：

```cpp
class writer {
public:
    virtual ~writer() {}
    virtual void write(const char* data, std::size_t size) = 0;
};
```

writer 支持：

- 写入文件或标准输出。
- 写入测试用动态缓冲区。
- 未来接入网络或其他目标。

写入失败通过异常或显式结果传播，具体策略在阶段 2 固定。

### 14.2 紧凑与美化

紧凑模式不输出不必要空白。

美化模式支持可配置缩进宽度：

- 默认 2 个空格。
- 数组和对象换行。
- 空数组输出 `[]`。
- 空对象输出 `{}`。
- 不改变成员顺序。
- 不对数字做重新格式化。

### 14.3 字符串转义

输出字符串时：

- 优先复制连续安全片段。
- 转义双引号、反斜杠和所有小于 `0x20` 的控制字符。
- 常用控制字符使用短转义。
- 其他控制字符使用 `\u00XX`。
- `/` 不需要默认转义。
- UTF-8 非 ASCII 字节默认原样输出。

序列化必须使用字符串长度，不能依赖 NUL 结尾。

## 15. 命令行工具

第一阶段先提供真实可用的：

```text
jotjson validate <file>
```

阶段 2 再增加：

```text
jotjson format [--indent N] <file>
```

行为约定：

- 解析成功返回退出码 0。
- 参数错误、文件错误或 JSON 错误返回非 0。
- 错误信息写入标准错误。
- 格式化结果写入标准输出。
- 资源由 RAII 自动回收，异常在 `main()` 边界统一处理。

## 16. 性能策略

### 16.1 已采用策略

- 单遍解析，不创建 token 数组。
- writer 按连续片段输出，减少小写入。
- ASCII 字符串走快速扫描路径。

阶段 4 再采用：

- Arena 集中分配，减少节点级 `new`。
- 无转义字符串零复制。
- 转义字符串只进行必要的局部扫描和一次精确分配。
- 数字延迟转换。
- 小对象线性查找，大对象哈希索引。

### 16.2 暂缓策略

- SIMD 扫描。
- 多线程解析。
- 基于输入采样的容量预测。
- 自定义更复杂的分配器。
- 针对特定 CPU 的指令集。

优化顺序必须是：

1. 建立正确性测试。
2. 建立固定基准语料。
3. 记录耗时、吞吐量、分配次数和峰值内存。
4. 定位真实热点。
5. 一次只改一个主要变量。
6. 用数据证明改动有效且没有破坏语义。

### 16.3 基准指标

- 解析吞吐量：MB/s。
- 序列化吞吐量：MB/s。
- 每个文档的分配次数。
- 每 KB 输入的分配次数。
- 峰值内存与输入大小之比。
- 大对象查找的缓存命中情况。
- Debug、Release 和可选 native 构建的差异。

`-march=native` 只能出现在独立的 `bench-native` 预设中，普通发布构建必须保持可移植。

## 17. 安全与边界

- 所有长度计算检查无符号整数回绕。
- 所有数组索引在访问前验证。
- 输入指针为空且长度大于 0 时返回参数错误。
- 空输入返回解析错误。
- 输入不要求 NUL 结尾。
- 不读取输入边界之外的字节。
- 不递归处理无限嵌套。
- 哈希表检查容量和扩容是否溢出。
- 字符串解码检查代理对和 UTF-8 边界。
- 序列化器检查 writer 写入结果。
- 不默认忽略 UTF-8 BOM；是否接受 BOM 由显式选项决定。

推荐默认值：

| 项目 | 默认值 |
|---|---|
| 最大嵌套深度 | 512 |
| 压缩输出缩进 | 0 |
| 美化输出缩进 | 2 |
| 小对象线性查找阈值 | 8 |
| 哈希表最大负载因子 | 0.7 |
| Arena 初始块大小 | 实现阶段确定并通过基准验证 |

## 18. 测试策略

### 18.1 测试层级

- 单元测试：字符串解码、数字语法、UTF-8、访问器和 DOM 构造。
- 集成测试：完整解析、错误定位、序列化往返。
- 语料测试：合法文件、非法文件和边界输入。
- 失败注入：阶段 4 以后模拟分配失败，验证旧数据不损坏且资源回滚。
- 模糊测试：阶段 5 引入，重点寻找越界、崩溃和无限耗时。

### 18.2 必备边界用例

- 空输入、只有空白、单个值后有多余字符。
- `null`、`true`、`false` 拼写错误。
- 最大深度附近和超限输入。
- 空数组、空对象、尾逗号。
- 对象缺少键、冒号、逗号或结束括号。
- 字符串缺少结束引号。
- 非法转义、短 `\u`、代理项错误。
- 未转义控制字符。
- 内嵌 NUL 转义和对象键中的 NUL。
- 前导零、指数错误、超出整数范围的数字。
- 重复键及转义后重复的键。
- 非法 UTF-8、过长编码、代理区编码和超出 Unicode 范围。
- 阶段 4 以后增加 Arena 和容器分配失败测试。
- writer 在任意一次写入时失败。

### 18.3 测试工具

第一版使用 CTest 和一个极小断言框架，不要求网络下载。后续可以引入 JSONTestSuite 作为补充语料，但基础测试必须能离线运行。

### 18.4 Sanitizer

- Debug 构建默认启用断言。
- 在支持的环境中提供 ASan、UBSan 预设。
- Windows + MinGW 的 Sanitizer 可用性需实际验证，不在未验证时写入“已支持”。
- Linux CI 至少执行一次 Sanitizer 构建。

## 19. 构建与工程规范

### 19.1 CMake

- 使用 `CMakeLists.txt` 管理库、CLI、测试、示例和基准。
- 库目标名使用 `jotjson`。
- 公共头文件使用目标安装接口和构建接口暴露。
- C++ 标准设置为 C++11，禁止 GNU 扩展。
- 编译警告按 `PRIVATE` 挂在项目目标上。

### 19.2 推荐预设

| 预设 | 用途 |
|---|---|
| `debug` | 开发、断言和常规测试 |
| `release` | 优化构建和性能基线 |
| `asan` | 地址和未定义行为检查，按平台支持 |
| `bench` | 可复现基准 |
| `bench-native` | 允许本机指令集优化的实验基准 |

### 19.3 代码规范

- 公共符号位于 `jotjson` 命名空间。
- 头文件使用 include guard 或项目约定的保护方式，保证可重复包含。
- 内部符号尽量放入匿名命名空间或标记为 `static`。
- 不使用全局可变状态。
- 每个动态资源的释放路径必须能从代码直接看懂。
- 复杂分支添加说明“为什么”的注释，不写逐字翻译代码的空注释。
- 格式化使用仓库内 `.clang-format`。
- 提交前至少执行 Debug 构建和全部测试。

## 20. 阶段计划

阶段编号按“用户可感知的功能”划分。构建、测试和文档是每个阶段的一部分，不单独占据一个空壳阶段。

### 阶段 1：可用的 JSON 解析器

目标：交付一个可以解析和查询真实 JSON 文件的 C++11 库与 CLI。

交付：

- CMake 工程、C++11 标准、Debug / Release 预设和测试骨架。
- 公共 `document`、`value` 和只读访问接口。
- 六种 JSON 类型：null、bool、number、string、array、object。
- 标准数字格式、字符串转义、`\uXXXX`、代理对和 UTF-8 校验。
- 嵌套数组、对象、重复键检测和最大嵌套深度保护。
- 输入游标、错误码、字节偏移、行号和列号。
- `jotjson validate <file>` 文件校验命令。
- 一个能读取实际 JSON 的示例程序。

实现策略：

- 使用 `std::string` 保存字符串。
- 使用 `std::vector` 保存数组和按顺序排列的对象成员。
- 子节点使用 `std::unique_ptr<value>` 表达所有权。
- 对象查找使用线性扫描。
- 单遍递归下降解析，不创建 token 数组。
- `document` 用 RAII 管理整棵树。
- 测试是完成门槛，与功能代码同一阶段交付。

验收：

- `jotjson validate sample.json` 对合法文件返回退出码 0。
- 非法 JSON 返回非零退出码并报告准确错误位置。
- 库示例可以读取嵌套对象、数组、字符串和数字。
- 六种类型和边界输入测试全部通过。
- Debug 和 Release 构建均通A过测试且无编译警告。
- 本阶段不依赖 Arena、哈希索引或第三方库。

### 阶段 2：JSON 序列化与格式化工具

目标：让库能够生成合法 JSON，并让 CLI 具备格式化能力。

交付：

- 紧凑序列化。
- 可配置缩进的美化序列化。
- 字符串和 Unicode 输出转义。
- writer 接口。
- `jotjson format [--indent N] <file>`。
- 解析和重新解析的往返测试。

验收：

- 所有合法文档都能序列化后重新解析。
- 数字词法片段不被无意改写。
- 空数组、空对象和嵌套结构格式正确。
- 字符串中的引号、反斜杠、控制字符和 Unicode 正确转义。
- CLI 格式化结果可直接通过 `validate`。

### 阶段 3：严格语义与安全边界

目标：把“常见 JSON 能用”提升为“恶意和边缘输入也有明确行为”。

交付：

- 完整 UTF-8 合法性检查。
- 非法代理项、控制字符、过长编码和越界码点测试。
- 重复键和转义后重复键测试。
- BOM、空输入、尾随垃圾和深度限制策略。
- 数字边界和区域设置处理。
- 更稳定的错误信息和合法/非法测试语料。

验收：

- 畸形输入不崩溃、不越界、不产生半成品文档。
- 所有语法错误都有稳定错误码和位置。
- 合法和非法语料全部通过。
- 公共行为与手册约定一致。

### 阶段 4：性能实现替换

目标：在不改变公共行为的前提下，减少分配和复制。

交付：

- 文档级 Arena。
- 无转义字符串借用视图和精确解码分配。
- 数字延迟转换。
- 小对象线性查找、大对象开放寻址哈希索引。
- 分配次数和峰值内存统计。
- 测试确认调用方不依赖内部布局。

验收：

- 阶段 1 到阶段 3 的公共测试全部继续通过。
- 无转义字符串路径不产生额外字符串副本。
- 文档释放由 RAII 一次完成。
- 大对象查找平均 O(1)。
- 性能改动有基准数据支持。

### 阶段 5：鲁棒性验证与性能报告

目标：用可复现证据验证解析器不怕异常输入，并确认性能改进真实有效。

交付：

- 模糊测试入口。
- 支持环境下的 ASan 和 UBSan 验证。
- 固定合法和非法语料。
- 解析和序列化基准。
- Release 与实验性 native 构建对比。
- 分配次数、吞吐量和峰值内存报告。

验收：

- 模糊测试短时间内不发现崩溃、越界或泄漏。
- Sanitizer 构建通过已有测试。
- 基准可重复运行，并记录优化前后差异。
- 性能报告说明瓶颈、改动和结果。

### 阶段 6：发布与外部使用

目标：交付可被其他项目安装和链接的 C++11 JSON 库。

交付：

- CMake 安装规则和包导出。
- 外部项目使用示例。
- Windows 与 Linux CI。
- 完善 README、API 说明和限制说明。
- 发布版本号与变更记录。

验收：

- 安装后的目标可被独立 C++ 项目链接。
- 全新构建目录可以按 README 完成构建和测试。
- CLI 的安装、版本和帮助信息可用。
- 跨平台 CI 结果可复现。

## 21. 每阶段交付格式

进入某个阶段后，代码按以下格式提供：

1. 本阶段目标和前置知识。
2. 本阶段包含与不包含的内容。
3. 文件路径和放置位置。
4. 新文件完整代码，或已有文件的最小补丁。
5. 使用 `// [阶段 N 新增]` 标记新增位置。
6. 用中文解释所有权、返回码或异常、易错边界。
7. 配置、编译、运行和测试命令。
8. 预期输出。
9. 阶段验收清单。
10. 一到两个检查理解的小练习。

代码只放在对话中，不直接写入项目文件。用户完成并验证一个阶段后，再进入下一阶段。

## 22. 阶段完成定义

一个阶段只有同时满足以下条件才算完成：

- 代码可编译。
- 本项目启用的警告为零。
- 本阶段测试全部通过。
- 失败路径已验证资源回收。
- 公共行为有书面说明。
- 没有把后续阶段功能提前塞入本阶段。
- 文档与实现不一致之处已更新。

如果测试失败，优先定位和修复，不以“下一阶段再处理”的方式绕过。

## 23. 待确认与后续记录

以下事项不阻塞阶段 1 的主体实现，但需要在对应阶段前确认：

| 事项 | 当前默认 | 最迟确认阶段 |
|---|---|---|
| 浮点精度要求 | 遵循 `double` 精度，同时保留原文 | 阶段 1 |
| 非 C 区域设置支持 | 阶段 3 前必须正确处理或明确限制 | 阶段 3 |
| BOM 策略 | 默认拒绝，错误位置指向首字节 | 阶段 3 |
| 对象不存在键的返回码 | 异常版抛 `std::out_of_range`，查找版返回空指针 | 阶段 1 |
| 小对象阈值 | 8 | 阶段 4 |
| Arena 初始块大小 | 由基准决定 | 阶段 4 |
| 公开自定义分配器 | 第一版不公开 | 阶段 4 |
| 语料来源 | 阶段 1 自建边界用例，阶段 5 补充公开语料 | 阶段 5 |

## 24. 术语表

- **DOM**：把整个 JSON 文档表示为树的数据结构。
- **Arena**：按块分配、整体释放的内存分配器。
- **RAII**：资源获取即初始化，由对象生命周期自动管理资源。
- **借用视图**：只保存指针和长度，不拥有底层内存。
- **零复制**：不需要为字符串另行分配和复制内存。
- **递归下降**：每个语法规则对应一个解析函数，函数之间可以递归调用。
- **开放寻址**：哈希冲突时在表内探测下一个可用槽位。
- **负载因子**：已用槽位数量与哈希表容量的比值。

## 25. 变更记录

| 版本 | 日期 | 内容 |
|---|---|---|
| v0.1 | 2026-10-28 | 建立 C11 项目设计基线 |
| v0.2 | 2026-10-28 | 更正项目语言为 C++11，重写目录、API、所有权和构建方案 |
| v0.3 | 2026-11-02 | 改为功能优先的 6 阶段路线，阶段 1 直接交付可解析真实 JSON 的库和 CLI |
