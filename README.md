# JotJSON

JotJSON 是一个使用 C++11 编写的严格 JSON 解析与序列化库，同时提供
命令行工具。

项目重点放在可读实现、严格语义和可验证行为上：

- 支持 RFC 8259 规定的六种 JSON 类型。
- 严格校验数字、字符串转义、Unicode 和 UTF-8。
- 拒绝重复键、BOM、尾随垃圾和超过深度限制的输入。
- 保留数字的原始文本，避免格式化时无意改变精度或指数写法。
- 对象成员按输入顺序保存。
- 提供完整 DOM、紧凑序列化和美化序列化。
- 提供固定语料、基准、模糊测试入口和性能报告。

当前版本为 `0.1.0`。

## 当前状态

JotJSON 已经具备可用的解析、访问和序列化能力，但性能实现仍处于基础
阶段。当前 DOM 使用 `std::string`、`std::vector` 和 `std::unique_ptr`
保存数据。

这意味着：

- 适合学习、验证和中小规模 JSON 处理。
- 当前不是 SIMD 或高吞吐量解析器。
- Arena、无转义字符串视图、数字转换缓存和大对象哈希索引尚未实现。
- 在大型 JSON 或大量小对象场景下，性能会明显低于成熟工业库。

阶段 5 的实测数据和限制见
[性能报告](docs/stage5-performance-report.zh-CN.md)。

## 环境要求

- CMake 3.20 或更高版本
- 支持 C++11 的编译器
- Ninja
- CTest

本项目已在 Windows + MinGW-W64 GCC 上完成 Debug 和 Release 测试。

## 快速构建

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

Release 构建：

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

构建成功后，主要产物包括：

```text
build/debug/libjotjson.a
build/debug/bin/jotjson.exe
build/debug/jotjson_example.exe
build/debug/jotjson_serialize_example.exe
```

## C++ 使用

公共头文件入口：

```cpp
#include <jotjson/jotjson.hpp>
```

### 解析和访问

```cpp
#include <jotjson/jotjson.hpp>

#include <exception>
#include <iostream>
#include <string>

int main() {
    const std::string input =
        R"({"name":"JotJSON","version":1,"items":[1,2,3]})";

    try {
        const jotjson::document document =
            jotjson::parse_copy(input);

        const jotjson::value& root = document.root();

        const jotjson::value* name = root.find("name");
        const jotjson::value* items = root.find("items");

        if (name == nullptr || items == nullptr) {
            return 1;
        }

        std::cout
            << name->as_string()
            << " has "
            << items->size()
            << " items\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }

    return 0;
}
```

### 序列化

```cpp
const jotjson::document document =
    jotjson::parse_copy(input);

// 紧凑格式
const std::string compact =
    jotjson::serialize_to_string(document.root());

// 两空格美化格式
const std::string pretty =
    jotjson::serialize_to_string(
        document.root(),
        jotjson::serialize_options(true, 2));
```

### 主要 API

解析：

```cpp
document parse_copy(
    const char* data,
    std::size_t size,
    const parse_options& options = parse_options());

document parse_copy(
    const std::string& data,
    const parse_options& options = parse_options());
```

访问：

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

std::size_t size() const;
const value& at(std::size_t index) const;
const value& at(const std::string& key) const;
const value* find(const std::string& key) const;

bool member_at(
    std::size_t index,
    std::string* key,
    const value** out_value) const;
```

序列化：

```cpp
void serialize(
    const value& root,
    writer& output,
    const serialize_options& options = serialize_options());

std::string serialize_to_string(
    const value& root,
    const serialize_options& options = serialize_options());
```

## 所有权与生命周期

`document` 拥有整棵 DOM。`value` 只能由文档创建和持有，调用方拿到的
`const value&` 或 `const value*` 只在对应文档存活期间有效。

```cpp
// 正确：document 存活期间访问 root
const jotjson::document document =
    jotjson::parse_copy(input);

const jotjson::value& root = document.root();

