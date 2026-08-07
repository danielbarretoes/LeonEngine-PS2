#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#endif

namespace {

namespace fs = std::filesystem;

[[nodiscard]] fs::path SiblingLeonCook(const char* argv0) {
#if defined(_WIN32)
    // Prefer the real module path so "leon-cli" from PATH still finds leon-cook beside it.
    wchar_t modulePath[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        return fs::path(modulePath).parent_path() / L"leon-cook.exe";
    }
#endif
    const fs::path self = fs::absolute(fs::path(argv0 != nullptr ? argv0 : "leon-cli"));
#if defined(_WIN32)
    return self.parent_path() / "leon-cook.exe";
#else
    return self.parent_path() / "leon-cook";
#endif
}

[[nodiscard]] int NormalizeSystemStatus(int status) {
    if (status == -1) {
        return 1; // system() failed to spawn
    }
#if defined(_WIN32)
    return status;
#else
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
#endif
}

[[nodiscard]] int RunLeonCookRecipe(const fs::path& cookExe, const std::string& recipePath) {
    const std::string cmd = "\"" + cookExe.string() + "\" recipe \"" + recipePath + "\"";
    return NormalizeSystemStatus(std::system(cmd.c_str()));
}

} // namespace

/// Minimal Tools CLI — no Engine link; forwards cook to sibling leon-cook.
// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv) {
    try {
        const std::string cmd = (argc > 1 && argv[1] != nullptr) ? argv[1] : "";
        if (cmd == "help" || cmd.empty()) {
            std::cout << "leon-cli — Leon offline tools\n"
                      << "  help                 Show this help\n"
                      << "  cook <recipe.json>   Run leon-cook recipe (sibling binary)\n"
                      << "  version              Print tools version\n";
            return cmd.empty() ? 1 : 0;
        }
        if (cmd == "version") {
            std::cout << "Leon Tools 0.9.0\n";
            return 0;
        }
        if (cmd == "cook") {
            if (argc < 3 || argv[2] == nullptr || argv[2][0] == '\0') {
                std::cerr << "Usage: leon-cli cook <recipe.json>\n"
                          << "  (or Scripts\\cook.bat <recipe.json>)\n";
                return 1;
            }
            const fs::path cookExe = SiblingLeonCook(argv[0]);
            if (!fs::exists(cookExe)) {
                std::cerr << "leon-cook not found next to leon-cli (" << cookExe.string()
                          << ").\nBuild Tools or use Scripts\\cook.bat\n";
                return 1;
            }
            return RunLeonCookRecipe(cookExe, argv[2]);
        }
        std::cerr << "Unknown command '" << cmd << "'. Try: leon-cli help\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << '\n';
        return 1;
    }
}
