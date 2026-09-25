#pragma once

#include <string>
#include <vector>

class FDiagnostics;

/** A header to reflect: its path on disk and the path the generated .gen.cpp includes. */
struct FManifestHeader
{
	std::string Path;
	std::string IncludePath;
};

/**
 * <Module>.lhtmanifest, written by LeonBuildTool's ReflectionRules.cmake (UHT: the .uhtmanifest UBT writes).
 * A line-based "Key=Value" file; '#' starts a comment line; list values separate fields with '|'. Relative paths
 * are relative to the manifest's folder.
 *
 *   Module=Engine                        module name (required)
 *   Api=ENGINE_API                       API macro of StaticClass<>/StaticStruct<>/StaticEnum<> (default <MODULE>_API)
 *   Package=/Script/Engine               default /Script/<Module>
 *   RootDir=C:/LeonEngine                CURRENT_FILE_ID is the header path relative to it
 *   OutputDir=Inc/Engine                 where the generated files go (default: the manifest's folder)
 *   InitFile=Engine.init.gen.cpp         package + RegisterReflection function (default <Module>.init.gen.cpp)
 *   RegisterFunction=RegisterReflection_Engine
 *   DefinePackage=1                      0: another unit of the module defines Z_Construct_UPackage__Script_<Module>
 *   TypeIndex=Engine.lhttypes            reflected types, read by dependent modules (default <Module>.lhttypes)
 *   Stamp=Engine.lhtstamp                rewritten after every successful run (optional)
 *   ReconfigureStamp=Engine.lhtreconfigure   touched when a Candidate gains a .generated.h include (optional)
 *   Dependency=Inc/Core/CoreUObject.lhttypes type index of a reflected dependency (repeatable)
 *   Header=<path>|<include path>         a header with #include "<Name>.generated.h" (repeatable, in order)
 *   Candidate=<path>                     a header of the module without it at configure time (repeatable)
 */
struct FManifest
{
	std::string Module;
	std::string Api;
	std::string Package;
	std::string RootDir;
	std::string OutputDir;
	std::string InitFile;
	std::string RegisterFunction;
	bool bDefinePackage = true;
	std::string TypeIndex;
	std::string Stamp;
	std::string ReconfigureStamp;
	std::vector<std::string> Dependencies;
	std::vector<FManifestHeader> Headers;
	std::vector<std::string> Candidates;

	/** Parses Text (read from Path, which also anchors relative paths) and fills the defaults. */
	bool Parse(const std::string& Path, const std::string& Text, FDiagnostics& Diagnostics);
};

bool ReadTextFile(const std::string& Path, std::string& OutContent);

/** Writes Content only when the file does not already hold exactly it, so ninja sees no change. */
bool WriteFileIfChanged(const std::string& Path, const std::string& Content);

/** Rewrites the file unconditionally (stamps). */
bool WriteFileAlways(const std::string& Path, const std::string& Content);

/** Splits "a|b|c" into its fields (one empty field for an empty string). */
std::vector<std::string> SplitFields(const std::string& Value);

/** Forward-slash, lexically normal form of Path; a relative Path is taken from BaseDir (or the current folder). */
std::string ResolvePath(const std::string& BaseDir, const std::string& Path);

std::string GetDirectory(const std::string& Path);
std::string GetFileName(const std::string& Path);
