// LeonHeaderTool: the UnrealHeaderTool counterpart. A std-only C++17 host program that LeonBuildTool builds into
// Engine/Intermediate/Build/HostTools/<Host> and runs before compiling a reflected module.
//
//   LeonHeaderTool <Module>.lhtmanifest      generate the reflection code of one module (see Manifest.h)
//   LeonHeaderTool -Test [<dir>] [-Update]   golden tests: <dir>/Inputs/<Case>/*.h against <dir>/Expected/<Case>/
//
// Errors are printed as "<file>(<line>): error: <message>" and make the exit code non-zero. Output files are only
// rewritten when their content changes, so ninja does not recompile unchanged generated code.

#include "CodeGenerator.h"
#include "Diagnostics.h"
#include "HeaderParser.h"
#include "Manifest.h"
#include "TypeModel.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifndef LHT_TESTS_DIR
	#define LHT_TESTS_DIR "Tests"
#endif

namespace
{
	/** CURRENT_FILE_ID: the header path relative to the root, non-identifier characters replaced by '_'. */
	std::string MakeFileId(const std::string& RootDir, const std::string& Path)
	{
		std::string Relative = GetFileName(Path);
		const std::filesystem::path RelativePath =
			std::filesystem::u8path(Path).lexically_relative(std::filesystem::u8path(RootDir));
		const std::string RelativeText = RelativePath.u8string();
		if (!RelativeText.empty() && RelativeText.compare(0, 2, "..") != 0)
		{
			Relative = RelativeText;
		}
		for (char& C : Relative)
		{
			if (std::isalnum(static_cast<unsigned char>(C)) == 0)
			{
				C = '_';
			}
		}
		if (!Relative.empty() && std::isdigit(static_cast<unsigned char>(Relative[0])) != 0)
		{
			Relative.insert(0, "_");
		}
		return Relative;
	}

	std::string StripExtension(const std::string& FileName)
	{
		const size_t Dot = FileName.find_last_of('.');
		return Dot == std::string::npos ? FileName : FileName.substr(0, Dot);
	}

	/**
	 * Parses, resolves and generates one manifest. OutFiles receives every output (headers' .generated.h / .gen.cpp,
	 * the init file and the type index) unless there were errors. bDisplayRelative prints header paths relative to
	 * RootDir (golden tests).
	 */
	bool RunUnit(const FManifest& Manifest, FDiagnostics& Diagnostics, bool bDisplayRelative,
		std::vector<FGeneratedFile>& OutFiles)
	{
		FTypeTable Types;
		for (const std::string& Dependency : Manifest.Dependencies)
		{
			Types.LoadIndex(Dependency, Diagnostics);
		}

		std::vector<FUnrealSourceFile> Files;
		std::map<std::string, std::string> BaseNames;
		for (const FManifestHeader& Header : Manifest.Headers)
		{
			FUnrealSourceFile File;
			File.Path = Header.Path;
			File.IncludePath = Header.IncludePath;
			File.BaseName = StripExtension(GetFileName(Header.Path));
			File.FileId = MakeFileId(Manifest.RootDir, Header.Path);
			File.DisplayPath = Header.Path;
			if (bDisplayRelative)
			{
				File.DisplayPath = std::filesystem::u8path(Header.Path)
									   .lexically_relative(std::filesystem::u8path(Manifest.RootDir))
									   .generic_u8string();
			}
			const auto Existing = BaseNames.find(File.BaseName);
			if (Existing != BaseNames.end())
			{
				Diagnostics.Error(File.DisplayPath, 0,
					"Two reflected headers of module " + Manifest.Module + " are named '" + File.BaseName +
						".h' (the other one is " + Existing->second + ")");
				continue;
			}
			BaseNames[File.BaseName] = File.DisplayPath;

			std::string Source;
			if (!ReadTextFile(Header.Path, Source))
			{
				Diagnostics.Error(File.DisplayPath, 0, "Cannot read the header");
				continue;
			}
			FHeaderParser Parser(File, Diagnostics);
			Parser.Parse(Source);
			Files.push_back(std::move(File));
		}

		// A header that gained its first .generated.h include after the build was configured.
		bool bNeedsReconfigure = false;
		for (const std::string& Candidate : Manifest.Candidates)
		{
			std::string Source;
			if (!ReadTextFile(Candidate, Source))
			{
				continue;
			}
			const int Line = FHeaderParser::FindGeneratedInclude(Source);
			if (Line > 0)
			{
				Diagnostics.Error(Candidate, Line,
					"This header includes its .generated.h but was not reflected when the build was configured; the "
					"build tree reconfigures on the next build");
				bNeedsReconfigure = true;
			}
		}
		if (bNeedsReconfigure && !Manifest.ReconfigureStamp.empty())
		{
			WriteFileAlways(Manifest.ReconfigureStamp,
				"Touched by LeonHeaderTool: a header of module " + Manifest.Module +
					" gained a .generated.h include.\n");
		}
		if (Diagnostics.GetErrorCount() > 0)
		{
			return false;
		}

		Types.AddLocalTypes(Manifest.Module, Files, Diagnostics);
		Types.Resolve(Files, Diagnostics);
		if (Diagnostics.GetErrorCount() > 0)
		{
			return false;
		}

		const FCodeGenerator Generator(Manifest);
		for (const FUnrealSourceFile& File : Files)
		{
			Generator.GenerateHeader(File, OutFiles);
		}
		OutFiles.push_back({Manifest.InitFile, Generator.GenerateInit(Files)});
		OutFiles.push_back({Manifest.TypeIndex, FTypeTable::WriteIndex(Manifest.Module, Files)});
		return true;
	}

