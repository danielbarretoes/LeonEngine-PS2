#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "RHIHandles.h"
#include "ShaderCore.h"

/** GLSL program with cached uniform locations and optional hot reload from disk. */
class RENDERER_API FShader
{
public:
	/** Called after a new program is linked and installed; return false to revert. */
	using FAcceptFunction = TFunction<bool()>;

	FShader() = default;
	~FShader();

	FShader(const FShader&) = delete;
	FShader& operator=(const FShader&) = delete;

	bool Create(const ANSICHAR* VertexSource, const ANSICHAR* FragmentSource);
	bool LoadFromFiles(const FString& InVertexPath, const FString& InFragmentPath);
	void Destroy();

	/** Recompiles when the file timestamps change (or when forced). A failed compile keeps the previous program. */
	[[nodiscard]] EShaderReloadResult ReloadFromDiskIfChanged(const FAcceptFunction& Accept = {});
	[[nodiscard]] EShaderReloadResult ForceReloadFromDisk(const FAcceptFunction& Accept = {});

	void Bind() const;
	void SetMat4(const ANSICHAR* Name, const float* Value16) const;
	/** Uploads the 16 floats as they are: GLSL sees the transpose, so its M * v is the row-vector v * M. */
	void SetMat4(const ANSICHAR* Name, const FMatrix& Value) const;
	void SetMat4Array(const ANSICHAR* Name, const float* Values, int32 Count) const;
	void SetMat3(const ANSICHAR* Name, const float* Value9) const;
	void SetVec3(const ANSICHAR* Name, float X, float Y, float Z) const;
	void SetVec2(const ANSICHAR* Name, float X, float Y) const;
	void SetVec4(const ANSICHAR* Name, float X, float Y, float Z, float W) const;
	void SetFloat(const ANSICHAR* Name, float Value) const;
	void SetInt(const ANSICHAR* Name, int32 Value) const;

	/** Binds a named uniform block to a binding point (matches FUniformBuffer::Create). */
	bool BindUniformBlock(const ANSICHAR* BlockName, uint32 BindingPoint) const;

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
		return !VertexPath.IsEmpty() && !FragmentPath.IsEmpty();
	}

private:
	/** A cached uniform location (GLSL names are case-sensitive). */
	struct FUniformSlot
	{
		FString Name;
		int32 Location = -1;
	};

	[[nodiscard]] int32 UniformLocation(const ANSICHAR* Name) const;
	EShaderReloadResult LoadFromStoredPaths(bool bForce, const FAcceptFunction& Accept);

	FRHIProgramId Program = InvalidProgram;
	mutable TArray<FUniformSlot> UniformCache;
	FString VertexPath;
	FString FragmentPath;
	FDateTime VertexTime;
	FDateTime FragmentTime;
};