// 错误：文档释放后不能再访问旧引用
```

`document` 可以移动，但不能复制。移动后原对象不应再访问 `root()`。

解析失败时不会返回半成品文档，已创建的资源由 RAII 自动释放。

## 错误处理

解析错误抛出 `jotjson::parse_error`，可通过 `code()` 和 `location()`
获取稳定错误码、字节偏移、行号和列号。

```cpp
try {
    jotjson::parse_copy(input);
} catch (const jotjson::parse_error& error) {
    const jotjson::error_location location = error.location();

    std::cerr
        << location.line << ":"
        << location.column << ": "
        << error.what() << "\n";
}
```

类型不匹配抛出 `jotjson::type_error`。数组下标越界和对象键不存在抛出
`std::out_of_range`。

## 命令行工具

构建后运行：

```powershell
.\build\debug\bin\jotjson.exe --help
```

支持：

```text
jotjson --version
jotjson --help
jotjson validate <file>
jotjson validate-text <json>
jotjson format [--indent N] <file>
```

示例：

```powershell
.\build\debug\bin\jotjson.exe validate tests\data\valid\nested.json
.\build\debug\bin\jotjson.exe validate-text "{\"ok\":true}"
.\build\debug\bin\jotjson.exe format --indent 2 tests\data\valid\nested.json
```

合法 JSON 返回退出码 `0`，参数错误、文件错误或 JSON 错误返回非零值。

## 测试

普通测试包含：

- 基础类型、访问器、错误码和错误位置。
- 数字、字符串、Unicode、UTF-8 和严格边界。
- 紧凑/美化序列化和往返解析。
- 固定合法与非法语料。
- 基线行为和阶段 5 验证。

```powershell
ctest --preset debug --output-on-failure
```

测试文件和语料位于：

```text
tests/
├─ test_main.cpp
├─ test_serialize.cpp
├─ test_strict.cpp
├─ test_baseline.cpp
├─ test_stage5.cpp
└─ data/
```

## 基准

基准使用独立构建，默认不进入普通开发构建：

```powershell
cmake --preset bench
cmake --build --preset bench

.\build\bench\jotjson_bench_parse.exe tests\data\valid\large_object.json 2000
.\build\bench\jotjson_bench_serialize.exe tests\data\valid\large_object.json 2000
```

bench-native 只用于实验，不允许替代可移植发布构建：

```powershell
cmake --preset bench-native
cmake --build --preset bench-native
```

## 模糊测试

没有 libFuzzer 时，可以单独运行固定语料入口：

```powershell
cmake --preset fuzz
cmake --build --preset fuzz

.\build\fuzz\bin\jotjson_fuzz_parse.exe tests\data\invalid\bad_escape.json
```

在支持 `-fsanitize=fuzzer` 的 Clang 环境中，可以启用持续模糊测试。
具体命令见 [阶段 5 验证入口](docs/stage5-verification.zh-CN.md)。

## Sanitizer

CMake 提供 `asan` 预设：

```powershell
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
```

当前 Windows MinGW 环境缺少 `libasan` 和 `libubsan`，因此本机
Sanitizer 尚未验证。必须在支持的工具链上完成实测后才能声明通过。

## 目录结构

```text
JotJSON/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ include/jotjson/      公共头文件
├─ src/                  库和 CLI 实现
├─ example/              使用示例
├─ tests/                单元、严格语义、基线和语料测试
├─ benchmarks/           解析和序列化基准
├─ fuzz/                 模糊测试入口
├─ scripts/              阶段 5 运行脚本
└─ docs/                 实现手册和性能报告
```

## 阶段状态

| 阶段 | 内容 | 状态 |
|---|---|---|
| 阶段 1 | 可用的 JSON 解析器 | 已完成 |
| 阶段 2 | 序列化与格式化 | 已完成 |
| 阶段 3 | 严格语义与安全边界 | 已完成 |
| 阶段 4 | 性能实现替换 | 未完成；尝试方案因未获得稳定收益已撤销 |
| 阶段 5 | 鲁棒性验证与性能报告 | 已完成本机可验证部分 |
| 阶段 6 | 发布与外部使用 | 未开始 |

阶段 4 仍需真正解决节点布局、内存分配和对象访问开销。当前没有可以
对外声称的稳定性能提升版本。

## 已知限制

- 只支持 UTF-8 输入；默认拒绝 UTF-8 BOM。
- 不提供注释和尾随逗号等 JSON5 扩展。
- 不支持解析视图或零复制字符串接口。
- 公共 API 当前返回 `const std::string&`。
- 对象查找仍使用线性扫描。
- 大型输入的用户自定义分配器尚未公开。
- 当前 MinGW 环境未验证 Sanitizer。
- 尚未提供安装导出和外部项目链接支持。

## 相关文档

- [实现手册](docs/implementation-manual.zh-CN.md)
- [阶段 5 验证入口](docs/stage5-verification.zh-CN.md)
- [阶段 5 性能报告](docs/stage5-performance-report.zh-CN.md)

## 许可证

当前仓库尚未添加许可证文件。对外发布或供其他项目使用前，应先明确
许可证。
