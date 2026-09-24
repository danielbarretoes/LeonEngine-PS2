#pragma once

#include "RHIHandles.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

enum class EShaderReloadResult : std::uint8_t
{
	Unchanged = 0,
	Reloaded = 1,
	Failed = 2,
};

[[nodiscard]] inline EShaderReloadResult MergeShaderReload(EShaderReloadResult A, EShaderReloadResult B)
{
	if (A == EShaderReloadResult::Failed || B == EShaderReloadResult::Failed)
	{
		return EShaderReloadResult::Failed;
	}
	if (A == EShaderReloadResult::Reloaded || B == EShaderReloadResult::Reloaded)
	{
		return EShaderReloadResult::Reloaded;
	}
	return EShaderReloadResult::Unchanged;
}

/// GLSL program with cached uniform locations and optional disk hot-reload.
class RENDERER_API FShader
{
public:
	/// Called after a new program is linked and installed; return false to revert.
	using FAcceptFunction = std::function<bool()>;

	FShader() = default;
	~FShader();

	FShader(const FShader&) = delete;
	FShader& operator=(const FShader&) = delete;

	bool Create(const char* VertexSource, const char* FragmentSource);
	bool LoadFromFiles(const std::string& InVertexPath, const std::string& InFragmentPath);
	void Destroy();

	/// Recompile when file timestamps change (or force). Failed compiles keep the previous program.
	[[nodiscard]] EShaderReloadResult ReloadFromDiskIfChanged(const FAcceptFunction& Accept = {});
	[[nodiscard]] EShaderReloadResult ForceReloadFromDisk(const FAcceptFunction& Accept = {});

	void Bind() const;
	void SetMat4(const char* Name, const float* Value16) const;
	void SetMat4Array(const char* Name, const float* Values, int Count) const;
	void SetMat3(const char* Name, const float* Value9) const;
	void SetVec3(const char* Name, float X, float Y, float Z) const;
	void SetVec2(const char* Name, float X, float Y) const;
	void SetVec4(const char* Name, float X, float Y, float Z, float W) const;
	void SetFloat(const char* Name, float Value) const;
	void SetInt(const char* Name, int Value) const;

	/// Bind a named uniform block to a binding point (matches FUniformBuffer::Create).
	bool BindUniformBlock(const char* BlockName, unsigned int BindingPoint) const;

	[[nodiscard]] bool Valid() const
	{
		return Program != 0;
	}
	[[nodiscard]] FRHIProgramId ProgramId() const
	{
		return Program;
	}
	[[nodiscard]] bool HasFilePaths() const
	{
		return !VertexPath.empty() && !FragmentPath.empty();
	}

private:
	static unsigned int Compile(unsigned int Type, const char* Source);
	[[nodiscard]] int UniformLocation(const char* Name) const;
	EShaderReloadResult LoadFromStoredPaths(bool bForce, const FAcceptFunction& Accept);

	FRHIProgramId Program = InvalidProgram;
	mutable std::unordered_map<std::string, int> UniformCache;
	std::string VertexPath;
	std::string FragmentPath;
	std::filesystem::file_time_type VertexTime;
	std::filesystem::file_time_type FragmentTime;
};
