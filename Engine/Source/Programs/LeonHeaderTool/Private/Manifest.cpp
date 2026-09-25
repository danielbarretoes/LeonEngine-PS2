#include "Manifest.h"

#include "Diagnostics.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
	std::string ToUpper(std::string Text)
	{
		for (char& C : Text)
		{
			C = static_cast<char>(std::toupper(static_cast<unsigned char>(C)));
		}
		return Text;
	}
} // namespace

bool FManifest::Parse(const std::string& Path, const std::string& Text, FDiagnostics& Diagnostics)
{
	const std::string BaseDir = GetDirectory(Path);
	std::istringstream Stream(Text);
	std::string Line;
	int LineNumber = 0;
	const int ErrorsBefore = Diagnostics.GetErrorCount();
	while (std::getline(Stream, Line))
	{
		++LineNumber;
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.pop_back();
		}
		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}
		const size_t Equals = Line.find('=');
		if (Equals == std::string::npos)
		{
			Diagnostics.Error(Path, LineNumber, "Expected <Key>=<Value>, got '" + Line + "'");
			continue;
		}
		const std::string Key = Line.substr(0, Equals);
		const std::string Value = Line.substr(Equals + 1);
		if (Key == "Module")
		{
			Module = Value;
		}
		else if (Key == "Api")
		{
			Api = Value;
		}
		else if (Key == "Package")
		{
			Package = Value;
		}
		else if (Key == "RootDir")
		{
			RootDir = ResolvePath(BaseDir, Value);
		}
		else if (Key == "OutputDir")
		{
			OutputDir = ResolvePath(BaseDir, Value);
		}
		else if (Key == "InitFile")
		{
			InitFile = Value;
		}
		else if (Key == "RegisterFunction")
		{
			RegisterFunction = Value;
		}
		else if (Key == "DefinePackage")
		{
			bDefinePackage = Value != "0";
		}
		else if (Key == "TypeIndex")
		{
			TypeIndex = Value;
		}
		else if (Key == "Stamp")
		{
			Stamp = Value;
		}
		else if (Key == "ReconfigureStamp")
		{
			ReconfigureStamp = ResolvePath(BaseDir, Value);
		}
		else if (Key == "Dependency")
		{
			Dependencies.push_back(ResolvePath(BaseDir, Value));
		}
		else if (Key == "Header")
		{
			const std::vector<std::string> Fields = SplitFields(Value);
			FManifestHeader Header;
			Header.Path = ResolvePath(BaseDir, Fields[0]);
			Header.IncludePath = Fields.size() > 1 ? Fields[1] : GetFileName(Header.Path);
			Headers.push_back(Header);
		}
		else if (Key == "Candidate")
		{
			Candidates.push_back(ResolvePath(BaseDir, Value));
		}
		else
		{
			Diagnostics.Error(Path, LineNumber, "Unknown manifest key '" + Key + "'");
		}
	}
	if (Module.empty())
	{
		Diagnostics.Error(Path, 0, "The manifest has no Module= line");
		return false;
	}
	if (Api.empty())
	{
		Api = ToUpper(Module) + "_API";
	}
	if (Package.empty())
	{
		Package = "/Script/" + Module;
	}
	if (RootDir.empty())
	{
		RootDir = BaseDir;
	}
	if (OutputDir.empty())
	{
		OutputDir = BaseDir;
	}
	if (InitFile.empty())
	{
		InitFile = Module + ".init.gen.cpp";
	}
	if (RegisterFunction.empty())
	{
		RegisterFunction = "RegisterReflection_" + Module;
	}
	if (TypeIndex.empty())
	{
		TypeIndex = Module + ".lhttypes";
	}
	return Diagnostics.GetErrorCount() == ErrorsBefore;
}

bool ReadTextFile(const std::string& Path, std::string& OutContent)
{
	std::ifstream File(std::filesystem::u8path(Path), std::ios::binary);
	if (!File)
	{
		return false;
	}
	std::ostringstream Buffer;
	Buffer << File.rdbuf();
	OutContent = Buffer.str();
	return true;
}

bool WriteFileIfChanged(const std::string& Path, const std::string& Content)
{
	std::string Existing;
	if (ReadTextFile(Path, Existing) && Existing == Content)
	{
		return true;
	}
	return WriteFileAlways(Path, Content);
}

bool WriteFileAlways(const std::string& Path, const std::string& Content)
{
	const std::filesystem::path FilePath = std::filesystem::u8path(Path);
	std::error_code Error;
	if (FilePath.has_parent_path())
	{
		std::filesystem::create_directories(FilePath.parent_path(), Error);
	}
	std::ofstream File(FilePath, std::ios::binary | std::ios::trunc);
	if (!File)
	{
		return false;
	}
	File.write(Content.data(), static_cast<std::streamsize>(Content.size()));
	return static_cast<bool>(File);
}

std::vector<std::string> SplitFields(const std::string& Value)
{
	std::vector<std::string> Fields;
	size_t Start = 0;
	while (true)
	{
		const size_t Bar = Value.find('|', Start);
		Fields.push_back(Value.substr(Start, Bar == std::string::npos ? std::string::npos : Bar - Start));
		if (Bar == std::string::npos)
		{
			break;
		}
		Start = Bar + 1;
	}
	return Fields;
}

std::string ResolvePath(const std::string& BaseDir, const std::string& Path)
{
	std::filesystem::path Result = std::filesystem::u8path(Path);
	if (Result.is_relative())
	{
		std::error_code Error;
		Result = BaseDir.empty() ? std::filesystem::absolute(Result, Error) : std::filesystem::u8path(BaseDir) / Result;
	}
	std::string Text = Result.lexically_normal().u8string();
	for (char& C : Text)
	{
		if (C == '\\')
		{
			C = '/';
		}
	}
	if (Text.size() > 1 && Text.back() == '/')
	{
		Text.pop_back();
	}
	return Text;
}

std::string GetDirectory(const std::string& Path)
{
	const size_t Slash = Path.find_last_of("/\\");
	return Slash == std::string::npos ? std::string() : Path.substr(0, Slash);
}

std::string GetFileName(const std::string& Path)
{
	const size_t Slash = Path.find_last_of("/\\");
	return Slash == std::string::npos ? Path : Path.substr(Slash + 1);
}
