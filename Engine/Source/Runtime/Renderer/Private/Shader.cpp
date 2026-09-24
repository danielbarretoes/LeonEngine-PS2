#include <glad/glad.h>

#include <array>
#include <fstream>
#include <iostream>
#include "Shader.h"
#include <sstream>
#include <system_error>

namespace {

bool ReadFile(const std::string& Path, std::string& Out) {
    std::ifstream File(Path, std::ios::in | std::ios::binary);
    if (!File.is_open()) {
        std::cerr << "Failed to open shader file: " << Path << '\n';
        return false;
    }
    std::ostringstream Ss;
    Ss << File.rdbuf();
    Out = Ss.str();
    return true;
}

bool FileWriteTime(const std::string& Path, std::filesystem::file_time_type& OutTime) {
    std::error_code Ec;
    OutTime = std::filesystem::last_write_time(Path, Ec);
    return !Ec;
}

unsigned int CompileShader(unsigned int Type, const char* Source) {
    const unsigned int Shader = glCreateShader(Type);
    glShaderSource(Shader, 1, &Source, nullptr);
    glCompileShader(Shader);

    int Success = 0;
    glGetShaderiv(Shader, GL_COMPILE_STATUS, &Success);
    if (Success == 0) {
        std::array<char, 1024> InfoLog{};
        glGetShaderInfoLog(Shader, static_cast<GLsizei>(InfoLog.size()), nullptr, InfoLog.data());
        std::cerr << "Shader compile error:\n" << InfoLog.data() << '\n';
        glDeleteShader(Shader);
        return 0;
    }
    return Shader;
}

bool LinkProgram(const char* VertexSource, const char* FragmentSource, unsigned int& OutProgram) {
    const unsigned int Vertex = CompileShader(GL_VERTEX_SHADER, VertexSource);
    const unsigned int Fragment = CompileShader(GL_FRAGMENT_SHADER, FragmentSource);
    if (Vertex == 0 || Fragment == 0) {
        if (Vertex != 0) {
            glDeleteShader(Vertex);
        }
        if (Fragment != 0) {
            glDeleteShader(Fragment);
        }
        return false;
    }

    const unsigned int LocalProgram = glCreateProgram();
    glAttachShader(LocalProgram, Vertex);
    glAttachShader(LocalProgram, Fragment);
    glLinkProgram(LocalProgram);
    glDeleteShader(Vertex);
    glDeleteShader(Fragment);

    int Success = 0;
    glGetProgramiv(LocalProgram, GL_LINK_STATUS, &Success);
    if (Success == 0) {
        std::array<char, 1024> InfoLog{};
        glGetProgramInfoLog(LocalProgram, static_cast<GLsizei>(InfoLog.size()), nullptr, InfoLog.data());
        std::cerr << "Shader link error:\n" << InfoLog.data() << '\n';
        glDeleteProgram(LocalProgram);
        return false;
    }

    OutProgram = LocalProgram;
    return true;
}

} // namespace

FShader::~FShader() {
    Destroy();
}

bool FShader::Create(const char* VertexSource, const char* FragmentSource) {
    unsigned int NewProgram = 0;
    if (!LinkProgram(VertexSource, FragmentSource, NewProgram)) {
        return false;
    }
    if (Program != 0) {
        glDeleteProgram(Program);
    }
    Program = NewProgram;
    UniformCache.clear();
    return true;
}

bool FShader::LoadFromFiles(const std::string& InVertexPath, const std::string& InFragmentPath) {
    std::string VertexSource;
    std::string FragmentSource;
    if (!ReadFile(InVertexPath, VertexSource) || !ReadFile(InFragmentPath, FragmentSource)) {
        return false;
    }
    if (!Create(VertexSource.c_str(), FragmentSource.c_str())) {
        return false;
    }

    VertexPath = InVertexPath;
    FragmentPath = InFragmentPath;
    (void)FileWriteTime(VertexPath, VertexTime);
    (void)FileWriteTime(FragmentPath, FragmentTime);
    return true;
}

