# PowerShell wrapper: build PS2 targets via ghcr.io/ps2dev/ps2dev
# Usage: .\Scripts\build-ps2-docker.ps1 [hello|lab|cube]
param(
    [ValidateSet("hello", "lab", "smoke", "cube")]
    [string]$Target = "cube"
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Image = if ($env:LEON_PS2DEV_IMAGE) { $env:LEON_PS2DEV_IMAGE } else { "ghcr.io/ps2dev/ps2dev:latest" }

docker run --rm `
  -v "${Root}:/src" `
  -w /src `
  $Image `
  sh /src/Scripts/docker-ps2-entry.sh $Target

exit $LASTEXITCODE
