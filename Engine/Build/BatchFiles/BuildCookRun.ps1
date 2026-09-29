# Engine\Build\BatchFiles\BuildCookRun.ps1 (called by BuildCookRun.bat; UE: RunUAT BuildCookRun).
#
#   BuildCookRun.bat -project=<Project>.lproj -platform=Win64|PS2 [-configuration=Shipping|Development]
#                    [-build] [-cook] [-stage] [-pak] [-iso] [-run] [-addcmdline="<game arguments>"] [-align=<bytes>]
#                    [-pakorder=<order file>] [-fullcook] [-region=NTSC|PAL] [-discserial=<SLUS_XXX.XX>]
#
#   -build   builds the game in the configuration (default Shipping), LeonCook and LeonPak (Development)
#   -cook    LeonCook <Project>.lproj -run=Cook -TargetPlatform=<Platform>: <Project>\Saved\Cooked\<Platform>\
#            (incremental through <Project>\Intermediate\CookCache; -fullcook cooks every package again)
#   -stage   <Project>\Saved\StagedBuilds\<Platform>\ in UE's layout, emptied first:
#              <Project>\Binaries\<Platform>\<Project>[-<Platform>-<Configuration>].exe
#              <Project>\Content\Paks\<Project>-<Platform>.lpak      (-pak: the cooked folder, whole)
#   -pak     LeonPak over the cooked folder, with the pak paths ../../../Engine/... and ../../../<Project>/...;
#            -pakorder=<file> lays the entries out in the order a -LogFileOpenOrder run opened them (its
#            Saved\Logs\FileOpenOrder-<Platform>.txt, or a PS2 EE log), the rest after (LeonPak -order=)
#   -run     starts the staged game with the -addcmdline arguments and returns its exit code
#
# The game is the project's own Game target (<Project>\Source\*.Target.cmake), or LeonGame for a content-only project
# (UE stages UE4Game renamed after the project).
#
# PS2 (Docs/PLANS/ps2-engine.md, E1 and E3): Development only. -build builds the ELF in Docker; the cook makes the
# textures paletted (PSMT8 / PSMT4) and writes <Project>\Saved\Cooked\PS2-VramReport.txt; -stage lays out what
# PCSX2's host: device serves:
#   <Project>\Saved\StagedBuilds\PS2\<Project>.elf
#   <Project>\Saved\StagedBuilds\PS2\audsrv.irx                  (the IOP module the audio loads; from the ps2dev SDK)
#   <Project>\Saved\StagedBuilds\PS2\Engine\..., <Project>\...   (without -pak: the cooked folder, loose)
#   <Project>\Saved\StagedBuilds\PS2\<Project>\Content\Paks\<Project>-PS2.lpak
#                                                             (-pak: the cooked folder with the paths Engine/... and
#                                                              <Project>/..., mounted at the ELF's folder, entries
#                                                              aligned to 2048 bytes unless -align says otherwise)
#   <Project>\Saved\StagedBuilds\PS2\LeonCommandLine.txt         (-addcmdline: PCSX2 passes the ELF no arguments)
# and -run starts it in PCSX2 (RunPCSX2.ps1 -StagedElf) without waiting: the result is in the EE log.
#
# PS2 -iso (Docs/PLANS/ps2-shipping.md N23): the staged ELF and pak as a bootable disc image,
# <Project>\Saved\StagedBuilds\PS2\<Project>.iso, made by xorriso in the PS2 build image (ISO 9660 level 1, system
# PLAYSTATION). On the disc, in this order: SYSTEM.CNF (BOOT2 = cdrom0:\<serial>;1, VER, VMODE of -region), the ELF
# as <serial> (an 8.3 name: SLUS_990.01 for NTSC, SLES_990.01 for PAL, or -discserial=), the pak, then the IOP modules
# and LeonCommandLine.txt (-addcmdline); every name as FPaths::ToIso9660Path makes it, which is how the game asks
# cdrom0: for its files (the pak: SHOOTERG\CONTENT\PAKS\SHOOTERG.LPA). Deterministic: the dates are fixed.
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
$PakOrder = ""
$Region = "NTSC"
$DiscSerial = ""
$Do = @{ build = $false; cook = $false; stage = $false; pak = $false; iso = $false; run = $false; fullcook = $false }
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
			"pakorder" { $PakOrder = $Value }
			"region" { $Region = $Value.ToUpper() }
			"discserial" { $DiscSerial = $Value.ToUpper() }
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
if (-not ($Do.build -or $Do.cook -or $Do.stage -or $Do.pak -or $Do.iso -or $Do.run)) { Fail "nothing to do: give -build, -cook, -stage, -pak, -iso and/or -run" }
$IsPS2 = $Platform -eq "PS2"
if ($Do.iso -and -not $IsPS2) { Fail "-iso makes a PS2 disc: give -platform=PS2" }
if ($Region -notin @("NTSC", "PAL")) { Fail "-region=${Region}: NTSC or PAL" }
if ($PakOrder -ne "")
{
	if (-not (Test-Path $PakOrder -PathType Leaf)) { Fail "no pak order file '$PakOrder'" }
	$PakOrder = (Resolve-Path $PakOrder).Path
}
if ($IsPS2)
{
	# Development only: the PS2 Shipping game is not built yet (ps2-engine E6). Without -pak, -stage copies the cooked
	# folder loose beside the ELF; with it, the content goes in one pak aligned to the disc's 2048-byte sectors.
	if (-not ($args -match '^[-/](configuration|clientconfig)=')) { $Configuration = "Development" }
	if ($Configuration -ne "Development") { Fail "-platform=PS2 stages Development only" }
	if ($Do.pak -and $Align -eq "") { $Align = "2048" }
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
	$CookArgs = @($ProjectFile, "-run=Cook", "-TargetPlatform=$Platform")
	if ($Do.fullcook) { $CookArgs += "-full" }
	& (Join-Path $ToolBinaries "LeonCook.exe") @CookArgs
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
		# The IOP modules the build put beside the ELF (the modules' RUNTIME_DEPENDENCIES: audsrv.irx) go with it.
		Get-ChildItem -Path (Split-Path $GameExe -Parent) -Filter "*.irx" -File -ErrorAction SilentlyContinue |
			ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $StageDir }
		# host: is the ELF's folder: the PS2 FPaths finds Engine\ and <Project>\ there, loose unless they are paked.
		if (-not $Do.pak) { Copy-Item -Recurse -Force -Path (Join-Path $CookedDir "*") -Destination $StageDir }
		if ($AddCmdLine -ne "")
		{
			Set-Content -Path (Join-Path $StageDir "LeonCommandLine.txt") -Value $AddCmdLine -Encoding ascii
		}
	}
}

