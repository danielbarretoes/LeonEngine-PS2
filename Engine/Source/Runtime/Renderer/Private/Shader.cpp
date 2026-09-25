#include "Shader.h"

#include "HAL/FileManager.h"
#include "Misc/CString.h"
#include "Misc/FileHelper.h"
#include "RendererLog.h"

#include <glad/glad.h>

namespace
{

	bool ReadFile(const FString& Path, FString& Out)
	{
		if (!FFileHelper::LoadFileToString(Out, *Path))
		{
			UE_LOG(LogRenderer, Error, "Failed to open shader file: %s", *Path);
			return false;
		}
		return true;
	}

	/** The file's modification time; false when the file is missing. */
	bool FileWriteTime(const FString& Path, FDateTime& OutTime)
	{
		OutTime = IFileManager::Get().GetTimeStamp(*Path);
		return OutTime != FDateTime::MinValue();
	}

	uint32 CompileShader(uint32 Type, const ANSICHAR* Source)
	{
		const uint32 Shader = glCreateShader(Type);
		glShaderSource(Shader, 1, &Source, nullptr);
		glCompileShader(Shader);

		int32 Success = 0;
		glGetShaderiv(Shader, GL_COMPILE_STATUS, &Success);
		if (Success == 0)
		{
			ANSICHAR InfoLog[1024] = {};
			glGetShaderInfoLog(Shader, static_cast<GLsizei>(sizeof(InfoLog)), nullptr, InfoLog);
			UE_LOG(LogRenderer, Error, "Shader compile error:\n%s", InfoLog);
			glDeleteShader(Shader);
			return 0;
		}
		return Shader;
	}

	bool LinkProgram(const ANSICHAR* VertexSource, const ANSICHAR* FragmentSource, uint32& OutProgram)
	{
		const uint32 Vertex = CompileShader(GL_VERTEX_SHADER, VertexSource);
		const uint32 Fragment = CompileShader(GL_FRAGMENT_SHADER, FragmentSource);
		if (Vertex == 0 || Fragment == 0)
		{
			if (Vertex != 0)
			{
				glDeleteShader(Vertex);
			}
			if (Fragment != 0)
			{
				glDeleteShader(Fragment);
			}
			return false;
		}

		const uint32 LocalProgram = glCreateProgram();
		glAttachShader(LocalProgram, Vertex);
		glAttachShader(LocalProgram, Fragment);
		glLinkProgram(LocalProgram);
		glDeleteShader(Vertex);
		glDeleteShader(Fragment);

		int32 Success = 0;
		glGetProgramiv(LocalProgram, GL_LINK_STATUS, &Success);
		if (Success == 0)
		{
			ANSICHAR InfoLog[1024] = {};
			glGetProgramInfoLog(LocalProgram, static_cast<GLsizei>(sizeof(InfoLog)), nullptr, InfoLog);
			UE_LOG(LogRenderer, Error, "Shader link error:\n%s", InfoLog);
			glDeleteProgram(LocalProgram);
			return false;
		}

		OutProgram = LocalProgram;
		return true;
	}

} // namespace

FShader::~FShader()
{
	Destroy();
}

bool FShader::Create(const ANSICHAR* VertexSource, const ANSICHAR* FragmentSource)
{
	uint32 NewProgram = 0;
	if (!LinkProgram(VertexSource, FragmentSource, NewProgram))
	{
		return false;
	}
	if (Program != 0)
	{
		glDeleteProgram(Program);
	}
	Program = NewProgram;
	UniformCache.Reset();
	return true;
}

bool FShader::LoadFromFiles(const FString& InVertexPath, const FString& InFragmentPath)
{
	FString VertexSource;
	FString FragmentSource;
	if (!ReadFile(InVertexPath, VertexSource) || !ReadFile(InFragmentPath, FragmentSource))
	{
		return false;
	}
	if (!Create(*VertexSource, *FragmentSource))
	{
		return false;
	}

	VertexPath = InVertexPath;
	FragmentPath = InFragmentPath;
	(void)FileWriteTime(VertexPath, VertexTime);
	(void)FileWriteTime(FragmentPath, FragmentTime);
	return true;
}

