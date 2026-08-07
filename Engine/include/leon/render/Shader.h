#pragma once

#include <leon/rhi/RHIHandles.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

namespace leon {

enum class EShaderReloadResult : std::uint8_t {
    Unchanged = 0,
    Reloaded = 1,
    Failed = 2,
};

[[nodiscard]] inline EShaderReloadResult MergeShaderReload(EShaderReloadResult a,
                                                           EShaderReloadResult b) {
    if (a == EShaderReloadResult::Failed || b == EShaderReloadResult::Failed) {
        return EShaderReloadResult::Failed;
    }
    if (a == EShaderReloadResult::Reloaded || b == EShaderReloadResult::Reloaded) {
        return EShaderReloadResult::Reloaded;
    }
    return EShaderReloadResult::Unchanged;
}

/// GLSL program with cached uniform locations and optional disk hot-reload.
class Shader {
public:
    /// Called after a new program is linked and installed; return false to revert.
    using AcceptFn = std::function<bool()>;

    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool Create(const char* vertexSource, const char* fragmentSource);
    bool LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    void Destroy();

    /// Recompile when file timestamps change (or force). Failed compiles keep the previous program.
    [[nodiscard]] EShaderReloadResult ReloadFromDiskIfChanged(const AcceptFn& accept = {});
    [[nodiscard]] EShaderReloadResult ForceReloadFromDisk(const AcceptFn& accept = {});

    void Bind() const;
    void SetMat4(const char* name, const float* value16) const;
    void SetMat4Array(const char* name, const float* values, int count) const;
    void SetMat3(const char* name, const float* value9) const;
    void SetVec3(const char* name, float x, float y, float z) const;
    void SetVec2(const char* name, float x, float y) const;
    void SetVec4(const char* name, float x, float y, float z, float w) const;
    void SetFloat(const char* name, float value) const;
    void SetInt(const char* name, int value) const;

    /// Bind a named uniform block to a binding point (matches UniformBuffer::Create).
    bool BindUniformBlock(const char* blockName, unsigned int bindingPoint) const;

    [[nodiscard]] bool Valid() const { return program_ != 0; }
    [[nodiscard]] rhi::RHIProgramId ProgramId() const { return program_; }
    [[nodiscard]] bool HasFilePaths() const {
        return !vertexPath_.empty() && !fragmentPath_.empty();
    }

private:
    static unsigned int Compile(unsigned int type, const char* source);
    [[nodiscard]] int UniformLocation(const char* name) const;
    EShaderReloadResult LoadFromStoredPaths(bool force, const AcceptFn& accept);

    rhi::RHIProgramId program_ = rhi::kInvalidProgram;
    mutable std::unordered_map<std::string, int> uniformCache_;
    std::string vertexPath_;
    std::string fragmentPath_;
    std::filesystem::file_time_type vertexTime_;
    std::filesystem::file_time_type fragmentTime_;
};

} // namespace leon
