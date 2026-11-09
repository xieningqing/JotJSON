# 阶段 5 验证入口

## 普通测试

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

包含：

- 阶段 1～3 基础、序列化和严格语义测试。
- 阶段 4 基线测试。
- 阶段 5 固定合法/非法语料测试。

## 一次性运行阶段 5

```powershell
.\scripts\run-stage5.ps1 -Iterations 2000
```

日志会写到 `logs/stage5-Release.log`。

## 基准

```powershell
cmake --preset bench
cmake --build --preset bench
.\build\bench\jotjson_bench_parse.exe tests\data\valid\large_object.json 2000
.\build\bench\jotjson_bench_serialize.exe tests\data\valid\large_object.json 2000
```

native 只用于实验，不进入普通发布构建：

```powershell
cmake --preset bench-native
cmake --build --preset bench-native
```

## 模糊入口

无 libFuzzer 时，入口是单文件执行程序：

```powershell
cmake --preset fuzz
cmake --build --preset fuzz
.\build\fuzz\bin\jotjson_fuzz_parse.exe tests\data\invalid\invalid_utf8.bin
```

在 Clang 环境启用 libFuzzer：

```powershell
cmake -S . -B build/fuzz-libfuzzer `
    -G Ninja `
    -DCMAKE_BUILD_TYPE=Debug `
    -DJOTJSON_BUILD_TESTS=OFF `
    -DJOTJSON_BUILD_EXAMPLES=OFF `
    -DJOTJSON_BUILD_CLI=OFF `
    -DJOTJSON_BUILD_FUZZER=ON `
    -DJOTJSON_USE_LIBFUZZER=ON

cmake --build build/fuzz-libfuzzer

.\build\fuzz-libfuzzer\bin\jotjson_fuzz_parse.exe `
    tests\data\valid `
    -max_total_time=60
```

`JOTJSON_USE_LIBFUZZER` 只适用于支持 `-fsanitize=fuzzer` 的编译器。

## Sanitizer

```powershell
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
```

如果运行时库缺失，链接会失败。必须记录为“未验证”，
不能把配置成功当成 Sanitizer 已通过。
