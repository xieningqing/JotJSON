# [阶段 5 新增]
#
# 运行固定语料测试、模糊语料回归和 Release 基准。
# 所有输出同时写入 stdout 和日志文件，便于性能报告引用。
param(
    [string]$Configuration = "Release",
    [int]$Iterations = 2000
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $projectRoot

$logDirectory = Join-Path $projectRoot "logs"
$logPath = Join-Path $logDirectory "stage5-$Configuration.log"

New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
Set-Content -LiteralPath $logPath -Value "" -Encoding ASCII

function Write-Section {
    param([string]$Name)

    Write-Output "=== $Name ==="
    Add-Content -LiteralPath $logPath -Value "=== $Name ===" -Encoding ASCII
}

function Assert-LastExitCode {
    param([string]$Name)

    if ($LASTEXITCODE -ne 0) {
        throw "command failed: $Name"
    }
}

Write-Section "configure debug"
cmake --preset debug 2>&1 | Tee-Object -FilePath $logPath -Append
Assert-LastExitCode "configure debug"

Write-Section "build debug"
cmake --build --preset debug 2>&1 | Tee-Object -FilePath $logPath -Append
Assert-LastExitCode "build debug"

Write-Section "test debug"
ctest --preset debug --output-on-failure 2>&1 | Tee-Object -FilePath $logPath -Append
Assert-LastExitCode "test debug"

Write-Section "configure bench"
cmake --preset bench 2>&1 | Tee-Object -FilePath $logPath -Append
Assert-LastExitCode "configure bench"

Write-Section "build bench"
cmake --build --preset bench 2>&1 | Tee-Object -FilePath $logPath -Append
Assert-LastExitCode "build bench"

$benchDirectory = Join-Path $projectRoot "build\bench"
$parseBenchmark = Join-Path $benchDirectory "jotjson_bench_parse.exe"
$serializeBenchmark = Join-Path $benchDirectory "jotjson_bench_serialize.exe"

if (-not (Test-Path -LiteralPath $parseBenchmark)) {
    $parseBenchmark = Join-Path $benchDirectory "jotjson_bench_parse"
}

if (-not (Test-Path -LiteralPath $serializeBenchmark)) {
    $serializeBenchmark = Join-Path $benchDirectory "jotjson_bench_serialize"
}

$benchmarkFiles = @(
    "tests\data\valid\nested.json",
    "tests\data\valid\large_object.json",
    "tests\data\baseline_escape_heavy.json"
)

foreach ($relativePath in $benchmarkFiles) {
    $absolutePath = Join-Path $projectRoot $relativePath

    Write-Section "parse benchmark: $relativePath"
    & $parseBenchmark $absolutePath $Iterations 2>&1 |
        Tee-Object -FilePath $logPath -Append
    Assert-LastExitCode "parse benchmark: $relativePath"

    Write-Section "serialize benchmark: $relativePath"
    & $serializeBenchmark $absolutePath $Iterations 2>&1 |
        Tee-Object -FilePath $logPath -Append
    Assert-LastExitCode "serialize benchmark: $relativePath"
}

Write-Output "stage 5 log written to $logPath"