	int RunManifest(const std::string& ManifestPath)
	{
		FDiagnostics Diagnostics;
		const std::string Path = ResolvePath("", ManifestPath);
		std::string Text;
		if (!ReadTextFile(Path, Text))
		{
			Diagnostics.Error(Path, 0, "Cannot read the manifest");
			return 1;
		}
		FManifest Manifest;
		if (!Manifest.Parse(Path, Text, Diagnostics))
		{
			return 1;
		}
		std::vector<FGeneratedFile> Files;
		if (!RunUnit(Manifest, Diagnostics, false, Files))
		{
			return 1;
		}
		for (const FGeneratedFile& File : Files)
		{
			const std::string OutputPath = Manifest.OutputDir + "/" + File.Name;
			if (!WriteFileIfChanged(OutputPath, File.Content))
			{
				Diagnostics.Error(OutputPath, 0, "Cannot write the generated file");
			}
		}
		if (Diagnostics.GetErrorCount() == 0 && !Manifest.Stamp.empty())
		{
			const std::string StampPath = ResolvePath(Manifest.OutputDir, Manifest.Stamp);
			if (!WriteFileAlways(StampPath, "LeonHeaderTool ran for module " + Manifest.Module + ".\n"))
			{
				Diagnostics.Error(StampPath, 0, "Cannot write the stamp file");
			}
		}
		return Diagnostics.GetErrorCount() > 0 ? 1 : 0;
	}

	std::string StripCarriageReturns(std::string Text)
	{
		Text.erase(std::remove(Text.begin(), Text.end(), '\r'), Text.end());
		return Text;
	}

	/** First differing line of two texts, for the failure report. */
	std::string DescribeDifference(const std::string& Expected, const std::string& Actual)
	{
		size_t ExpectedPos = 0;
		size_t ActualPos = 0;
		int Line = 1;
		while (true)
		{
			const size_t ExpectedEnd = Expected.find('\n', ExpectedPos);
			const size_t ActualEnd = Actual.find('\n', ActualPos);
			const std::string ExpectedLine = ExpectedPos > Expected.size()
				? "<end of file>"
				: Expected.substr(
					  ExpectedPos, ExpectedEnd == std::string::npos ? std::string::npos : ExpectedEnd - ExpectedPos);
			const std::string ActualLine = ActualPos > Actual.size()
				? "<end of file>"
				: Actual.substr(ActualPos, ActualEnd == std::string::npos ? std::string::npos : ActualEnd - ActualPos);
			if (ExpectedLine != ActualLine || ExpectedEnd == std::string::npos || ActualEnd == std::string::npos)
			{
				return "line " + std::to_string(Line) + "\n      expected: " + ExpectedLine +
					"\n      actual:   " + ActualLine;
			}
			ExpectedPos = ExpectedEnd + 1;
			ActualPos = ActualEnd + 1;
			++Line;
		}
	}

	std::vector<std::string> ListEntries(const std::string& Dir, bool bDirectories)
	{
		std::vector<std::string> Names;
		std::error_code Error;
		for (const std::filesystem::directory_entry& Entry :
			std::filesystem::directory_iterator(std::filesystem::u8path(Dir), Error))
		{
			if (Entry.is_directory() == bDirectories)
			{
				Names.push_back(Entry.path().filename().u8string());
			}
		}
		std::sort(Names.begin(), Names.end());
		return Names;
	}