EShaderReloadResult FShader::LoadFromStoredPaths(bool bForce, const FAcceptFunction& Accept) {
    if (!HasFilePaths()) {
        return EShaderReloadResult::Failed;
    }

    std::filesystem::file_time_type LocalVertexTime{};
    std::filesystem::file_time_type LocalFragmentTime{};
    if (!FileWriteTime(VertexPath, LocalVertexTime) || !FileWriteTime(FragmentPath, LocalFragmentTime)) {
        std::cerr << "Shader reload: missing file(s) " << VertexPath << " / " << FragmentPath
                  << '\n';
        return EShaderReloadResult::Failed;
    }

    if (!bForce && LocalVertexTime == VertexTime && LocalFragmentTime == FragmentTime) {
        return EShaderReloadResult::Unchanged;
    }

    std::string VertexSource;
    std::string FragmentSource;
    if (!ReadFile(VertexPath, VertexSource) || !ReadFile(FragmentPath, FragmentSource)) {
        // Advance mtimes so a briefly locked file does not spam every frame forever;
        // the next real save still bumps mtime and retriggers.
        VertexTime = LocalVertexTime;
        FragmentTime = LocalFragmentTime;
        return EShaderReloadResult::Failed;
    }

    unsigned int NewProgram = 0;
    if (!LinkProgram(VertexSource.c_str(), FragmentSource.c_str(), NewProgram)) {
        std::cerr << "Shader reload failed; keeping previous program (" << VertexPath << ")\n";
        VertexTime = LocalVertexTime;
        FragmentTime = LocalFragmentTime;
        return EShaderReloadResult::Failed;
    }

    const unsigned int Previous = Program;
    Program = NewProgram;
    UniformCache.clear();

    if (Accept && !Accept()) {
        std::cerr << "Shader reload rejected by validator; reverting (" << VertexPath << ")\n";
        glDeleteProgram(NewProgram);
        Program = Previous;
        UniformCache.clear();
        VertexTime = LocalVertexTime;
        FragmentTime = LocalFragmentTime;
        return EShaderReloadResult::Failed;
    }

    if (Previous != 0) {
        glDeleteProgram(Previous);
    }
    VertexTime = LocalVertexTime;
    FragmentTime = LocalFragmentTime;
    std::cout << "Shader reloaded: " << VertexPath << " + " << FragmentPath << '\n';
    return EShaderReloadResult::Reloaded;
}

EShaderReloadResult FShader::ReloadFromDiskIfChanged(const FAcceptFunction& Accept) {
    return LoadFromStoredPaths(false, Accept);
}

EShaderReloadResult FShader::ForceReloadFromDisk(const FAcceptFunction& Accept) {
    return LoadFromStoredPaths(true, Accept);
}

void FShader::Destroy() {
    if (Program != 0) {
        glDeleteProgram(Program);
        Program = 0;
    }
    UniformCache.clear();
    VertexPath.clear();
    FragmentPath.clear();
    VertexTime = {};
    FragmentTime = {};
}

void FShader::Bind() const {
    glUseProgram(Program);
}

void FShader::SetMat4(const char* Name, const float* Value16) const {
    glUniformMatrix4fv(UniformLocation(Name), 1, GL_FALSE, Value16);
}

void FShader::SetMat4Array(const char* Name, const float* Values, int Count) const {
    if (Count <= 0 || Values == nullptr) {
        return;
    }
    glUniformMatrix4fv(UniformLocation(Name), Count, GL_FALSE, Values);
}

void FShader::SetMat3(const char* Name, const float* Value9) const {
    glUniformMatrix3fv(UniformLocation(Name), 1, GL_FALSE, Value9);
}

void FShader::SetVec3(const char* Name, float X, float Y, float Z) const {
    glUniform3f(UniformLocation(Name), X, Y, Z);
}

void FShader::SetVec2(const char* Name, float X, float Y) const {
    glUniform2f(UniformLocation(Name), X, Y);
}

void FShader::SetVec4(const char* Name, float X, float Y, float Z, float W) const {
    glUniform4f(UniformLocation(Name), X, Y, Z, W);
}

void FShader::SetFloat(const char* Name, float Value) const {
    glUniform1f(UniformLocation(Name), Value);
}

void FShader::SetInt(const char* Name, int Value) const {
    glUniform1i(UniformLocation(Name), Value);
}

bool FShader::BindUniformBlock(const char* BlockName, unsigned int BindingPoint) const {
    if (!Valid() || BlockName == nullptr) {
        return false;
    }
    const unsigned int BlockIndex = glGetUniformBlockIndex(Program, BlockName);
    if (BlockIndex == GL_INVALID_INDEX) {
        std::cerr << "Uniform block not found: " << BlockName << '\n';
        return false;
    }
    glUniformBlockBinding(Program, BlockIndex, BindingPoint);
    return true;
}

unsigned int FShader::Compile(unsigned int Type, const char* Source) {
    return CompileShader(Type, Source);
}

int FShader::UniformLocation(const char* Name) const {
    if (const auto It = UniformCache.find(Name); It != UniformCache.end()) {
        return It->second;
    }

    const int Location = glGetUniformLocation(Program, Name);
    UniformCache.emplace(Name, Location);
    return Location;
}

