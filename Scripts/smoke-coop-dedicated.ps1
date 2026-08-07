# Scripts/smoke-coop-dedicated.ps1 — CoopTp --dedicated must bind and reach Courtyard.
param(
    [int]$Port = 18001,
    [int]$WaitSeconds = 8
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$exeCandidates = @(
    (Join-Path $root "Projects\CoopTp\Shipping\leon-CoopTp.exe"),
    (Join-Path $root "Projects\CoopTp\build-fast\leon-CoopTp.exe")
)
$exe = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) {
    Write-Error "leon-CoopTp.exe not found. Build: Scripts\build-project.bat Projects\CoopTp"
    exit 1
}

$workDir = Split-Path -Parent $exe
$log = Join-Path $env:TEMP "leon-coop-dedicated-smoke.log"
Remove-Item $log -ErrorAction SilentlyContinue

Write-Host "Smoke: $exe --dedicated --port $Port --tick 5"
Write-Host "Cwd:   $workDir"

$arg = "--dedicated --port $Port --tick 5"
$proc = Start-Process -FilePath "cmd.exe" `
    -ArgumentList @("/c", "cd /d `"$workDir`" && `"$exe`" $arg > `"$log`" 2>&1") `
    -PassThru -WindowStyle Hidden

$deadline = (Get-Date).AddSeconds($WaitSeconds)
$out = ""
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 400
    if (Test-Path $log) {
        $out = Get-Content -Raw $log -ErrorAction SilentlyContinue
        if ($null -eq $out) { $out = "" }
        if ($out -match "CoopTp: level='Courtyard'" -and
            $out -match "(?i)DedicatedServer|dedicated server ready") {
            break
        }
    }
}

Get-Process -Name "leon-CoopTp" -ErrorAction SilentlyContinue | Stop-Process -Force
if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
Start-Sleep -Milliseconds 400

if (Test-Path $log) {
    $out = Get-Content -Raw $log -ErrorAction SilentlyContinue
    if ($null -eq $out) { $out = "" }
}

Write-Host "--- log ---"
Write-Host $out
Write-Host "--- end ---"

if ([string]::IsNullOrWhiteSpace($out)) {
    Write-Host "FAIL: empty log (is the exe headless-flushing stdout? rebuild CoopTp)"
    exit 2
}
if ($out -notmatch "Courtyard") {
    Write-Host "FAIL: Courtyard not reached"
    exit 1
}
if ($out -notmatch "(?i)DedicatedServer|dedicated server ready") {
    Write-Host "FAIL: dedicated host not ready"
    exit 1
}
if ($out -notmatch "CoopTp: level='Courtyard'") {
    Write-Host "FAIL: CoopTp Courtyard GameMode not entered"
    exit 1
}

Write-Host "OK: dedicated smoke - Courtyard"
exit 0
