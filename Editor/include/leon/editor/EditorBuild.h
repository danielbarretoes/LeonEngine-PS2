#pragma once

#include <string>

namespace leon::editor {

/// Invokes packaging / cmake scripts under `Scripts/` (async, Output Log).
class EditorBuild {
public:
    /// CMake-build the open game pack and export `<project>/Shipping/`.
    /// When `buildDedicatedServer`, also builds `leon-<Name>-server` (if the target exists).
    [[nodiscard]] static bool StartProjectBuild(const std::string& projectPath,
                                                const std::string& projectName,
                                                bool buildDedicatedServer = false);

    [[nodiscard]] static bool IsBusy();

    /// Best-effort cmake target name (`leon-<Name>`), preferring the first client
    /// `add_executable(leon-…)` (skips `*-server`).
    [[nodiscard]] static std::string ResolveCmakeTarget(const std::string& projectPath,
                                                        const std::string& projectName);
};

} // namespace leon::editor
