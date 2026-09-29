# Measures a PS2 game's frame in PCSX2, unattended (Docs/PLANS/ps2-shipping.md N1).
# Usage: Engine\Build\BatchFiles\MeasurePS2.bat [-Project Game\ShooterGame] [-Rounds 2] [-Seed 7] [-Seconds 120]
#                                               [-NoBuild] [-TimeoutSeconds 900] [-Label <text>] [-Iso]
#                                               [-ExtraArgs <game arguments>] (e.g. -novu1: the EE's C++ emitter)
#                                               [-PakOrder <order file>] [-LogFileOpenOrder]
#
# 1. BuildCookRun -platform=PS2 -build -cook -stage -pak stages the game with its measuring command line (-NoBuild keeps
#    the stage and only rewrites LeonCommandLine.txt): a bot match of -Rounds rounds with -Seed, the local player
#    watching through a bot's eyes (-BotMatchSpectate), -LogFrameTimes, and -ExitAfterSeconds as a bound.
# 2. PCSX2 runs it without its window (-nogui) from a private data folder (<Project>\Saved\PCSX2) whose PCSX2.ini is the
#    user's with Engine\Platforms\PS2\Build\PCSX2\Measure.ini on top: the console's EE and VU timings, host: on.
# 3. The EE log is read until the game's `ProfileSummary:` line (UGameEngine at exit, after `FrameStats Summary:`), then
#    PCSX2 is closed: the PS2's exit returns to the BIOS, it does not end the emulator.
# 4. The figures go to <Project>\Saved\Profiling\PS2Frame.csv (one row per run: the `FrameStats Summary:` pairs, the heap
#    at exit and the allocations per frame among them (heap_kb, allocs_per_frame, N17), then the `ProfileSummary:` ones as
#    Profile_<key>, N9, the `MemoryTags:` ones, each memory tag's peak, as Memory_<key>, N17, and the `SceneWork:` ones,
#    the scene's objects, draws and batches a frame by placement, as Scene_<key>, N29), and the Budgets.md row
#    and the run's last `Profile over` block (the cycle stats' hierarchy of the frame) are printed.
#
# -Iso (Docs/PLANS/ps2-shipping.md N23) boots the disc instead of the ELF: BuildCookRun -iso makes
# <Project>\Saved\StagedBuilds\PS2\<Project>.iso with the command line on it (LEONCOMM.TXT), and PCSX2 boots it
# (-fastboot -- <iso>): the game reads its pak from cdrom0:, at the emulated drive's speed. -PakOrder lays the pak out
# in a recorded open order (BuildCookRun -pakorder=, N23), and -LogFileOpenOrder adds the switch that records it to the
# game's command line: the run's EE log is the order file of the next (N24 measures the disc with the ordered pak).
#
# The game's clock is the emulated console's, so the milliseconds do not depend on the host's speed. PCSX2 is not the
# hardware: rows are labelled "PCSX2 <version>, Measure.ini <hash>".
param(
	[string]$Project = "Game\ShooterGame",
	[int]$Rounds = 2,
	[int]$Seed = 7,
	[int]$Seconds = 120,
	[switch]$NoBuild,
	[int]$TimeoutSeconds = 900,
	[string]$Label = "",
	[switch]$Iso,
	[string]$ExtraArgs = "",
	[string]$PakOrder = "",
	[switch]$LogFileOpenOrder
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..\..")).Path

function Fail([string]$Message)
{
	Write-Host "MeasurePS2: $Message" -ForegroundColor Red
	exit 1
}

$ProjectDir = if ([System.IO.Path]::IsPathRooted($Project)) { $Project } else { Join-Path $Root $Project }
$ProjectFile = Get-ChildItem -Path $ProjectDir -Filter *.lproj | Select-Object -First 1
if (-not $ProjectFile) { Fail "no .lproj in $ProjectDir" }
$ProjectName = [System.IO.Path]::GetFileNameWithoutExtension($ProjectFile.Name)
$StageDir = Join-Path $ProjectDir "Saved\StagedBuilds\PS2"
$StagedElf = Join-Path $StageDir "$ProjectName.elf"
$GameArgs = "-botmatch -rounds=$Rounds -seed=$Seed -BotMatchSpectate -LogFrameTimes -ExitAfterSeconds=$Seconds"
if ($ExtraArgs) { $GameArgs = "$GameArgs $ExtraArgs" }
if ($LogFileOpenOrder) { $GameArgs += " -LogFileOpenOrder" }
$StagedIso = Join-Path $StageDir "$ProjectName.iso"
$IsoArg = if ($Iso) { " -iso" } else { "" }
if ($PakOrder -ne "")
{
	if (-not (Test-Path $PakOrder -PathType Leaf)) { Fail "no pak order file '$PakOrder'" }
	$IsoArg += " ""-pakorder=$((Resolve-Path $PakOrder).Path)"""
}

# 1. The staged game and its command line.
if (-not $NoBuild)
{
	& cmd /c "`"$Root\Engine\Build\BatchFiles\BuildCookRun.bat`" `"-project=$($ProjectFile.FullName)`" -platform=PS2 -build -cook -stage -pak$IsoArg `"-addcmdline=$GameArgs`""
	if ($LASTEXITCODE -ne 0) { Fail "BuildCookRun failed ($LASTEXITCODE)" }
}
if (-not (Test-Path $StagedElf)) { Fail "no staged game '$StagedElf' (run without -NoBuild)" }
Set-Content -Path (Join-Path $StageDir "LeonCommandLine.txt") -Value $GameArgs -Encoding ascii
if ($Iso -and $NoBuild)
{
	# The disc again, with this command line on it.
	& cmd /c "`"$Root\Engine\Build\BatchFiles\BuildCookRun.bat`" `"-project=$($ProjectFile.FullName)`" -platform=PS2 -iso"
	if ($LASTEXITCODE -ne 0) { Fail "BuildCookRun -iso failed ($LASTEXITCODE)" }
}
if ($Iso -and -not (Test-Path $StagedIso)) { Fail "no disc image '$StagedIso'" }

# 2. PCSX2 and its private settings.
$Pcsx2 = $env:LEON_PCSX2
if (-not $Pcsx2)
{
	$Pcsx2 = @("$env:ProgramFiles\PCSX2\pcsx2-qt.exe", "$env:LOCALAPPDATA\Programs\PCSX2\pcsx2-qt.exe") |
		Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Pcsx2) { Fail "PCSX2 not found (set LEON_PCSX2)" }
$Pcsx2Version = (Get-Item $Pcsx2).VersionInfo.ProductVersion
$UserData = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "PCSX2"
$UserIni = Join-Path $UserData "inis\PCSX2.ini"
if (-not (Test-Path $UserIni)) { Fail "no PCSX2 settings at '$UserIni': run PCSX2 once and set its BIOS" }

# -datapath names the parent of PCSX2's data root: PCSX2 reads <datapath>\PCSX2\inis\PCSX2.ini.
$DataPath = Join-Path $ProjectDir "Saved\PCSX2"
$DataDir = Join-Path $DataPath "PCSX2"
New-Item -ItemType Directory -Force -Path (Join-Path $DataDir "inis"), (Join-Path $DataDir "logs") | Out-Null
$MeasureIniPath = Join-Path $Root "Engine\Platforms\PS2\Build\PCSX2\Measure.ini"
# The hash of the settings with LF line ends, so a checkout's line endings (core.autocrlf) do not change the label.
$MeasureText = [System.IO.File]::ReadAllText($MeasureIniPath).Replace("`r`n", "`n")
$MeasureBytes = [System.Text.Encoding]::UTF8.GetBytes($MeasureText)
$MeasureHash = ([System.BitConverter]::ToString([System.Security.Cryptography.SHA1]::Create().ComputeHash($MeasureBytes)) -replace "-", "").Substring(0, 8).ToLowerInvariant()

# An .ini as an ordered list of sections, each an ordered map of keys (comments dropped).
function Read-Ini([string[]]$Lines)
{
	$Sections = [ordered]@{}
	$Current = ""
	$Sections[$Current] = [ordered]@{}
	foreach ($Line in $Lines)
	{
		$Trimmed = $Line.Trim()
		if ($Trimmed -eq "" -or $Trimmed.StartsWith(";") -or $Trimmed.StartsWith("#")) { continue }
		if ($Trimmed -match '^\[(.+)\]$')
		{
			$Current = $Matches[1]
			if (-not $Sections.Contains($Current)) { $Sections[$Current] = [ordered]@{} }
			continue
		}
		$Equals = $Trimmed.IndexOf("=")
		if ($Equals -gt 0) { $Sections[$Current][$Trimmed.Substring(0, $Equals).Trim()] = $Trimmed.Substring($Equals + 1).Trim() }
	}
	return $Sections
}

$UserBios = Join-Path $UserData "bios"
$Overrides = (Get-Content $MeasureIniPath) | ForEach-Object { $_.Replace("{BIOS}", $UserBios).Replace("{DATA}", $DataDir) }
$Merged = Read-Ini (Get-Content $UserIni)
$OverrideSections = Read-Ini $Overrides
foreach ($Section in $OverrideSections.Keys)
{
	if (-not $Merged.Contains($Section)) { $Merged[$Section] = [ordered]@{} }
	foreach ($Key in $OverrideSections[$Section].Keys) { $Merged[$Section][$Key] = $OverrideSections[$Section][$Key] }
}
$IniText = foreach ($Section in $Merged.Keys)
{
	if ($Section -ne "") { "[$Section]" }
	foreach ($Key in $Merged[$Section].Keys) { "$Key = $($Merged[$Section][$Key])" }
	""
}
# UTF-8 without a byte order mark (Set-Content -Encoding utf8 writes one in Windows PowerShell).
[System.IO.File]::WriteAllLines((Join-Path $DataDir "inis\PCSX2.ini"), [string[]]$IniText, (New-Object System.Text.UTF8Encoding $false))

# 3. The run.
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$Log = Join-Path $DataDir "logs\Measure-$Stamp.log"
$Boot = if ($Iso) { @("--", "`"$StagedIso`"") } else { @("-elf", "`"$StagedElf`"") }
Write-Host "MeasurePS2: $(if ($Iso) { $StagedIso } else { $StagedElf }) in PCSX2 $Pcsx2Version (Measure.ini $MeasureHash), log $Log"
$Process = Start-Process -FilePath $Pcsx2 -PassThru -ArgumentList (@(
	"-nogui", "-datapath", "`"$DataPath`"", "-logfile", "`"$Log`"", "-fastboot") + $Boot)
$Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$Summary = $null
while ((Get-Date) -lt $Deadline -and -not $Process.HasExited)
{
	Start-Sleep -Seconds 2
	if (Test-Path $Log)
	{
		$ProfileLine = Select-String -Path $Log -Pattern "ProfileSummary:" -SimpleMatch | Select-Object -Last 1
		if ($ProfileLine)
		{
			$Summary = Select-String -Path $Log -Pattern "FrameStats Summary:" -SimpleMatch | Select-Object -Last 1
			break
		}
	}
}
Start-Sleep -Seconds 1
if (-not $Process.HasExited) { Stop-Process -Id $Process.Id -Force }
if (-not $Summary -or -not $ProfileLine)
{
	$Why = if ($Process.HasExited) { "PCSX2 exited ($($Process.ExitCode)) before" } else { "not within $TimeoutSeconds s" }
	Fail "no 'FrameStats Summary:' and 'ProfileSummary:' lines: $Why (log: $Log)"
}

# 4. The figures: the key=value pairs after a line's tag.
function Read-Pairs([string]$Line, [string]$Tag)
{
	$Pairs = [ordered]@{}
	foreach ($Pair in ($Line -split $Tag)[1].Trim() -split "\s+")
	{
		$Equals = $Pair.IndexOf("=")
		if ($Equals -gt 0) { $Pairs[$Pair.Substring(0, $Equals)] = $Pair.Substring($Equals + 1) }
	}
	return $Pairs
}
$Values = Read-Pairs $Summary.Line "FrameStats Summary:"
$ProfileValues = Read-Pairs $ProfileLine.Line "ProfileSummary:"
# Each memory tag's peak (N17; a build without the memory tracker logs none).
$MemoryLine = Select-String -Path $Log -Pattern "MemoryTags:" -SimpleMatch | Select-Object -Last 1
$MemoryValues = if ($MemoryLine) { Read-Pairs $MemoryLine.Line "MemoryTags:" } else { [ordered]@{} }
# The scene's work a frame: objects, draws and batches by placement (N29), as Scene_<key>.
$SceneLine = Select-String -Path $Log -Pattern "SceneWork:" -SimpleMatch | Select-Object -Last 1
$SceneValues = if ($SceneLine) { Read-Pairs $SceneLine.Line "SceneWork:" } else { [ordered]@{} }
# The last `Profile over N frames` block: its header and the indented scopes after it.
$LogLines = @(Get-Content $Log)
$ProfileBlock = @()
$BlockStart = (Select-String -Path $Log -Pattern "Profile over " -SimpleMatch | Select-Object -Last 1)
if ($BlockStart)
{
	$ProfileBlock += $LogLines[$BlockStart.LineNumber - 1]
	for ($Index = $BlockStart.LineNumber; $Index -lt $LogLines.Count -and $LogLines[$Index] -match "Display:   "; $Index++)
	{
		$ProfileBlock += $LogLines[$Index]
	}
}
$VBlank = $ProfileBlock | Where-Object { $_ -match "Vertical Blank Wait: ([0-9.]+) ms" } | Select-Object -First 1
$VBlankMs = if ($VBlank -and $VBlank -match "Vertical Blank Wait: ([0-9.]+) ms") { $Matches[1] } else { "?" }
$BotMatch = Select-String -Path $Log -Pattern "Botmatch OK", "Botmatch FAILED" -SimpleMatch | Select-Object -Last 1

$Commit = (& git -C $Root rev-parse --short HEAD).Trim()
$Dirty = if ((& git -C $Root status --porcelain) ) { "+dirty" } else { "" }
$Row = [ordered]@{
	Date = (Get-Date -Format "yyyy-MM-dd HH:mm")
	Commit = "$Commit$Dirty"
	Label = $Label
	Pcsx2 = $Pcsx2Version
	MeasureIni = $MeasureHash
	Args = $GameArgs + $(if ($Iso) { " (disc)" } else { "" })
}
foreach ($Key in $Values.Keys) { $Row[$Key] = $Values[$Key] }
foreach ($Key in $ProfileValues.Keys) { $Row["Profile_$Key"] = $ProfileValues[$Key] }
foreach ($Key in $MemoryValues.Keys) { $Row["Memory_$Key"] = $MemoryValues[$Key] }
foreach ($Key in $SceneValues.Keys) { $Row["Scene_$Key"] = $SceneValues[$Key] }
$ProfilingDir = Join-Path $ProjectDir "Saved\Profiling"
New-Item -ItemType Directory -Force -Path $ProfilingDir | Out-Null
$Csv = Join-Path $ProfilingDir "PS2Frame.csv"
# A row with columns the file does not have yet rewrites it with every column (Export-Csv -Append would drop them).
$NewRow = [PSCustomObject]$Row
$OldRows = if (Test-Path $Csv) { @(Import-Csv -Path $Csv) } else { @() }
if ($OldRows.Count -eq 0)
{
	$NewRow | Export-Csv -Path $Csv -NoTypeInformation
}
else
{
	$Columns = @($OldRows[0].PSObject.Properties.Name)
	foreach ($Key in $Row.Keys) { if ($Columns -notcontains $Key) { $Columns += $Key } }
	@($OldRows) + @($NewRow) | Select-Object -Property $Columns | Export-Csv -Path $Csv -NoTypeInformation
}

# The load time (N23): the engine's start to its first frame, on the EE's clock.
$LoadLine = Select-String -Path $Log -Pattern "First frame after" -SimpleMatch | Select-Object -Last 1
Write-Host ""
if ($LoadLine) { Write-Host $LoadLine.Line }
# The first frame (the map's first tick) and the worst after it (N24: no frame waits for the disc), and the long frames.
Write-Host ("First frame {0} ms, worst after it {1} ms" -f $Values["first_ms"], $Values["worst_later_ms"])
Select-String -Path $Log -Pattern "Long frame " -SimpleMatch | ForEach-Object { Write-Host $_.Line }
Write-Host $Summary.Line
Write-Host $ProfileLine.Line
if ($MemoryLine) { Write-Host $MemoryLine.Line }
if ($SceneLine) { Write-Host $SceneLine.Line }
$ProfileBlock | ForEach-Object { Write-Host $_ }
if ($BotMatch) { Write-Host $BotMatch.Line }
Write-Host ""
Write-Host "Budgets.md row:"
Write-Host ("| {0} | {1} | {2} ms | {3} / {4} / {5} ms | {6} ms | {7} ms | {8} ms | {9} + {10} ms | {11} ms | {12} ms ({13}) | {14} | {15} KB | {16} KB | {17} KB | {18} | {19} | {20}, {21} |" -f
	$(if ($Label) { $Label } else { $Commit }), $Values["fps"], $Values["avg_ms"], $Values["p50_ms"], $Values["p95_ms"],
	$Values["p99_ms"], $Values["worst_ms"], $Values["world_ms"], $Values["scene_ms"], $Values["hud_ms"],
	$Values["canvas_ms"], $Values["audio_ms"], $Values["present_ms"], $VBlankMs, $Values["tris"], $Values["gif_kb"],
	$Values["gmalloc_peak_kb"], $Values["heap_kb"], $Values["allocs_per_frame"], $Values["uobjects_peak"],
	$Pcsx2Version, $MeasureHash)
Write-Host "MeasurePS2 OK ($Csv)"
