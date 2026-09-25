# Engine\Build\BatchFiles\CheckBannedApis.ps1 - fails when engine or game code uses an API that Core replaces (gate G4).
#
# Banned (comments are ignored; names are case-sensitive):
#   glm and nlohmann                         -> Core math, the Json module
#   std::vector / string / map / unordered_map / function / shared_ptr / unique_ptr
#                                            -> TArray, FString, TMap, TFunction, TSharedPtr, TUniquePtr
#   <iostream>, std::cout, std::cerr          -> UE_LOG
#   printf and its variants                  -> UE_LOG / FString::Printf / FCString
#   LegacyGL, FLegacyTransform, LegacyAxes   -> removed in P7 (UE view and projection, FTransform, UE axes); tests too
#   FLegacyCoordinateConversion and LegacyCoordinateConversion.h
#                                            -> UE-space data; only the legacy bridge converts legacy (Y up, metres)
#                                               data: see $LegacyBridge
#
# Allowed where Core wraps the C and C++ libraries (D2): ThirdParty, the platform HAL sources (Private/Windows,
# Private/Linux, the PS2 Core), the printf family inside Core/Private, LeonHeaderTool (a std-only host tool) and the
# test program mains.
#
# A violation prints "<file>:<line>: G4 <rule>: <code> -> <what to use>". -Root checks another tree (default: the repo).
param([string] $Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") {
	$Root = Join-Path $PSScriptRoot "..\..\.."
}
$Root = (Resolve-Path $Root).Path.TrimEnd('\', '/')

# The legacy bridge: FLegacyCoordinateConversion itself, the legacy level reader and saver (.llev, until P15), the tests
# and the golden adapters that compare against legacy-space tables.
$LegacyBridge = '[\\/]Runtime[\\/]RenderCore[\\/](Public|Private)[\\/]LegacyCoordinateConversion\.(h|cpp)$|' +
	'[\\/]Runtime[\\/]Engine[\\/](Public|Private)[\\/]Level[\\/]LeonLevelFormat\.(h|cpp)$|' +
	'[\\/]Private[\\/]Tests[\\/]|' +
	'[\\/]Runtime[\\/]Engine[\\/]Public[\\/]Tests[\\/]LegacyGolden\.h$'

$Rules = @(
	@{ Name = "glm"; Pattern = 'glm::|<glm/'; Use = "Core math" },
	@{ Name = "nlohmann"; Pattern = 'nlohmann'; Use = "the Json module" },
	@{ Name = "std container / string / function / smart pointer"
		Pattern = 'std::(vector|string|map|unordered_map|function|shared_ptr|unique_ptr)\b'
		Use = "TArray, FString, TMap, TFunction, TSharedPtr, TUniquePtr" },
	@{ Name = "iostream"; Pattern = '<iostream>|std::(cout|cerr|clog)\b'; Use = "UE_LOG" },
	@{ Name = "printf"; Pattern = '(?<![A-Za-z_])(f|s|sn|v|vs|vsn)?printf\s*\('; AllowedIn = '[\\/]Runtime[\\/]Core[\\/]Private[\\/]'
		Use = "UE_LOG, FString::Printf or FCString" },
	@{ Name = "legacy GL / transform / axes"; Pattern = 'LegacyGL|FLegacyTransform|LegacyAxes'
		Use = "UE view and projection matrices (ToGLClipSpace last), FTransform, UE axes" },
	@{ Name = "legacy coordinate conversion outside the legacy bridge"
		Pattern = 'FLegacyCoordinateConversion|LegacyCoordinateConversion\.h'; AllowedIn = $LegacyBridge
		Use = "UE-space data; convert legacy data in its reader or saver (allowlist: `$LegacyBridge in CheckBannedApis.ps1)" }
)
$Excluded = '[\\/](ThirdParty|Intermediate|Binaries|Saved)[\\/]|[\\/]Private[\\/](Windows|Linux)[\\/]|' +
	'[\\/]Platforms[\\/]PS2[\\/]Source[\\/]Runtime[\\/]Core[\\/]|[\\/]LeonHeaderTool[\\/]|' +
	'LeonAutomationTestsMain\.cpp$|[\\/]TestPAL[\\/]Private[\\/]'

$Roots = @("Engine\Source", "Engine\Platforms", "Engine\Plugins", "Game") | ForEach-Object { Join-Path $Root $_ } |
	Where-Object { Test-Path $_ }
$Files = Get-ChildItem -Path $Roots -Recurse -File -Include *.h, *.cpp, *.inl | Where-Object { $_.FullName -notmatch $Excluded }

$Violations = 0
foreach ($File in $Files) {
	$Text = [System.IO.File]::ReadAllText($File.FullName)
	# Blank out comments but keep the line structure so reported line numbers stay right.
	$Text = [regex]::Replace($Text, '/\*.*?\*/', { param($M) [regex]::Replace($M.Value, '[^\n]', '') }, 'Singleline')
	$Text = [regex]::Replace($Text, '//[^\n]*', '')
	$Lines = $Text -split "`n"
	for ($Index = 0; $Index -lt $Lines.Count; $Index++) {
		foreach ($Rule in $Rules) {
			if ($Lines[$Index] -cnotmatch $Rule.Pattern) { continue }
			if ($Rule.AllowedIn -and $File.FullName -match $Rule.AllowedIn) { continue }
			$Relative = $File.FullName.Substring($Root.Length + 1)
			Write-Output ("{0}:{1}: G4 {2}: {3} -> {4}" -f $Relative, ($Index + 1), $Rule.Name, $Lines[$Index].Trim(),
				$Rule.Use)
			$Violations++
		}
	}
}

if ($Violations -gt 0) {
	Write-Output "CheckBannedApis: $Violations violation(s)"
	exit 1
}
Write-Output "CheckBannedApis: OK ($($Files.Count) files)"
exit 0
