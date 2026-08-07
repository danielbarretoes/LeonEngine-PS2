# Scripts/smoke-packs.ps1 — optional pack dedicated smoke (CoopTp / Zombies / Furytoon).
# Usage: Scripts\smoke-packs.ps1 [-Pack CoopTp] [-Port 18001]
param(
    [ValidateSet("CoopTp", "Zombies", "Furytoon", "All")]
    [string]$Pack = "All",
    [int]$Port = 18001,
    [int]$WaitSeconds = 8
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Smoke-OnePack {
    param(
        [string]$Name,
        [string]$ExeBase,
        [string]$LogMatch,
        [int]$UsePort
    )
    $exeCandidates = @(
        (Join-Path $root "Projects\$Name\Shipping\$ExeBase.exe"),
        (Join-Path $root "Projects\$Name\build-fast\$ExeBase.exe")
    )
    $exe = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $exe) {
        Write-Warning "$ExeBase.exe not found for $Name — skip (build: Scripts\build-project.bat Projects\$Name --with-server)"
        return $false
    }
    $workDir = Split-Path -Parent $exe
    $log = Join-Path $env:TEMP "leon-$Name-dedicated-smoke.log"
    Remove-Item $log -ErrorAction SilentlyContinue
    Write-Host "Smoke: $exe --dedicated --port $UsePort --tick 5"
    $arg = "--dedicated --port $UsePort --tick 5"
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
            if ($out -match $LogMatch -and $out -match "(?i)DedicatedServer|dedicated server ready") {
                break
            }
        }
    }
    Get-Process -Name $ExeBase -ErrorAction SilentlyContinue | Stop-Process -Force
    if (-not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Milliseconds 300
    if (Test-Path $log) {
        $out = Get-Content -Raw $log -ErrorAction SilentlyContinue
        if ($null -eq $out) { $out = "" }
    }
    $ok = ($out -match $LogMatch)
    if ($ok) {
        Write-Host "PASS $Name"
    } else {
        Write-Host "FAIL $Name"
        Write-Host $out
    }
    return $ok
}

$packs = @()
if ($Pack -eq "All") {
    $packs = @(
        @{ Name = "CoopTp"; Exe = "leon-CoopTp"; Match = "CoopTp: level="; Port = $Port },
        @{ Name = "Zombies"; Exe = "leon-Zombies"; Match = "Zombies:"; Port = ($Port + 1) },
        @{ Name = "Furytoon"; Exe = "leon-Furytoon"; Match = "Furytoon:"; Port = ($Port + 2) }
    )
} else {
    $map = @{
        CoopTp = @{ Exe = "leon-CoopTp"; Match = "CoopTp: level=" }
        Zombies = @{ Exe = "leon-Zombies"; Match = "Zombies:" }
        Furytoon = @{ Exe = "leon-Furytoon"; Match = "Furytoon:" }
    }
    $packs = @(@{ Name = $Pack; Exe = $map[$Pack].Exe; Match = $map[$Pack].Match; Port = $Port })
}

$failed = 0
foreach ($p in $packs) {
    if (-not (Smoke-OnePack -Name $p.Name -ExeBase $p.Exe -LogMatch $p.Match -UsePort $p.Port)) {
        $failed++
    }
}
if ($failed -gt 0) {
    Write-Error "$failed pack smoke(s) failed"
    exit 1
}
Write-Host "All requested pack smokes passed (or skipped missing builds)."
exit 0