EShaderReloadResult FShader::LoadFromStoredPaths(bool bForce, const FAcceptFunction& Accept)
{
	if (!HasFilePaths())
	{
		return EShaderReloadResult::Failed;
	}

	FDateTime LocalVertexTime;
	FDateTime LocalFragmentTime;
	if (!FileWriteTime(VertexPath, LocalVertexTime) || !FileWriteTime(FragmentPath, LocalFragmentTime))
	{
		UE_LOG(LogRenderer, Warning, "Shader reload: missing file(s) %s / %s", *VertexPath, *FragmentPath);
		return EShaderReloadResult::Failed;
	}

	if (!bForce && LocalVertexTime == VertexTime && LocalFragmentTime == FragmentTime)
	{
		return EShaderReloadResult::Unchanged;
	}

	FString VertexSource;
	FString FragmentSource;
	if (!ReadFile(VertexPath, VertexSource) || !ReadFile(FragmentPath, FragmentSource))
	{
		// Advance the times so a briefly locked file does not retry every frame; the next real save retriggers.
		VertexTime = LocalVertexTime;
		FragmentTime = LocalFragmentTime;
		return EShaderReloadResult::Failed;
	}

	uint32 NewProgram = 0;
	if (!LinkProgram(*VertexSource, *FragmentSource, NewProgram))
	{
		UE_LOG(LogRenderer, Warning, "Shader reload failed; keeping the previous program (%s)", *VertexPath);
		VertexTime = LocalVertexTime;
		FragmentTime = LocalFragmentTime;
		return EShaderReloadResult::Failed;
	}

	const uint32 Previous = Program;
	Program = NewProgram;
	UniformCache.Reset();

	if (Accept && !Accept())
	{
		UE_LOG(LogRenderer, Warning, "Shader reload rejected by the validator; reverting (%s)", *VertexPath);
		glDeleteProgram(NewProgram);
		Program = Previous;
		UniformCache.Reset();
		VertexTime = LocalVertexTime;
		FragmentTime = LocalFragmentTime;
		return EShaderReloadResult::Failed;
	}

	if (Previous != 0)
	{
		glDeleteProgram(Previous);
	}
	VertexTime = LocalVertexTime;
	FragmentTime = LocalFragmentTime;
	UE_LOG(LogRenderer, Log, "Shader reloaded: %s + %s", *VertexPath, *FragmentPath);
	return EShaderReloadResult::Reloaded;
}

EShaderReloadResult FShader::ReloadFromDiskIfChanged(const FAcceptFunction& Accept)
{
	return LoadFromStoredPaths(false, Accept);
}

EShaderReloadResult FShader::ForceReloadFromDisk(const FAcceptFunction& Accept)
{
	return LoadFromStoredPaths(true, Accept);
}

void FShader::Destroy()
{
	if (Program != 0)
	{
		glDeleteProgram(Program);
		Program = 0;
	}
	UniformCache.Reset();
	VertexPath.Empty();
	FragmentPath.Empty();
	VertexTime = FDateTime();
	FragmentTime = FDateTime();
}

void FShader::Bind() const
{
	glUseProgram(Program);
}

void FShader::SetMat4(const ANSICHAR* Name, const float* Value16) const
{
	glUniformMatrix4fv(UniformLocation(Name), 1, GL_FALSE, Value16);
}

void FShader::SetMat4Array(const ANSICHAR* Name, const float* Values, int32 Count) const
{
	if (Count <= 0 || Values == nullptr)
	{
		return;
	}
	glUniformMatrix4fv(UniformLocation(Name), Count, GL_FALSE, Values);
}

void FShader::SetMat3(const ANSICHAR* Name, const float* Value9) const
{
	glUniformMatrix3fv(UniformLocation(Name), 1, GL_FALSE, Value9);
}

void FShader::SetVec3(const ANSICHAR* Name, float X, float Y, float Z) const
{
	glUniform3f(UniformLocation(Name), X, Y, Z);
}

void FShader::SetVec2(const ANSICHAR* Name, float X, float Y) const
{
	glUniform2f(UniformLocation(Name), X, Y);
}

void FShader::SetVec4(const ANSICHAR* Name, float X, float Y, float Z, float W) const
{
	glUniform4f(UniformLocation(Name), X, Y, Z, W);
}

void FShader::SetFloat(const ANSICHAR* Name, float Value) const
{
	glUniform1f(UniformLocation(Name), Value);
}

void FShader::SetInt(const ANSICHAR* Name, int32 Value) const
{
	glUniform1i(UniformLocation(Name), Value);
}

bool FShader::BindUniformBlock(const ANSICHAR* BlockName, uint32 BindingPoint) const
{
	if (!Valid() || BlockName == nullptr)
	{
		return false;
	}
	const uint32 BlockIndex = glGetUniformBlockIndex(Program, BlockName);
	if (BlockIndex == GL_INVALID_INDEX)
	{
		UE_LOG(LogRenderer, Error, "Uniform block not found: %s", BlockName);
		return false;
	}
	glUniformBlockBinding(Program, BlockIndex, BindingPoint);
	return true;
}

int32 FShader::UniformLocation(const ANSICHAR* Name) const
{
	for (const FUniformSlot& Slot : UniformCache)
	{
		if (FCStringAnsi::Strcmp(*Slot.Name, Name) == 0)
		{
			return Slot.Location;
		}
	}

	const int32 Location = glGetUniformLocation(Program, Name);
	UniformCache.Add(FUniformSlot{FString(Name), Location});
	return Location;
}
