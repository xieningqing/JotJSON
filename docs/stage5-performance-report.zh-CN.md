# 阶段 5 鲁棒性验证与性能报告

## 1. 结论摘要

本阶段新增了固定合法/非法语料、语料回归测试、解析/序列化基准、
模糊测试入口、ASan/UBSan 和 native 构建预设，以及性能报告。

当前实现仍是阶段 1 的 `std::string`、`std::vector` 和
`std::unique_ptr` DOM。阶段 4 的 Arena、无转义字符串视图、
数字结果缓存和大对象哈希索引尚未落地，因此本报告记录的是
**优化前基线**，不能声称阶段 4 已带来性能提升。

本机实测限制：

- 编译器：MinGW-W64 GCC 15.2.0，x86_64，UCRT，POSIX 线程模型。
- 构建：Debug 和 Release 测试通过。
- Sanitizer：本机缺少 `libasan` 和 `libubsan`，链接失败，未验证。
- libFuzzer：本机没有对应运行时库，只验证了普通模糊入口。
- 分配次数和峰值内存：现有 MinGW `new/delete` 路径无法被稳定包装，
  没有可信测量，不能把 0 当成结果。

## 2. 固定语料

合法语料：

| 文件 | 覆盖点 |
|---|---|
| `tests/data/valid/empty_object.json` | 空对象 |
| `tests/data/valid/nested.json` | 嵌套对象和数组 |
| `tests/data/valid/unicode.json` | UTF-8、代理对、嵌入 NUL |
| `tests/data/valid/numbers.json` | 整数、负数、小数、指数和 int64 边界 |
| `tests/data/valid/large_object.json` | 32 成员对象 |

非法语料：

| 文件 | 覆盖点 |
|---|---|
| `tests/data/invalid/trailing_garbage.json` | 尾随垃圾 |
| `tests/data/invalid/leading_zero.json` | 前导零 |
| `tests/data/invalid/bad_escape.json` | 非法转义 |
| `tests/data/invalid/unterminated_string.json` | 未结束字符串 |
| `tests/data/invalid/invalid_utf8.bin` | 非法 UTF-8 |
| `tests/data/invalid/duplicate_key.json` | 重复键 |
| `tests/data/invalid/empty.json` | 空输入 |
| `tests/data/invalid/whitespace.json` | 仅空白 |
| `tests/data/invalid/depth_limit.json` | 超过默认嵌套深度 |

测试程序：`tests/test_stage5.cpp`

## 3. 功能与鲁棒性结果

Debug 构建测试结果：

```text
100% tests passed, 0 tests failed out of 5
```

模糊入口 `fuzz/fuzz_parse.cpp` 已编译，并用全部 18 个固定语料文件
逐一运行，进程均正常退出。非法输入由 `parse_error` 拒绝，没有把
语法错误当成崩溃，也没有吞掉 `std::bad_alloc`。

这只能证明入口和固定语料路径可运行，不证明随机模糊探索已经完成。
阶段 5 的验收仍需要在支持 libFuzzer 的平台执行持续随机测试。

## 4. 基准方法

- 编译器：MinGW-W64 GCC 15.2.0。
- 普通基准：`bench` 预设，`Release`，可移植编译选项。
- 实验基准：`bench-native` 预设，`Release` 加 `-march=native`。
- 计时：`std::chrono::steady_clock`。
- 每次测量先预热一次，再重复解析或序列化。
- 解析测量包含 `parse_copy` 的输入复制和 DOM 构建。
- 序列化测量只包含序列化，DOM 在计时前构建。
- 吞吐量按“输入文件字节数 × 迭代次数”计算，不是输出字节数。

## 5. 实测结果

以下数字来自本机单次运行，只用于说明方法和当前量级：

### `tests/data/valid/nested.json`，72 字节

