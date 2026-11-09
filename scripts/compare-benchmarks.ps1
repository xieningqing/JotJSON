# [阶段 5 新增]
#
# 比较两份 === parse benchmark: ... === 段落中的 throughput_mb_s。
# 只做直观比较，不作为统计显著性证明。
param(
    [Parameter(Mandatory = $true)]
    [string]$Before,

    [Parameter(Mandatory = $true)]
    [string]$After
)

$ErrorActionPreference = "Stop"

function Read-Benchmarks {
    param([string]$Path)

    $result = @{}
    $currentName = $null

    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -match '^=== (.+?) ===$') {
            $currentName = $Matches[1]
            continue
        }

        if ($line -match '^BENCH name=(\S+) .*file=(\S+) .*throughput_mb_s=([0-9.]+)') {
            $key = "$currentName|$($Matches[1])|$($Matches[2])"
            $result[$key] = [double]$Matches[3]
        }
    }

    return $result
}

$beforeResults = Read-Benchmarks $Before
$afterResults = Read-Benchmarks $After

foreach ($key in $beforeResults.Keys | Sort-Object) {
    if (-not $afterResults.ContainsKey($key)) {
        continue
    }

    $beforeValue = $beforeResults[$key]
    $afterValue = $afterResults[$key]
    $change = (($afterValue / $beforeValue) - 1.0) * 100.0

    "{0} : {1:N2} -> {2:N2} MB/s ({3:+0.00;-0.00;0.00}%)" -f `
        $key, $beforeValue, $afterValue, $change
}