	/**
	 * Golden tests: every <Dir>/Inputs/<Case> is a module (its optional Test.lhtmanifest sets Module, Api,
	 * Dependency... with paths relative to the case; without Header= lines every *.h of the case is reflected, in name
	 * order). The outputs, plus Diagnostics.txt when anything was reported, must equal <Dir>/Expected/<Case>/.
	 * -Update rewrites the expected files instead.
	 */
	int RunGoldenTests(const std::string& InDir, bool bUpdate)
	{
		const std::string Dir = ResolvePath("", InDir);
		const std::string InputsDir = Dir + "/Inputs";
		const std::string ExpectedDir = Dir + "/Expected";
		const std::vector<std::string> Cases = ListEntries(InputsDir, true);
		if (Cases.empty())
		{
			std::fprintf(stderr, "%s: error: no golden test cases found\n", InputsDir.c_str());
			return 1;
		}
		int Passed = 0;
		for (const std::string& Case : Cases)
		{
			const std::string CaseDir = InputsDir + "/" + Case;
			FDiagnostics Diagnostics(true);
			FManifest Manifest;
			std::string ManifestText = "Module=LhtTest\n";
			ReadTextFile(CaseDir + "/Test.lhtmanifest", ManifestText);
			Manifest.Parse(CaseDir + "/Test.lhtmanifest", ManifestText, Diagnostics);
			Manifest.RootDir = InputsDir;
			if (Manifest.Headers.empty())
			{
				for (const std::string& Name : ListEntries(CaseDir, false))
				{
					if (Name.size() > 2 && Name.compare(Name.size() - 2, 2, ".h") == 0)
					{
						Manifest.Headers.push_back({CaseDir + "/" + Name, Name});
					}
				}
			}

			std::vector<FGeneratedFile> Outputs;
			if (Diagnostics.GetErrorCount() == 0)
			{
				RunUnit(Manifest, Diagnostics, true, Outputs);
			}
			std::map<std::string, std::string> Actual;
			for (const FGeneratedFile& File : Outputs)
			{
				Actual[File.Name] = File.Content;
			}
			if (!Diagnostics.GetMessages().empty())
			{
				std::string Messages;
				for (const std::string& Message : Diagnostics.GetMessages())
				{
					Messages += Message + "\n";
				}
				Actual["Diagnostics.txt"] = Messages;
			}

			const std::string CaseExpectedDir = ExpectedDir + "/" + Case;
			if (bUpdate)
			{
				std::error_code Error;
				std::filesystem::remove_all(std::filesystem::u8path(CaseExpectedDir), Error);
				for (const auto& File : Actual)
				{
					WriteFileAlways(CaseExpectedDir + "/" + File.first, File.second);
				}
				std::printf("LeonHeaderTool: UPDATED %s\n", Case.c_str());
				++Passed;
				continue;
			}

			std::map<std::string, std::string> Expected;
			for (const std::string& Name : ListEntries(CaseExpectedDir, false))
			{
				std::string Content;
				ReadTextFile(CaseExpectedDir + "/" + Name, Content);
				Expected[Name] = StripCarriageReturns(Content);
			}
			std::string Failure;
			for (const auto& File : Expected)
			{
				const auto Found = Actual.find(File.first);
				if (Found == Actual.end())
				{
					Failure += "\n    missing output " + File.first;
				}
				else if (Found->second != File.second)
				{
					Failure += "\n    " + File.first + " differs at " + DescribeDifference(File.second, Found->second);
				}
			}
			for (const auto& File : Actual)
			{
				if (Expected.find(File.first) == Expected.end())
				{
					Failure += "\n    unexpected output " + File.first;
					if (File.first == "Diagnostics.txt")
					{
						Failure += ":\n" + File.second;
					}
				}
			}
			if (Failure.empty())
			{
				std::printf("LeonHeaderTool: PASS %s\n", Case.c_str());
				++Passed;
			}
			else
			{
				std::printf("LeonHeaderTool: FAIL %s%s\n", Case.c_str(), Failure.c_str());
			}
		}
		std::printf("LeonHeaderTool -Test: %d of %d golden cases %s\n", Passed, static_cast<int>(Cases.size()),
			bUpdate ? "updated" : "passed");
		return Passed == static_cast<int>(Cases.size()) ? 0 : 1;
	}
} // namespace

int main(int ArgC, char* ArgV[])
{
	std::vector<std::string> Args(ArgV + 1, ArgV + ArgC);
	if (!Args.empty() && Args[0] == "-Test")
	{
		std::string Dir = LHT_TESTS_DIR;
		bool bUpdate = false;
		for (size_t Index = 1; Index < Args.size(); ++Index)
		{
			if (Args[Index] == "-Update")
			{
				bUpdate = true;
			}
			else
			{
				Dir = Args[Index];
			}
		}
		return RunGoldenTests(Dir, bUpdate);
	}
	if (Args.size() == 1 && !Args[0].empty() && Args[0][0] != '-')
	{
		return RunManifest(Args[0]);
	}
	std::fprintf(stderr,
		"Usage: LeonHeaderTool <Module>.lhtmanifest\n"
		"       LeonHeaderTool -Test [<TestsDir>] [-Update]\n");
	return 1;
}
