#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <leon/core/Paths.h>
#include <leon/editor/EditorBuild.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/EditorToast.h>
#include <regex>
#include <thread>
#include <vector>


#if defined(_WIN32)
#include <stdlib.h>
#endif

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

std::atomic<bool> g_buildBusy{false};

/// Walk upward from seeds until `Scripts/<scriptName>` is found (repo or Dist/LeonEditor).
[[nodiscard]] fs::path FindRepoRoot(const fs::path& projectPath = {}) {
    std::vector<fs::path> seeds;
    seeds.push_back(ExecutableDirectory());
    if (!projectPath.empty()) {
        seeds.push_back(projectPath);
    }
    std::error_code ec;
    seeds.push_back(fs::current_path(ec));

    for (fs::path start : seeds) {
        if (start.empty()) {
            continue;
        }
        start = start.lexically_normal();
        fs::path p = start;
        for (int i = 0; i < 8; ++i) {
            if (fs::is_regular_file(p / "Scripts" / "build-project.bat", ec) && !ec) {
                return p;
            }
            const fs::path parent = p.parent_path();
            if (parent.empty() || parent == p) {
                break;
            }
            p = parent;
        }
    }
    return {};
}

[[nodiscard]] fs::path FindScript(const fs::path& projectPath, const char* scriptFile) {
    const fs::path repo = FindRepoRoot(projectPath);
    if (repo.empty()) {
        return {};
    }
    const fs::path script = repo / "Scripts" / scriptFile;
    if (fs::is_regular_file(script)) {
        return script.lexically_normal();
    }
    return {};
}

void AppendBuildLine(const std::string& line) {
    if (line.find("ERROR") != std::string::npos || line.find("error C") != std::string::npos ||
        line.find("FAILED:") != std::string::npos) {
        EditorLogError(line);
    } else if (line.find("warning") != std::string::npos ||
               line.find("Warning") != std::string::npos) {
        EditorLogWarn(line);
    } else {
        EditorLogInfo(line);
    }
}

void RunScriptProcess(std::string label, std::string cmd, std::string successExtra) {
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        EditorLogError(label + ": failed to start process");
        g_buildBusy.store(false);
        return;
    }

    // cmd.exe + .bat exit codes are unreliable via _pclose; scripts echo LEON_BUILD_EXIT=N.
    bool sawExitMarker = false;
    int markerCode = 1;
    char buffer[512];
    std::string pending;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        pending += buffer;
        std::size_t pos = 0;
        while ((pos = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                static const char kMarker[] = "LEON_BUILD_EXIT=";
                if (line.rfind(kMarker, 0) == 0) {
                    sawExitMarker = true;
                    try {
                        markerCode = std::stoi(line.substr(sizeof(kMarker) - 1));
                    } catch (...) {
                        markerCode = 1;
                    }
                } else {
                    AppendBuildLine(line);
                }
            }
            pending.erase(0, pos + 1);
        }
    }
    if (!pending.empty()) {
        if (pending.back() == '\r') {
            pending.pop_back();
        }
        if (!pending.empty()) {
            static const char kMarker[] = "LEON_BUILD_EXIT=";
            if (pending.rfind(kMarker, 0) == 0) {
                sawExitMarker = true;
                try {
                    markerCode = std::stoi(pending.substr(sizeof(kMarker) - 1));
                } catch (...) {
                    markerCode = 1;
                }
            } else {
                AppendBuildLine(pending);
            }
        }
    }

    const int pcloseCode = _pclose(pipe);
    // Prefer script marker; else treat non-zero _pclose as failure (may be code<<8).
    int code = sawExitMarker ? markerCode : pcloseCode;
    if (!sawExitMarker && code != 0 && (code & 0xff) == 0) {
        code = code >> 8;
    }
    if (code == 0) {
        EditorLogInfo(label + ": succeeded");
        if (!successExtra.empty()) {
            EditorLogInfo(successExtra);
        }
        EditorToast(label + " succeeded", EEditorToastKind::Success, 4.5f);
    } else {
        EditorLogError(label + ": failed (exit " + std::to_string(code) + ")");
        EditorToast(label + " failed", EEditorToastKind::Error, 5.0f);
    }
#else
    (void)cmd;
    (void)successExtra;
    EditorLogError(label + ": only implemented on Windows");
#endif
    g_buildBusy.store(false);
}

[[nodiscard]] bool TryBeginBusy(const char* alreadyMsg) {
    bool expected = false;
    if (!g_buildBusy.compare_exchange_strong(expected, true)) {
        EditorLogWarn(alreadyMsg);
        return false;
    }
    return true;
}

} // namespace

bool EditorBuild::IsBusy() {
    return g_buildBusy.load();
}

std::string EditorBuild::ResolveCmakeTarget(const std::string& projectPath,
                                            const std::string& projectName) {
    const fs::path cmakeLists = fs::path(projectPath) / "CMakeLists.txt";
    std::ifstream in(cmakeLists);
    if (in.is_open()) {
        std::string contents((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        static const std::regex kExe(R"(add_executable\s*\(\s*(leon-[A-Za-z0-9_]+))");
        std::sregex_iterator it(contents.begin(), contents.end(), kExe);
        const std::sregex_iterator end;
        for (; it != end; ++it) {
            const std::string name = (*it)[1].str();
            if (name.size() >= 7 && name.compare(name.size() - 7, 7, "-server") == 0) {
                continue;
            }
            return name;
        }
    }
    if (!projectName.empty()) {
        return "leon-" + projectName;
    }
    return "leon-" + fs::path(projectPath).filename().string();
}

bool EditorBuild::StartProjectBuild(const std::string& projectPath, const std::string& projectName,
                                    bool buildDedicatedServer) {
    if (projectPath.empty()) {
        EditorLogError("Build Project: no project open");
        return false;
    }
    if (!TryBeginBusy("Build: already running")) {
        return false;
    }

    const fs::path proj = fs::path(projectPath).lexically_normal();
    if (!fs::is_regular_file(proj / "CMakeLists.txt")) {
        EditorLogError("Build Project: missing CMakeLists.txt in " + proj.string());
        g_buildBusy.store(false);
        return false;
    }

    const fs::path script = FindScript(proj, "build-project.bat");
    if (script.empty()) {
        EditorLogError("Build Project: Scripts/build-project.bat not found");
        EditorLogError("  Use Scripts\\package-editor.bat to make a portable Dist\\LeonEditor");
        EditorLogError("  exe: " + ExecutableDirectory().string());
        g_buildBusy.store(false);
        return false;
    }

    const std::string target = ResolveCmakeTarget(proj.string(), projectName);
    const std::string shipHint = "Game output: " + (proj / "Shipping").lexically_normal().string() +
                                 "  (copy that folder to another PC)";

    EditorLogInfo("Build Game: starting " + target +
                  (buildDedicatedServer ? " (+ dedicated server)" : "") + " …");
    EditorLogInfo("  project: " + proj.string());
    EditorLogInfo("  script:  " + script.string());
    EditorLogInfo("  output:  <project>/Shipping/  (and build-fast/ while compiling)");

    std::string cmd = "cmd.exe /c \"\"";
    cmd += script.string();
    cmd += "\" \"";
    cmd += proj.string();
    cmd += "\" \"";
    cmd += target;
    cmd += "\"";
    if (buildDedicatedServer) {
        cmd += " --with-server";
    }
    cmd += "\" 2>&1";

    std::thread(RunScriptProcess, std::string("Build Game"), std::move(cmd), shipHint).detach();
    return true;
}

} // namespace leon::editor
