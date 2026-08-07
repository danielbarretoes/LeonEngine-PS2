#pragma once

#include <filesystem>
#include <leon/render/PostProcess.h>
#include <string>
#include <vector>

namespace leon::editor {

struct RecentProject {
    std::string path;
    std::string name;
    std::string displayName;
    /// Engine version that last opened this project (empty if unknown).
    std::string engineVersion;
};

struct EditorProjectInfo {
    std::string path;
    std::string name;
    std::string displayName;
    /// Last engine version written into `leon.game.json` (may be empty).
    std::string engineVersion;
    /// Optional start map from `leon.game.json` (`Levels/MainMenu.llev`, etc.).
    std::string defaultLevel;
    /// Optional pack GameMode when a level has no override (`defaultGameMode`).
    std::string defaultGameMode;
    /// Source template folder id (`Templates/<id>/`) when created via New Project. Fixed gameplay seed.
    std::string templateId;
    std::string description;
    /// When true, Build Game also builds `leon-<Name>-server` (headless dedicated).
    bool buildDedicatedServer = false;
    /// When true, `editorPostProcess` was present in `leon.game.json`.
    bool hasEditorPostProcess = false;
    /// Project-scoped editor Post Process prefs (`editorPostProcess` in leon.game.json).
    PostProcessSettings editorPostProcess{};
};

/// Project hub helpers: recent list, open/create/scaffold from project Templates/
/// (e.g. Blank, ThirdPerson). Distinct from editor New Level templates (Blank / Starter).
class EditorProjectService {
public:
    void LoadRecents();
    void SaveRecents() const;
    void Remember(const std::string& projectPath);

    [[nodiscard]] const std::vector<RecentProject>& Recents() const { return recents_; }

    /// True if `path` is a project folder (contains `leon.game.json`) or the marker file itself.
    [[nodiscard]] static bool ResolveProjectDirectory(const std::string& pathOrMarker,
                                                      std::string& outDirectory,
                                                      std::string& outError);

    [[nodiscard]] static bool ReadProjectInfo(const std::string& projectDirectory,
                                              EditorProjectInfo& out, std::string& outError);

    /// Stamp `engineVersion` into `leon.game.json` (creates/updates the field).
    [[nodiscard]] static bool WriteProjectEngineVersion(const std::string& projectDirectory,
                                                        const std::string& engineVersion,
                                                        std::string& outError);

    /// Stamp `defaultLevel` into `leon.game.json` (Unreal-like Game Default Map).
    /// Prefer `Levels/<Name>.llev` relative to project Content.
    [[nodiscard]] static bool WriteProjectDefaultLevel(const std::string& projectDirectory,
                                                       const std::string& defaultLevel,
                                                       std::string& outError);

    /// Stamp `defaultGameMode` into `leon.game.json` (empty clears the field).
    [[nodiscard]] static bool WriteProjectDefaultGameMode(const std::string& projectDirectory,
                                                          const std::string& defaultGameMode,
                                                          std::string& outError);

    /// Stamp `displayName` into `leon.game.json`.
    [[nodiscard]] static bool WriteProjectDisplayName(const std::string& projectDirectory,
                                                      const std::string& displayName,
                                                      std::string& outError);

    /// Stamp `description` into `leon.game.json` (empty clears the field).
    [[nodiscard]] static bool WriteProjectDescription(const std::string& projectDirectory,
                                                      const std::string& description,
                                                      std::string& outError);

    /// Stamp `buildDedicatedServer` into `leon.game.json` (omit when false).
    [[nodiscard]] static bool WriteProjectBuildDedicatedServer(const std::string& projectDirectory,
                                                               bool buildDedicatedServer,
                                                               std::string& outError);

    /// Persist editor Post Process prefs under `editorPostProcess` in `leon.game.json`.
    [[nodiscard]] static bool WriteProjectEditorPostProcess(const std::string& projectDirectory,
                                                            const PostProcessSettings& settings,
                                                            std::string& outError);

    /// Create `parent/<name>/` with pack files (uses Templates/Blank when present).
    [[nodiscard]] static bool CreateBlankProject(const std::string& parentDirectory,
                                                 const std::string& projectName,
                                                 const std::string& displayName,
                                                 std::string& outProjectPath,
                                                 std::string& outError);

    /// Copy project template `Templates/<templateId>/` into `parent/<name>/` as-is
    /// (Unreal-like fixed seed). Stamps `name` / `displayName` / `templateId` in leon.game.json.
    [[nodiscard]] static bool CreateFromTemplate(const std::string& parentDirectory,
                                                 const std::string& projectName,
                                                 const std::string& displayName,
                                                 const std::string& templateId,
                                                 std::string& outProjectPath,
                                                 std::string& outError);

    [[nodiscard]] static std::string DefaultProjectsRoot();
    [[nodiscard]] static std::string TemplatesRoot();
    [[nodiscard]] static std::vector<std::string> ListTemplateIds();

    [[nodiscard]] static bool IsValidProjectName(const std::string& name, std::string& outError);

private:
    std::vector<RecentProject> recents_;

    [[nodiscard]] static std::string RecentsFilePath();
    [[nodiscard]] static bool WriteTextFile(const std::filesystem::path& path,
                                            const std::string& contents, std::string& outError);
    /// Write the scaffolded `Levels/Main.llev` (ground plane + PlayerStart + sun).
    [[nodiscard]] static bool WriteStarterLevelFile(const std::filesystem::path& path,
                                                    std::string& outError);
    [[nodiscard]] static bool ScaffoldMinimalPack(const std::filesystem::path& projectDir,
                                                  const std::string& name,
                                                  const std::string& displayName,
                                                  std::string& outError);
};

} // namespace leon::editor
