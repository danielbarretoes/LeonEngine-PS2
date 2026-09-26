# Engine\Build\BatchFiles\BuildCookRun.ps1 (called by BuildCookRun.bat; UE: RunUAT BuildCookRun).
#
#   BuildCookRun.bat -project=<Project>.lproj -platform=Win64|PS2 [-configuration=Shipping|Development]
#                    [-build] [-cook] [-stage] [-pak] [-run] [-addcmdline="<game arguments>"] [-align=<bytes>]
#
#   -build   builds the game in the configuration (default Shipping), LeonCook and LeonPak (Development)
#   -cook    LeonCook <Project>.lproj -run=Cook -TargetPlatform=<Platform>: <Project>\Saved\Cooked\<Platform>\
#   -stage   <Project>\Saved\StagedBuilds\<Platform>\ in UE's layout, emptied first:
#              <Project>\Binaries\<Platform>\<Project>[-<Platform>-<Configuration>].exe
#              <Project>\Content\Paks\<Project>-<Platform>.lpak      (-pak: the cooked folder, whole)
#   -pak     LeonPak over the cooked folder, with the pak paths ../../../Engine/... and ../../../<Project>/...
#   -run     starts the staged game with the -addcmdline arguments and returns its exit code
#
# The game is the project's own Game target (<Project>\Source\*.Target.cmake), or LeonGame for a content-only project
# (UE stages UE4Game renamed after the project).
#
# PS2 (Docs/PLANS/ps2-engine.md, E1): Development only, and no pak yet (it comes with the PS2 cook, E3). -build builds
# the ELF in Docker; -stage lays out what PCSX2's host: device serves, the cooked folder loose beside the ELF:
#   <Project>\Saved\StagedBuilds\PS2\<Project>.elf
#   <Project>\Saved\StagedBuilds\PS2\Engine\..., <Project>\...   (the cooked folder, whole)
#   <Project>\Saved\StagedBuilds\PS2\LeonCommandLine.txt         (-addcmdline: PCSX2 passes the ELF no arguments)
# and -run starts it in PCSX2 (RunPCSX2.ps1 -StagedElf) without waiting: the result is in the EE log.
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path

function Fail([string]$Message)
{
	Write-Host "BuildCookRun: ERROR: $Message"
	exit 1
}

function Step([string]$Message)
{
	Write-Host "BuildCookRun: $Message"
}

# Arguments: -name=value and -switch, case-insensitive (UE's style).
$Project = ""
$Platform = "Win64"
$Configuration = "Shipping"
$AddCmdLine = ""
$Align = ""
$Do = @{ build = $false; cook = $false; stage = $false; pak = $false; run = $false }
foreach ($Arg in $args)
{
	$Text = [string]$Arg
	if ($Text -match '^[-/]([A-Za-z]+)=(.*)$')
	{
		$Name = $Matches[1].ToLower()
		$Value = $Matches[2].Trim('"')
		switch ($Name)
		{
			"project" { $Project = $Value }
			"platform" { $Platform = $Value }
			"targetplatform" { $Platform = $Value }
			"configuration" { $Configuration = $Value }
			"clientconfig" { $Configuration = $Value }
			"addcmdline" { $AddCmdLine = $Value }
			"align" { $Align = $Value }
			default { Fail "unknown argument '$Text'" }
		}
	}
	elseif ($Text -match '^[-/]([A-Za-z]+)$' -and $Do.ContainsKey($Matches[1].ToLower()))
	{
		$Do[$Matches[1].ToLower()] = $true
	}
	else
	{
		Fail "unknown argument '$Text'"
	}
}
if ($Project -eq "") { Fail "-project=<Project>.lproj is required" }
if (-not (Test-Path $Project -PathType Leaf)) { Fail "no project file '$Project'" }
if ($Platform -notin @("Win64", "PS2")) { Fail "-platform=${Platform}: Win64 or PS2" }
if ($Configuration -notin @("Development", "Shipping")) { Fail "-configuration must be Development or Shipping" }
if (-not ($Do.build -or $Do.cook -or $Do.stage -or $Do.pak -or $Do.run)) { Fail "nothing to do: give -build, -cook, -stage, -pak and/or -run" }
$IsPS2 = $Platform -eq "PS2"
if ($IsPS2)
{
	# A Shipping game reads nothing but its paks, and the PS2 has none yet.
	if (-not ($args -match '^[-/](configuration|clientconfig)=')) { $Configuration = "Development" }
	if ($Configuration -ne "Development") { Fail "-platform=PS2 stages Development only until the PS2 pak (ps2-engine E3)" }
	if ($Do.pak) { Fail "-platform=PS2 has no pak yet (ps2-engine E3): -stage copies the cooked folder loose" }
}
elseif ($Do.stage -and -not $Do.pak) { Fail "-stage needs -pak: a staged build reads its content from its pak" }