| 构建 | 操作 | 迭代 | 总耗时（秒） | 吞吐量（MB/s） |
|---|---:|---:|---:|---:|
| bench | parse | 5000 | 0.0074778 | 45.9123 |
| bench | serialize | 5000 | 0.002046 | 167.802 |
| bench-native | parse | 5000 | 0.007409 | 46.3386 |
| bench-native | serialize | 5000 | 0.001864 | 184.186 |

### `tests/data/valid/large_object.json`，366 字节

| 构建 | 操作 | 迭代 | 总耗时（秒） | 吞吐量（MB/s） |
|---|---:|---:|---:|---:|
| bench | parse | 2000 | 0.0142516 | 48.9832 |
| bench | serialize | 2000 | 0.0035628 | 195.938 |
| bench-native | parse | 2000 | 0.0161654 | 43.1842 |
| bench-native | serialize | 2000 | 0.003436 | 203.169 |

解读：

- small nested 输入的 native parse 和 serialize 略快，
  但单次数据不足以证明稳定收益。
- large object 的 native serialize 略快，
  但 native parse 反而比普通 Release 慢约 12%。
- native 编译标志不能保证每个负载都变快；
  短输入上的调度、缓存和启动噪声很明显。
- 输入只有几十到几百字节，仍不适合和大型 JSON 做横向比较。

## 6. 当前瓶颈

当前实现的主要开销来自：

1. 每个节点通过 `view` 单独分配。
2. 字符串解码写入 `std::string`，包括无转义路径也会复制。
3. 对象查找对所有成员线性扫描。
4. 数字访问每次重新转换词法片段。
5. 基准输入过小，固定开销主导测量结果。

前四项正是阶段 4 规划但尚未实现的优化。阶段 5 不能把这些
计划优化写成已完成性能成果。

## 7. 复现命令

普通测试：

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

普通性能基准：

```powershell
cmake --preset bench
cmake --build --preset bench
.\build\bench\jotjson_bench_parse.exe tests\data\valid\nested.json 5000
.\build\bench\jotjson_bench_serialize.exe tests\data\valid\nested.json 5000
```

native 实验基准：

```powershell
cmake --preset bench-native
cmake --build --preset bench-native
.\build\bench-native\jotjson_bench_parse.exe tests\data\valid\nested.json 5000
.\build\bench-native\jotjson_bench_serialize.exe tests\data\valid\nested.json 5000
```

模糊入口：

```powershell
cmake --preset fuzz
cmake --build --preset fuzz
.\build\fuzz\bin\jotjson_fuzz_parse.exe tests\data\invalid\bad_escape.json
```

Sanitizer：

```powershell
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
```

在 Linux 上，如果 GCC 或 Clang 提供 `libasan`、`libubsan`，
上面的命令应当能完成；Windows MinGW 需要预装对应运行时，
否则链接会明确报告缺少 `-lasan` 和 `-lubsan`。

## 8. 验收状态

| 项目 | 状态 |
|---|---|
| 固定合法/非法语料 | 已完成 |
| 语料回归测试 | 已完成并通过 |
| 解析基准 | 已完成 |
| 序列化基准 | 已完成 |
| Release / native 对比 | 已完成一次实测 |
| 普通模糊入口 | 已完成并通过固定语料 |
| libFuzzer 持续模糊测试 | 本机不支持，待 Linux CI |
| ASan / UBSan | 本机不支持，待 Linux CI |
| 分配次数 | 当前工具链不可信，未测 |
| 峰值内存 | 当前工具链不可信，未测 |
| 优化前后差异 | 阶段 4 未实施，待后续 |

## 9. 后续工作

1. 在 Linux CI 中运行 `asan` 预设，并至少执行一次短时 libFuzzer。
2. 加入更大的固定 JSON 语料，让吞吐量由解析本身主导。
3. 完成阶段 4 的 Arena、字符串视图、数字缓存和对象哈希。
4. 在同一台机器、同一编译器、同一语料上重新运行本报告。
5. 增加可信的分配计数和峰值内存来源，或明确使用外部测量工具。