if ($Do.pak)
{
	# One file per line, "<source>" "<path in the pak>", sorted: the pak paths start at the stage's root, which is
	# ../../../ from <Project>\Binaries\<Platform>\ (UE's mount point), and the PS2 ELF's own folder.
	if (-not (Test-Path $CookedDir)) { Fail "no cooked content in '$CookedDir' (run with -cook)" }
	$CookedRoot = (Resolve-Path $CookedDir).Path
	$PakPrefix = if ($IsPS2) { "" } else { "../../../" }
	$Lines = Get-ChildItem -Recurse -File -LiteralPath $CookedRoot | ForEach-Object {
		$Relative = $_.FullName.Substring($CookedRoot.Length + 1).Replace([char]92, [char]47)
		$Source = $_.FullName.Replace([char]92, [char]47)
		"`"$Source`" `"$PakPrefix$Relative`""
	} | Sort-Object -Culture ([Globalization.CultureInfo]::InvariantCulture)
	$ResponseFile = Join-Path $ProjectDir "Saved\Cooked\PakList_$ProjectName-$Platform.txt"
	Set-Content -Path $ResponseFile -Value $Lines -Encoding ascii
	$PakFile = Join-Path $StageDir "$ProjectName\Content\Paks\$ProjectName-$Platform.lpak"
	New-Item -ItemType Directory -Force (Split-Path $PakFile -Parent) | Out-Null
	$PakArgs = @($PakFile, "-create=$ResponseFile")
	if ($Align -ne "") { $PakArgs += "-align=$Align" }
	if ($PakOrder -ne "") { $PakArgs += "-order=$PakOrder" }
	Step "pak $($Lines.Count) files into $PakFile"
	& (Join-Path $ToolBinaries "LeonPak.exe") @PakArgs
	if ($LASTEXITCODE -ne 0) { Fail "LeonPak failed" }
}

