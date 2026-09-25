#pragma once

#include "Manifest.h"
#include "TypeModel.h"

#include <string>
#include <vector>

struct FGeneratedFile
{
	/** File name inside the manifest's OutputDir. */
	std::string Name;
	std::string Content;
};

/**
 * Writes UE 4.27-shaped reflection code (UHT: FNativeClassHeaderGenerator): <Header>.generated.h with the
 * GENERATED_BODY macros, <Header>.gen.cpp with the UE4CodeGen_Private parameter tables, Z_Construct_* functions and
 * exec thunks, and <Module>.init.gen.cpp with the package and the explicit registration function. The contract the
 * runtime (CoreUObject) implements is in Engine/Source/Programs/LeonHeaderTool/README.md.
 */
class FCodeGenerator
{
public:
	explicit FCodeGenerator(const FManifest& InManifest);

	/** Adds <BaseName>.generated.h and <BaseName>.gen.cpp for one parsed header. */
	void GenerateHeader(const FUnrealSourceFile& File, std::vector<FGeneratedFile>& OutFiles) const;

	/** The InitFile: Z_Construct_UPackage__Script_<Module> and the RegisterFunction for every type in order. */
	std::string GenerateInit(const std::vector<FUnrealSourceFile>& Files) const;

private:
	std::string GenerateGeneratedHeader(const FUnrealSourceFile& File) const;
	std::string GenerateGenCpp(const FUnrealSourceFile& File) const;

	std::string GetPackageFunction() const;

	const FManifest& Manifest;
};