$ProjectFile = (Resolve-Path $Project).Path
$ProjectDir = Split-Path $ProjectFile -Parent
$ProjectName = [IO.Path]::GetFileNameWithoutExtension($ProjectFile)
$CookedDir = Join-Path $ProjectDir "Saved\Cooked\$Platform"
$StageDir = Join-Path $ProjectDir "Saved\StagedBuilds\$Platform"
$Extension = if ($IsPS2) { ".elf" } else { ".exe" }
$ExeSuffix = if ($Configuration -eq "Development") { $Extension } else { "-$Platform-$Configuration$Extension" }
$BatchFiles = Join-Path $Root "Engine\Build\BatchFiles"
$EngineBinaries = Join-Path $Root "Engine\Binaries\$Platform"
# The cook and pak tools run on the development platform.
$ToolBinaries = Join-Path $Root "Engine\Binaries\Win64"

# The game target: the project's Game target, or LeonGame for a content-only project.
$GameTarget = "LeonGame"
$GameBinaries = $EngineBinaries
$BuildProjectArg = @()
$TargetFiles = @(Get-ChildItem -Path (Join-Path $ProjectDir "Source") -Filter "*.Target.cmake" -ErrorAction SilentlyContinue)
foreach ($TargetFile in $TargetFiles)
{
	$Match = Select-String -Path $TargetFile.FullName -Pattern 'leon_target\(\s*(\w+)\s+TYPE\s+Game' | Select-Object -First 1
	if ($Match)
	{
		$GameTarget = $Match.Matches[0].Groups[1].Value
		$GameBinaries = Join-Path $ProjectDir "Binaries\$Platform"
		$BuildProjectArg = @("-Project=$ProjectFile")
		break
	}
}
$GameExe = Join-Path $GameBinaries "$GameTarget$ExeSuffix"
Step "project $ProjectName ($ProjectFile), $Platform $Configuration, game target $GameTarget"

if ($Do.build)
{
	Step "build $GameTarget $Platform $Configuration"
	& "$BatchFiles\Build.bat" $GameTarget $Platform $Configuration @BuildProjectArg
	if ($LASTEXITCODE -ne 0) { Fail "building $GameTarget failed" }
	foreach ($Tool in @("LeonCook", "LeonPak"))
	{
		Step "build $Tool Win64 Development"
		& "$BatchFiles\Build.bat" $Tool Win64 Development
		if ($LASTEXITCODE -ne 0) { Fail "building $Tool failed" }
	}
}

if ($Do.cook)
{
	Step "cook for $Platform into $CookedDir"
	& (Join-Path $ToolBinaries "LeonCook.exe") $ProjectFile -run=Cook "-TargetPlatform=$Platform"
	if ($LASTEXITCODE -ne 0) { Fail "the cook failed" }
}

