# Engine\Build\BatchFiles\BuildCookRun.ps1 (called by BuildCookRun.bat; UE: RunUAT BuildCookRun).
#
#   BuildCookRun.bat -project=<Project>.lproj -platform=Win64 [-configuration=Shipping|Development]
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
# (UE stages UE4Game renamed after the project). Only Win64 stages today; the PS2 comes with the Engine port.
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
if ($Platform -ne "Win64") { Fail "-platform=${Platform}: only Win64 stages today (the PS2 build comes with the Engine port)" }
if ($Configuration -notin @("Development", "Shipping")) { Fail "-configuration must be Development or Shipping" }
if (-not ($Do.build -or $Do.cook -or $Do.stage -or $Do.pak -or $Do.run)) { Fail "nothing to do: give -build, -cook, -stage, -pak and/or -run" }
if ($Do.stage -and -not $Do.pak) { Fail "-stage needs -pak: a staged build reads its content from its pak" }

$ProjectFile = (Resolve-Path $Project).Path
$ProjectDir = Split-Path $ProjectFile -Parent
$ProjectName = [IO.Path]::GetFileNameWithoutExtension($ProjectFile)
$CookedDir = Join-Path $ProjectDir "Saved\Cooked\$Platform"
$StageDir = Join-Path $ProjectDir "Saved\StagedBuilds\$Platform"
$ExeSuffix = if ($Configuration -eq "Development") { ".exe" } else { "-$Platform-$Configuration.exe" }
$BatchFiles = Join-Path $Root "Engine\Build\BatchFiles"
$EngineBinaries = Join-Path $Root "Engine\Binaries\$Platform"

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
	& (Join-Path $EngineBinaries "LeonCook.exe") $ProjectFile -run=Cook "-TargetPlatform=$Platform"
	if ($LASTEXITCODE -ne 0) { Fail "the cook failed" }
}

$StagedExe = Join-Path $StageDir "$ProjectName\Binaries\$Platform\$ProjectName$ExeSuffix"
if ($Do.stage)
{
	Step "stage into $StageDir"
	if (-not (Test-Path $GameExe)) { Fail "no game executable '$GameExe' (run with -build)" }
	if (-not (Test-Path $CookedDir)) { Fail "no cooked content in '$CookedDir' (run with -cook)" }
	if (Test-Path $StageDir) { Remove-Item -Recurse -Force -LiteralPath $StageDir }
	New-Item -ItemType Directory -Force (Split-Path $StagedExe -Parent) | Out-Null
	Copy-Item -LiteralPath $GameExe -Destination $StagedExe
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
	& (Join-Path $EngineBinaries "LeonPak.exe") @PakArgs
	if ($LASTEXITCODE -ne 0) { Fail "LeonPak failed" }
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
