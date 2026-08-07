# Normalize compile_commands.json so clangd can parse MSVC/Ninja commands on Windows.
# CMake emits `command` strings with backslash paths (`-IC:\Users\...`). clangd's
# command-line lexer treats `\U` in `\Users` as an escape; forward slashes fix that.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$targets = @(
    (Join-Path $repo "Editor\build-ninja\compile_commands.json"),
    (Join-Path $repo "Tools\build-ninja\compile_commands.json"),
    (Join-Path $repo "Editor\build-fast\compile_commands.json")
)
if ($args.Count -gt 0) {
    $targets = $args
}

function Fix-CompileCommands([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Write-Host "skip (missing): $path"
        return 0
    }
    $raw = Get-Content -LiteralPath $path -Raw -Encoding UTF8
    $data = $raw | ConvertFrom-Json
    $changed = 0
    foreach ($entry in $data) {
        if ($null -ne $entry.command) {
            $fixed = [string]$entry.command -replace '\\', '/'
            if ($fixed -ne $entry.command) {
                $entry.command = $fixed
                $changed++
            }
        }
        if ($null -ne $entry.arguments) {
            $newArgs = @()
            $argChanged = $false
            foreach ($a in $entry.arguments) {
                if ($a -is [string]) {
                    $fa = $a -replace '\\', '/'
                    if ($fa -ne $a) { $argChanged = $true }
                    $newArgs += $fa
                } else {
                    $newArgs += $a
                }
            }
            if ($argChanged) {
                $entry.arguments = $newArgs
                $changed++
            }
        }
        if ($null -ne $entry.directory) {
            $entry.directory = [string]$entry.directory -replace '\\', '/'
        }
        if ($null -ne $entry.file) {
            $entry.file = [string]$entry.file -replace '\\', '/'
        }
    }
    $json = $data | ConvertTo-Json -Depth 100
    # PowerShell wraps single-object arrays; ensure array root when one entry.
    if ($data -isnot [System.Array]) {
        $json = "[$json]"
    }
    [System.IO.File]::WriteAllText($path, $json + "`n", [System.Text.UTF8Encoding]::new($false))
    Write-Host "fixed $changed entries in $path"
    return $changed
}

$total = 0
foreach ($t in $targets) {
    $total += Fix-CompileCommands $t
}

$primary = Join-Path $repo "Editor\build-ninja\compile_commands.json"
if (Test-Path -LiteralPath $primary -PathType Leaf) {
    $rootCc = Join-Path $repo "compile_commands.json"
    Copy-Item -LiteralPath $primary -Destination $rootCc -Force
    Write-Host "copied -> $rootCc"
}
exit 0
