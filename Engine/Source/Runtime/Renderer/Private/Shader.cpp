#include <glad/glad.h>

#include <array>
#include <fstream>
#include <iostream>
#include "Shader.h"
#include <sstream>
#include <system_error>

namespace {

bool readFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << path << '\n';
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

bool fileWriteTime(const std::string& path, std::filesystem::file_time_type& outTime) {
    std::error_code ec;
    outTime = std::filesystem::last_write_time(path, ec);
    return !ec;
}

unsigned int compileShader(unsigned int type, const char* source) {
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == 0) {
        std::array<char, 1024> infoLog{};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "Shader compile error:\n" << infoLog.data() << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool linkProgram(const char* vertexSource, const char* fragmentSource, unsigned int& outProgram) {
    const unsigned int vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    const unsigned int fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) {
            glDeleteShader(vertex);
        }
        if (fragment != 0) {
            glDeleteShader(fragment);
        }
        return false;
    }

    const unsigned int program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == 0) {
        std::array<char, 1024> infoLog{};
        glGetProgramInfoLog(program, static_cast<GLsizei>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "Shader link error:\n" << infoLog.data() << '\n';
        glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

} // namespace

Shader::~Shader() {
    Destroy();
}

bool Shader::Create(const char* vertexSource, const char* fragmentSource) {
    unsigned int newProgram = 0;
    if (!linkProgram(vertexSource, fragmentSource, newProgram)) {
        return false;
    }
    if (program_ != 0) {
        glDeleteProgram(program_);
    }
    program_ = newProgram;
    uniformCache_.clear();
    return true;
}

bool Shader::LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertexSource;
    std::string fragmentSource;
    if (!readFile(vertexPath, vertexSource) || !readFile(fragmentPath, fragmentSource)) {
        return false;
    }
    if (!Create(vertexSource.c_str(), fragmentSource.c_str())) {
        return false;
    }

    vertexPath_ = vertexPath;
    fragmentPath_ = fragmentPath;
    (void)fileWriteTime(vertexPath_, vertexTime_);
    (void)fileWriteTime(fragmentPath_, fragmentTime_);
    return true;
}

EShaderReloadResult Shader::LoadFromStoredPaths(bool force, const AcceptFn& accept) {
    if (!HasFilePaths()) {
        return EShaderReloadResult::Failed;
    }

    std::filesystem::file_time_type vertexTime{};
    std::filesystem::file_time_type fragmentTime{};
    if (!fileWriteTime(vertexPath_, vertexTime) || !fileWriteTime(fragmentPath_, fragmentTime)) {
        std::cerr << "Shader reload: missing file(s) " << vertexPath_ << " / " << fragmentPath_
                  << '\n';
        return EShaderReloadResult::Failed;
    }

    if (!force && vertexTime == vertexTime_ && fragmentTime == fragmentTime_) {
        return EShaderReloadResult::Unchanged;
    }

    std::string vertexSource;
    std::string fragmentSource;
    if (!readFile(vertexPath_, vertexSource) || !readFile(fragmentPath_, fragmentSource)) {
        // Advance mtimes so a briefly locked file does not spam every frame forever;
        // the next real save still bumps mtime and retriggers.
        vertexTime_ = vertexTime;
        fragmentTime_ = fragmentTime;
        return EShaderReloadResult::Failed;
    }

    unsigned int newProgram = 0;
    if (!linkProgram(vertexSource.c_str(), fragmentSource.c_str(), newProgram)) {
        std::cerr << "Shader reload failed; keeping previous program (" << vertexPath_ << ")\n";
        vertexTime_ = vertexTime;
        fragmentTime_ = fragmentTime;
        return EShaderReloadResult::Failed;
    }

    const unsigned int previous = program_;
    program_ = newProgram;
    uniformCache_.clear();

    if (accept && !accept()) {
        std::cerr << "Shader reload rejected by validator; reverting (" << vertexPath_ << ")\n";
        glDeleteProgram(newProgram);
        program_ = previous;
        uniformCache_.clear();
        vertexTime_ = vertexTime;
        fragmentTime_ = fragmentTime;
        return EShaderReloadResult::Failed;
    }

    if (previous != 0) {
        glDeleteProgram(previous);
    }
    vertexTime_ = vertexTime;
    fragmentTime_ = fragmentTime;
    std::cout << "Shader reloaded: " << vertexPath_ << " + " << fragmentPath_ << '\n';
    return EShaderReloadResult::Reloaded;
}

EShaderReloadResult Shader::ReloadFromDiskIfChanged(const AcceptFn& accept) {
    return LoadFromStoredPaths(false, accept);
}

EShaderReloadResult Shader::ForceReloadFromDisk(const AcceptFn& accept) {
    return LoadFromStoredPaths(true, accept);
}

void Shader::Destroy() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    uniformCache_.clear();
    vertexPath_.clear();
    fragmentPath_.clear();
    vertexTime_ = {};
    fragmentTime_ = {};
}

void Shader::Bind() const {
    glUseProgram(program_);
}

void Shader::SetMat4(const char* name, const float* value16) const {
    glUniformMatrix4fv(UniformLocation(name), 1, GL_FALSE, value16);
}

void Shader::SetMat4Array(const char* name, const float* values, int count) const {
    if (count <= 0 || values == nullptr) {
        return;
    }
    glUniformMatrix4fv(UniformLocation(name), count, GL_FALSE, values);
}

void Shader::SetMat3(const char* name, const float* value9) const {
    glUniformMatrix3fv(UniformLocation(name), 1, GL_FALSE, value9);
}

void Shader::SetVec3(const char* name, float x, float y, float z) const {
    glUniform3f(UniformLocation(name), x, y, z);
}

void Shader::SetVec2(const char* name, float x, float y) const {
    glUniform2f(UniformLocation(name), x, y);
}

void Shader::SetVec4(const char* name, float x, float y, float z, float w) const {
    glUniform4f(UniformLocation(name), x, y, z, w);
}

void Shader::SetFloat(const char* name, float value) const {
    glUniform1f(UniformLocation(name), value);
}

void Shader::SetInt(const char* name, int value) const {
    glUniform1i(UniformLocation(name), value);
}

bool Shader::BindUniformBlock(const char* blockName, unsigned int bindingPoint) const {
    if (!Valid() || blockName == nullptr) {
        return false;
    }
    const unsigned int blockIndex = glGetUniformBlockIndex(program_, blockName);
    if (blockIndex == GL_INVALID_INDEX) {
        std::cerr << "Uniform block not found: " << blockName << '\n';
        return false;
    }
    glUniformBlockBinding(program_, blockIndex, bindingPoint);
    return true;
}

unsigned int Shader::Compile(unsigned int type, const char* source) {
    return compileShader(type, source);
}

int Shader::UniformLocation(const char* name) const {
    if (const auto it = uniformCache_.find(name); it != uniformCache_.end()) {
        return it->second;
    }

    const int location = glGetUniformLocation(program_, name);
    uniformCache_.emplace(name, location);
    return location;
}

