# Builds and packages the project for its platforms (UE: RunUAT BuildCookRun -archive), called by the root Package.bat.
#   Win64: ShooterGame Shipping, built, cooked, staged and paked by BuildCookRun.ps1, copied to Packages\Win64\.
#   PS2:   ThirdPerson, TestPAL and GSConformance Development ELFs built in the pinned ps2dev image (Docker), with the
#          config that RunPCSX2.ps1 -StageOnly stages next to them, and ShooterGame staged by BuildCookRun with its
#          PS2 cook in a pak (the game, played with the pad; -LogFrameTimes), copied to Packages\PS2\<Name>\.
# -NoWin64 / -NoPS2 skip a platform. Packages\<Platform>\ is emptied for each platform that is packaged and is git-ignored.
#
# PS2 packaging origins (one Publish-PS2Package path for every artifact):
#   Dev-loop (RunPCSX2 -StageOnly beside Binaries\PS2): ThirdPerson, TestPAL; GSConformance has no config to stage.
#   BuildCookRun StagedBuilds\PS2: ShooterGame (ELF at the stage root + pak / LeonCommandLine.txt).
param(
	[switch]$NoWin64,
	[switch]$NoPS2
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$BatchFiles = Join-Path $Root "Engine\Build\BatchFiles"
$Packages = Join-Path $Root "Packages"
$RunPCSX2 = Join-Path $Root "Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1"

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

# Empties Packages\<Platform> so a partial previous run cannot leave stale artifacts.
function Clear-PlatformPackages([string]$Platform)
{
	$Dir = Join-Path $Packages $Platform
	if (Test-Path $Dir) { Remove-Item -Recurse -Force $Dir }
	New-Item -ItemType Directory -Force -Path $Dir | Out-Null
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

# Single publish path for every PS2 artifact: Packages\PS2\<Name>\ gets Items from SourceDir.
function Publish-PS2Package([string]$Name, [string]$SourceDir, [string[]]$Items)
{
	Copy-Package $SourceDir $Items (Join-Path $Packages "PS2\$Name")
}

# Dev-loop PS2: Build.bat, optional RunPCSX2 -StageOnly (config beside the ELF), then Publish-PS2Package.
# StageOnlySplat is splatted into RunPCSX2.ps1 (e.g. @{ Project = $Path; StageOnly = $true }).
function Publish-PS2DevLoopArtifact(
	[string]$Name,
	[string]$BuildArgs,
	[string]$SourceDir,
	[string[]]$Items,
	[hashtable]$StageOnlySplat = $null
)
{
	Invoke-Batch (Join-Path $BatchFiles "Build.bat") $BuildArgs
	if ($null -ne $StageOnlySplat)
	{
		& $RunPCSX2 @StageOnlySplat
		if ($LASTEXITCODE -ne 0) { Fail "staging ${Name}'s config failed" }
	}
	Publish-PS2Package $Name $SourceDir $Items
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
	Clear-PlatformPackages "Win64"
	$ShooterGame = Join-Path $Root "Game\ShooterGame\ShooterGame.lproj"
	Step "Win64: ShooterGame Shipping (build, cook, stage, pak)"
	Invoke-Batch (Join-Path $BatchFiles "BuildCookRun.bat") "`"-project=$ShooterGame`" -platform=Win64 -configuration=Shipping -build -cook -stage -pak"
	$Staged = Join-Path $Root "Game\ShooterGame\Saved\StagedBuilds\Win64"
	Copy-Package $Staged @("*") (Join-Path $Packages "Win64")
}

if (-not $NoPS2)
{
	Clear-PlatformPackages "PS2"
	$ThirdPerson = Join-Path $Root "Game\ThirdPerson\ThirdPerson.lproj"
	$ShooterGame = Join-Path $Root "Game\ShooterGame\ShooterGame.lproj"

	Step "PS2: ThirdPerson Development"
	Publish-PS2DevLoopArtifact -Name "ThirdPerson" `
		-BuildArgs "ThirdPerson PS2 Development `"-Project=$ThirdPerson`"" `
		-SourceDir (Join-Path $Root "Game\ThirdPerson\Binaries\PS2") `
		-Items @("ThirdPerson.elf", "Engine", "ThirdPerson") `
		-StageOnlySplat @{ Project = $ThirdPerson; StageOnly = $true }

	Step "PS2: TestPAL Development"
	Publish-PS2DevLoopArtifact -Name "TestPAL" `
		-BuildArgs "TestPAL PS2 Development" `
		-SourceDir (Join-Path $Root "Engine\Binaries\PS2") `
		-Items @("TestPAL.elf", "Engine") `
		-StageOnlySplat @{ Program = "TestPAL"; StageOnly = $true }

	# ShooterGame on the EE (Docs/PLANS/ps2-engine.md, E1 to E4): BuildCookRun stages the ELF at the stage root with
	# its PS2 cook in a pak and -LogFrameTimes in LeonCommandLine.txt.
	Step "PS2: ShooterGame Development (the game, with its pak)"
	Invoke-Batch (Join-Path $BatchFiles "BuildCookRun.bat") "`"-project=$ShooterGame`" -platform=PS2 -build -cook -stage -pak `"-addcmdline=-LogFrameTimes`""
	Publish-PS2Package "ShooterGame" (Join-Path $Root "Game\ShooterGame\Saved\StagedBuilds\PS2") @("*")

	# The GS conformance scenes (Docs/PLANS/ps2-gs-parity.md): no config to stage.
	Step "PS2: GSConformance Development"
	Publish-PS2DevLoopArtifact -Name "GSConformance" `
		-BuildArgs "GSConformance PS2 Development" `
		-SourceDir (Join-Path $Root "Engine\Binaries\PS2") `
		-Items @("GSConformance.elf")
}

Step "done: $Packages"
if (-not $NoWin64)
{
	Write-Host "  Win64: Packages\Win64\ShooterGame\Binaries\Win64\ShooterGame-Win64-Shipping.exe"
}
if (-not $NoPS2)
{
	Write-Host "  PS2:   Packages\PS2\ThirdPerson\ThirdPerson.elf, Packages\PS2\TestPAL\TestPAL.elf,"
	Write-Host "         Packages\PS2\GSConformance\GSConformance.elf and Packages\PS2\ShooterGame\ShooterGame.elf"
	Write-Host "         (ShooterGame: de_leon against nine bots, played with the pad; its frame times are in the EE log)"
	Write-Host "         PCSX2: enable Settings > Advanced > Enable Host Filesystem (the staged config is read through host:),"
	Write-Host "         then boot the ELF (pcsx2-qt -fastboot -elf <file>). TestPAL prints its result to the EE log."
}
exit 0