$StagedExe = if ($IsPS2) { Join-Path $StageDir "$ProjectName$ExeSuffix" } else { Join-Path $StageDir "$ProjectName\Binaries\$Platform\$ProjectName$ExeSuffix" }
if ($Do.stage)
{
	Step "stage into $StageDir"
	if (-not (Test-Path $GameExe)) { Fail "no game executable '$GameExe' (run with -build)" }
	if (-not (Test-Path $CookedDir)) { Fail "no cooked content in '$CookedDir' (run with -cook)" }
	if (Test-Path $StageDir) { Remove-Item -Recurse -Force -LiteralPath $StageDir }
	New-Item -ItemType Directory -Force (Split-Path $StagedExe -Parent) | Out-Null
	Copy-Item -LiteralPath $GameExe -Destination $StagedExe
	if ($IsPS2)
	{
		# host: is the ELF's folder: the PS2 FPaths finds Engine\ and <Project>\ there, loose.
		Copy-Item -Recurse -Force -Path (Join-Path $CookedDir "*") -Destination $StageDir
		if ($AddCmdLine -ne "")
		{
			Set-Content -Path (Join-Path $StageDir "LeonCommandLine.txt") -Value $AddCmdLine -Encoding ascii
		}
	}
}

if ($Do.pak)
{
	# One file per line, "<source>" "<path in the pak>", sorted: the pak paths start at the stage's root, which is
	# ../../../ from <Project>\Binaries\<Platform>\ (UE's mount point).
	if (-not (Test-Path $CookedDir)) { Fail "no cooked content in '$CookedDir' (run with -cook)" }
	$CookedRoot = (Resolve-Path $CookedDir).Path
	$Lines = Get-ChildItem -Recurse -File -LiteralPath $CookedRoot | ForEach-Object {
		$Relative = $_.FullName.Substring($CookedRoot.Length + 1).Replace([char]92, [char]47)
		$Source = $_.FullName.Replace([char]92, [char]47)
		"`"$Source`" `"../../../$Relative`""
	} | Sort-Object -Culture ([Globalization.CultureInfo]::InvariantCulture)
	$ResponseFile = Join-Path $ProjectDir "Saved\Cooked\PakList_$ProjectName-$Platform.txt"
	Set-Content -Path $ResponseFile -Value $Lines -Encoding ascii
	$PakFile = Join-Path $StageDir "$ProjectName\Content\Paks\$ProjectName-$Platform.lpak"
	New-Item -ItemType Directory -Force (Split-Path $PakFile -Parent) | Out-Null
	$PakArgs = @($PakFile, "-create=$ResponseFile")
	if ($Align -ne "") { $PakArgs += "-align=$Align" }
	Step "pak $($Lines.Count) files into $PakFile"
	& (Join-Path $ToolBinaries "LeonPak.exe") @PakArgs
	if ($LASTEXITCODE -ne 0) { Fail "LeonPak failed" }
}

if ($Do.run -and $IsPS2)
{
	if (-not (Test-Path $StagedExe)) { Fail "no staged game '$StagedExe' (run with -stage)" }
	# The arguments were staged in LeonCommandLine.txt; PCSX2 runs detached.
	Step "run $StagedExe in PCSX2 (the result is in the EE log)"
	& (Join-Path $Root "Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1") -StagedElf $StagedExe
	Step "done"
	exit 0
}

if ($Do.run)
{
	if (-not (Test-Path $StagedExe)) { Fail "no staged game '$StagedExe' (run with -stage -pak)" }
	# The game's arguments, split on spaces outside double quotes.
	$GameArgs = @([regex]::Matches($AddCmdLine, '"[^"]*"|\S+') | ForEach-Object { $_.Value.Trim('"') })
	Step "run $StagedExe $($GameArgs -join ' ')"
	& $StagedExe @GameArgs
	$ExitCode = $LASTEXITCODE
	Step "the game exited with $ExitCode"
	if ($ExitCode -ne 0) { exit $ExitCode }
}
Step "done"
exit 0
