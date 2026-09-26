# Builds and packages the project for its platforms (UE: RunUAT BuildCookRun -archive), called by the root Package.bat.
#   Win64: ShooterGame Shipping, built, cooked, staged and paked by BuildCookRun.ps1, copied to Packages\Win64\.
#   PS2:   ThirdPerson and TestPAL Development ELFs built in the pinned ps2dev image (Docker), with the config that
#          RunPCSX2.ps1 -StageOnly stages next to them, copied to Packages\PS2\<Name>\.
# -NoWin64 / -NoPS2 skip a platform. Packages\ is emptied for the platforms that are packaged and is git-ignored.
param(
	[switch]$NoWin64,
	[switch]$NoPS2
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$BatchFiles = Join-Path $Root "Engine\Build\BatchFiles"
$Packages = Join-Path $Root "Packages"

function Fail([string]$Message)
{
	Write-Host "Package: $Message" -ForegroundColor Red
	exit 1
}

function Step([string]$Message)
{
	Write-Host ""
	Write-Host "Package: $Message" -ForegroundColor Cyan
}

# Runs a batch file with arguments and fails the package when it fails.
function Invoke-Batch([string]$Batch, [string]$Arguments)
{
	& cmd /c "`"$Batch`" $Arguments"
	if ($LASTEXITCODE -ne 0) { Fail "$(Split-Path $Batch -Leaf) $Arguments failed ($LASTEXITCODE)" }
}

# Replaces Destination with a copy of the listed items of Source.
function Copy-Package([string]$Source, [string[]]$Items, [string]$Destination)
{
	if (Test-Path $Destination) { Remove-Item -Recurse -Force $Destination }
	New-Item -ItemType Directory -Force -Path $Destination | Out-Null
	foreach ($Item in $Items)
	{
		$Path = Join-Path $Source $Item
		if (-not (Test-Path $Path)) { Fail "missing $Path" }
		Copy-Item -Recurse -Force -Path $Path -Destination $Destination
	}
}

if ($NoWin64 -and $NoPS2) { Fail "nothing to package (-NoWin64 and -NoPS2)" }

Step "setup (pinned third-party downloads)"
Invoke-Batch (Join-Path $Root "Setup.bat") ""

if (-not $NoPS2 -and -not $env:PS2DEV)
{
	# Check Docker before the long Win64 build: the PS2 toolchain runs in the pinned ps2dev image.
	& docker info *> $null
	if ($LASTEXITCODE -ne 0) { Fail "Docker is not running: start Docker Desktop (Linux containers) for the PS2 build, or run Package.bat -NoPS2" }
}

if (-not $NoWin64)
{
	$ShooterGame = Join-Path $Root "Game\ShooterGame\ShooterGame.lproj"
	Step "Win64: ShooterGame Shipping (build, cook, stage, pak)"
	Invoke-Batch (Join-Path $BatchFiles "BuildCookRun.bat") "`"-project=$ShooterGame`" -platform=Win64 -configuration=Shipping -build -cook -stage -pak"
	$Staged = Join-Path $Root "Game\ShooterGame\Saved\StagedBuilds\Win64"
	Copy-Package $Staged @("*") (Join-Path $Packages "Win64")
}

if (-not $NoPS2)
{
	$ThirdPerson = Join-Path $Root "Game\ThirdPerson\ThirdPerson.lproj"
	$RunPCSX2 = Join-Path $Root "Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1"

	Step "PS2: ThirdPerson Development"
	Invoke-Batch (Join-Path $BatchFiles "Build.bat") "ThirdPerson PS2 Development `"-Project=$ThirdPerson`""
	& $RunPCSX2 -Project $ThirdPerson -StageOnly
	if ($LASTEXITCODE -ne 0) { Fail "staging ThirdPerson's config failed" }
	Copy-Package (Join-Path $Root "Game\ThirdPerson\Binaries\PS2") @("ThirdPerson.elf", "Engine", "ThirdPerson") (Join-Path $Packages "PS2\ThirdPerson")

	Step "PS2: TestPAL Development"
	Invoke-Batch (Join-Path $BatchFiles "Build.bat") "TestPAL PS2 Development"
	& $RunPCSX2 -Program TestPAL -StageOnly
	if ($LASTEXITCODE -ne 0) { Fail "staging TestPAL's config failed" }
	Copy-Package (Join-Path $Root "Engine\Binaries\PS2") @("TestPAL.elf", "Engine") (Join-Path $Packages "PS2\TestPAL")
}

Step "done: $Packages"
if (-not $NoWin64)
{
	Write-Host "  Win64: Packages\Win64\ShooterGame\Binaries\Win64\ShooterGame-Win64-Shipping.exe"
}
if (-not $NoPS2)
{
	Write-Host "  PS2:   Packages\PS2\ThirdPerson\ThirdPerson.elf and Packages\PS2\TestPAL\TestPAL.elf"
	Write-Host "         PCSX2: enable Settings > Advanced > Enable Host Filesystem (the staged config is read through host:),"
	Write-Host "         then boot the ELF (pcsx2-qt -fastboot -elf <file>). TestPAL prints its result to the EE log."
}
exit 0