# ISO 9660 level 1 names, as FPaths::ToIso9660Path makes them (the game asks cdrom0: for them the same way).
function Get-IsoPath([string]$Path)
{
	$Parts = @($Path.Replace([char]92, [char]47).Split([char]47) | Where-Object { $_ -ne "" })
	$ToId = { param([string]$Text, [int]$Max) $Id = ($Text.ToUpperInvariant() -replace "[^A-Z0-9_]", "_"); $Id.Substring(0, [Math]::Min($Max, $Id.Length)) }
	$Names = for ($Index = 0; $Index -lt $Parts.Count; $Index++)
	{
		$Name = $Parts[$Index]
		$Dot = if ($Index -eq $Parts.Count - 1) { $Name.LastIndexOf(".") } else { -1 }
		if ($Dot -ge 0) { (& $ToId $Name.Substring(0, $Dot) 8) + "." + (& $ToId $Name.Substring($Dot + 1) 3) } else { & $ToId $Name 8 }
	}
	return ($Names -join "\")
}

if ($Do.iso)
{
	$StagedPak = Join-Path $StageDir "$ProjectName\Content\Paks\$ProjectName-$Platform.lpak"
	if (-not (Test-Path $StagedExe)) { Fail "no staged game '$StagedExe' (run with -stage)" }
	if (-not (Test-Path $StagedPak)) { Fail "no staged pak '$StagedPak' (run with -stage -pak)" }
	if ($DiscSerial -eq "") { $DiscSerial = if ($Region -eq "PAL") { "SLES_990.01" } else { "SLUS_990.01" } }
	if ($DiscSerial -notmatch '^[A-Z]{4}_[0-9]{3}\.[0-9]{2}$') { Fail "-discserial=${DiscSerial}: four letters, '_', three digits, '.', two digits (SLUS_990.01)" }

	# The disc's tree, in <Project>\Intermediate\PS2Disc\, and the order xorriso lays it out in (a higher weight first).
	$WorkDir = Join-Path $ProjectDir "Intermediate"
	$DiscDir = Join-Path $WorkDir "PS2Disc"
	if (Test-Path $DiscDir) { Remove-Item -Recurse -Force -LiteralPath $DiscDir }
	New-Item -ItemType Directory -Force $DiscDir | Out-Null
	$SystemCnf = "BOOT2 = cdrom0:\$DiscSerial;1`r`nVER = 1.00`r`nVMODE = $Region`r`n"
	[System.IO.File]::WriteAllText((Join-Path $DiscDir "SYSTEM.CNF"), $SystemCnf, [System.Text.Encoding]::ASCII)
	Copy-Item -LiteralPath $StagedExe -Destination (Join-Path $DiscDir $DiscSerial)
	$PakIsoPath = Get-IsoPath "$ProjectName/Content/Paks/$ProjectName-$Platform.lpak"
	New-Item -ItemType Directory -Force (Split-Path (Join-Path $DiscDir $PakIsoPath) -Parent) | Out-Null
	Copy-Item -LiteralPath $StagedPak -Destination (Join-Path $DiscDir $PakIsoPath)
	$Weights = @("4 /SYSTEM.CNF", "3 /$DiscSerial", "2 /$($PakIsoPath.Replace([char]92, [char]47))")
	Get-ChildItem -LiteralPath $StageDir -File | Where-Object { $_.Extension -eq ".irx" -or $_.Name -eq "LeonCommandLine.txt" } |
		Sort-Object Name | ForEach-Object {
			$IsoFileName = Get-IsoPath $_.Name
			Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $DiscDir $IsoFileName)
			$Weights += "1 /$IsoFileName"
		}
	$SortFile = Join-Path $WorkDir "PS2DiscSort.txt"
	[System.IO.File]::WriteAllText($SortFile, ($Weights -join "`n") + "`n", [System.Text.Encoding]::ASCII)

	# The PS2 build image (with xorriso), tagged as LeonBuildTool tags it: leon/ps2-build:<hash of the Dockerfile and
	# the pinned base image>, built here when it is missing.
	$DockerFile = Join-Path $Root "Engine\Platforms\PS2\Build\Docker\Dockerfile"
	$PlatformRules = Join-Path $Root "Engine\Platforms\PS2\Source\Programs\LeonBuildTool\LeonBuildPS2.cmake"
	$BaseImage = (Select-String -Path $PlatformRules -Pattern 'DOCKER_IMAGE\s+"([^"]+)"' | Select-Object -First 1).Matches[0].Groups[1].Value
	$FileHash = (Get-FileHash -LiteralPath $DockerFile -Algorithm SHA256).Hash.ToLowerInvariant()
	$Sha = [System.Security.Cryptography.SHA256]::Create()
	$DerivedHash = (($Sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes("$FileHash$BaseImage")) | ForEach-Object { $_.ToString("x2") }) -join "").Substring(0, 12)
	$Image = "leon/ps2-build:$DerivedHash"
	& docker image inspect $Image *> $null
	if ($LASTEXITCODE -ne 0)
	{
		Step "build the PS2 build image $Image (once)"
		& docker build -t $Image --build-arg "BASE_IMAGE=$BaseImage" -f $DockerFile (Split-Path $DockerFile -Parent)
		if ($LASTEXITCODE -ne 0) { Fail "the PS2 build image could not be built" }
	}

	$IsoName = "$ProjectName.iso"
	$VolumeId = ($ProjectName.ToUpperInvariant() -replace "[^A-Z0-9_]", "_")
	$VolumeId = $VolumeId.Substring(0, [Math]::Min(32, $VolumeId.Length))
	Step "iso $IsoName in $StageDir ($DiscSerial, $Region) with $Image"
	# Fixed dates (SOURCE_DATE_EPOCH, and every file's): the same stage gives the same bytes.
	$Script = "find /work/PS2Disc -exec touch -h -d @946684800 {} + && " +
		"xorriso -as mkisofs -quiet -iso-level 1 -sysid PLAYSTATION -V $VolumeId -A $VolumeId " +
		"--sort-weight-list /work/PS2DiscSort.txt -o /out/$IsoName /work/PS2Disc"
	& docker run --rm -e SOURCE_DATE_EPOCH=946684800 -v "${WorkDir}:/work" -v "${StageDir}:/out" $Image sh -c $Script
	if ($LASTEXITCODE -ne 0) { Fail "xorriso failed" }
	$IsoFile = Join-Path $StageDir $IsoName
	Step ("{0}: {1:N0} bytes" -f $IsoFile, (Get-Item $IsoFile).Length)
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
