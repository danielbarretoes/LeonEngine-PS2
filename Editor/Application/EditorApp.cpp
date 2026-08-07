// Windows before glad: glad defines APIENTRY when unset; windows.h then C4005-redefines it.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <filesystem>
#include <vector>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <iostream>
#include <leon/core/Ascii.h>
#include <leon/core/EKey.h>
#include <leon/core/Paths.h>
#include <leon/core/Window.h>
#include <leon/editor/EditorApp.h>
#include <leon/editor/EditorBuild.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/EditorTheme.h>
#include <leon/editor/EditorToast.h>
#include <leon/editor/LightmapBaker.h>
#include <leon/editor/PieGameMode.h>
#include <leon/editor/PiePackRegistry.h>
#include <leon/gameplay/DefaultGameMode.h>
#include <leon/gameplay/GameInstance.h>
#include <leon/level/LeonLevelFormat.h>
#include <leon/level/LevelLoader.h>
#include <leon/level/LightmapIO.h>
#include <leon/net/NetProtocol.h>
#include <string>
#include <string_view>

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

void ApplyLeonWindowIcon(Window& window) {
    const std::string iconPath = ResolveAssetPath("assets/icons/LeonEditor.png");
    if (iconPath.empty()) {
        return;
    }
    if (!window.SetIconFromFile(iconPath.c_str())) {
        std::cerr << "Editor: failed to load window icon: " << iconPath << '\n';
    }
}

/// Exact open-level catalog key for in-process PIE (includes MainMenu / Lobby).
[[nodiscard]] std::string CurrentEditorLevelKey(const EditorContext& ctx) {
    if (ctx.level != nullptr && !ctx.level->Name().empty()) {
        return ctx.level->Name();
    }
    if (!ctx.levelPath.empty()) {
        return fs::path(ctx.levelPath).stem().string();
    }
    return {};
}

/// Level catalog key for multi Shipping spawn (avoid front-end maps as join targets).
[[nodiscard]] std::string CurrentPiePlayMapKey(const EditorContext& ctx) {
    std::string key = CurrentEditorLevelKey(ctx);
    if (key.empty() || key == "MainMenu" || key == "Lobby") {
        return "Courtyard";
    }
    return key;
}

[[nodiscard]] fs::path FindPackShippingExecutable(const std::string& projectPath,
                                                   const std::string& projectName) {
    if (projectPath.empty()) {
        return {};
    }
    const std::string target = EditorBuild::ResolveCmakeTarget(projectPath, projectName);
    // Prefer Shipping/, then local build-fast next to the pack.
    const fs::path candidates[] = {
        fs::path(projectPath) / "Shipping" / (target + ".exe"),
        fs::path(projectPath) / "build-fast" / (target + ".exe"),
        fs::path(projectPath) / "Shipping" / (projectName + ".exe"),
    };
    std::error_code ec;
    for (const fs::path& c : candidates) {
        if (fs::is_regular_file(c, ec) && !ec) {
            return c;
        }
    }
    // Fallback: first non-server .exe under Shipping/
    const fs::path ship = fs::path(projectPath) / "Shipping";
    if (fs::is_directory(ship, ec)) {
        for (const fs::directory_entry& entry : fs::directory_iterator(ship, ec)) {
            if (ec || !entry.is_regular_file(ec)) {
                continue;
            }
            std::string name = entry.path().filename().string();
            std::string lower = name;
            for (char& ch : lower) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".exe") {
                continue;
            }
            if (lower.find("server") != std::string::npos) {
                continue;
            }
            return entry.path();
        }
    }
    return {};
}

#ifdef _WIN32
[[nodiscard]] bool SpawnDetachedProcess(const fs::path& exe, const std::string& args,
                                        std::uint32_t& outPid) {
    outPid = 0;
    if (exe.empty()) {
        return false;
    }
    std::string cmd = "\"" + exe.lexically_normal().string() + "\"";
    if (!args.empty()) {
        cmd += " ";
        cmd += args;
    }
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> cmdline(cmd.begin(), cmd.end());
    cmdline.push_back('\0');
    const std::string workDir = exe.parent_path().string();
    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        workDir.empty() ? nullptr : workDir.c_str(), &si, &pi)) {
        return false;
    }
    outPid = static_cast<std::uint32_t>(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

/// True when something (Shipping --listen) already bound UDP `port` on this machine.
[[nodiscard]] bool IsUdpPortInUse(std::uint16_t port) {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    const bool inUse = (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR &&
                        WSAGetLastError() == WSAEADDRINUSE);
    closesocket(sock);
    WSACleanup();
    return inUse;
}

#endif

} // namespace

bool EditorApp::Initialize(Engine& engine) {
    ctx_.engine = &engine;
    ctx_.level = &engine.GetLevel();
    ctx_.world = &world_;
    ctx_.camera = &engine.GetCamera();
    ctx_.renderer = &engine.GetRenderer();
    ctx_.window = &engine.GetWindow();
    ctx_.resources = &engine.GetResources();
    ctx_.catalog = &catalog_;
    ctx_.history = &history_;

    engine.SetCursorCaptured(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetSuppressCameraDrag(true);
    ApplyLeonWindowIcon(engine.GetWindow());

    GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(engine.GetWindow().NativeHandle());
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Persist docking / window layout beside the executable (Window → Save Layout).
    imguiIniPath_ = EditorLayout::LayoutIniPath();
    io.IniFilename = imguiIniPath_.c_str();
    ApplyEditorTheme();
    (void)LoadEditorFonts();

    ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    imguiReady_ = true;
    EditorOutputLog::Instance().InstallStreamTee();

    projects_.LoadRecents();
    welcome_.Reset();
    projectOpen_ = false;
    return true;
}

bool EditorApp::OpenProject(Engine& engine, const std::string& projectPath) {
    EditorProjectInfo info;
    std::string err;
    if (!EditorProjectService::ReadProjectInfo(projectPath, info, err)) {
        std::cerr << "Editor: cannot open project: " << err << '\n';
        return false;
    }

    if (ctx_.piePlaying) {
        StopPie(engine);
    }

    ctx_.ClearSelection();
    ctx_.dirty = false;
    ctx_.levelPath.clear();
    ctx_.pendingOpenPath.clear();
    ctx_.projectPath = info.path;
    ctx_.projectName = info.name;
    ctx_.projectDisplayName = info.displayName;
    ctx_.projectDefaultLevel = info.defaultLevel;
    ctx_.projectDefaultGameMode = info.defaultGameMode;
    ctx_.projectTemplateId = info.templateId;
    ctx_.projectDescription = info.description;
    ctx_.projectBuildDedicatedServer = info.buildDedicatedServer;
    ctx_.requestCloseProject = false;
    ctx_.pendingCloseProject = false;
    ctx_.pendingExit = false;
    ctx_.requestContentRefresh = true;
    SetActiveContentRoot(info.path);

    if (info.hasEditorPostProcess) {
        engine.GetRenderer().SetPostProcessSettings(info.editorPostProcess);
    }

    if (!catalog_.ScanPack(info.path)) {
        std::cerr << "Editor: project has no levels under '" << info.path
                  << "/Content/Levels'\n";
    }

    std::string defaultPath;
    if (!catalog_.IsEmpty()) {
        defaultPath = catalog_.Entries().front().path;
        if (!info.defaultLevel.empty()) {
            const std::string key = std::filesystem::path(info.defaultLevel).stem().string();
            const std::size_t idx = catalog_.FindIndexByLevelKey(key);
            if (idx < catalog_.NumEntries()) {
                defaultPath = catalog_.Entries()[idx].path;
            }
        }
    }
    if (!defaultPath.empty()) {
        if (LoadLevelFile(engine, defaultPath)) {
            ctx_.levelPath = defaultPath;
            ctx_.dirty = false;
            ReloadLevelLightmapsForPath(engine.GetLevel(), defaultPath);
        } else {
            std::cerr << "Editor: failed to load project level '" << defaultPath
                      << "' (check materials under the pack / ContentValidator)\n";
        }
    }

    history_.Clear();
    history_.Capture(ctx_);
    projects_.Remember(info.path);
    projectOpen_ = true;
    return true;
}

void EditorApp::CloseProject(Engine& engine) {
    if (ctx_.piePlaying) {
        StopPie(engine);
    }
    ctx_.ClearSelection();
    ctx_.dirty = false;
    ctx_.levelPath.clear();
    ctx_.pendingOpenPath.clear();
    ctx_.projectPath.clear();
    ctx_.projectName.clear();
    ctx_.projectDisplayName.clear();
    ctx_.projectDefaultLevel.clear();
    ctx_.projectDefaultGameMode.clear();
    ctx_.projectTemplateId.clear();
    ctx_.projectDescription.clear();
    ctx_.requestCloseProject = false;
    ctx_.pendingCloseProject = false;
    ctx_.pendingExit = false;
    SetActiveContentRoot({});
    catalog_ = LevelCatalog{};
    ctx_.catalog = &catalog_;
    projectOpen_ = false;
    projects_.LoadRecents();
    welcome_.Reset();
}

void EditorApp::Shutdown() {
    if (ctx_.piePlaying && ctx_.engine != nullptr) {
        StopPie(*ctx_.engine);
    }
    DestroyPiePresentResources();
    pieTarget_.Destroy();
    if (imguiReady_) {
        if (const char* ini = ImGui::GetIO().IniFilename; ini != nullptr && ini[0] != '\0') {
            ImGui::SaveIniSettingsToDisk(ini);
        }
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
    layout_.Viewport().Target().Destroy();
}

void EditorApp::BeginImGuiFrame() {
    ImGuiIO& io = ImGui::GetIO();
    // Selected Viewport PIE: GLFW hides the cursor but keeps feeding mouse coords (often
    // warped to the window center). Without this, ImGui still hovers/clicks menus & docks.
    const bool lockEditorUiMouse = ctx_.piePlaying && !ctx_.pieNewWindow && !ctx_.piePaused;
    if (lockEditorUiMouse) {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    if (lockEditorUiMouse) {
        io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
        io.MousePosPrev = ImVec2(-FLT_MAX, -FLT_MAX);
        for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); ++i) {
            io.MouseDown[i] = false;
            io.MouseClicked[i] = false;
            io.MouseDoubleClicked[i] = false;
            io.MouseReleased[i] = false;
        }
        io.MouseWheel = 0.0f;
        io.MouseWheelH = 0.0f;
        io.WantCaptureMouse = false;
    }

    ImGui::NewFrame();
    // Required each frame before Manipulate / IsUsing / IsOver.
    ImGuizmo::BeginFrame();
}

void EditorApp::EndImGuiFrame() {
    ImGui::Render();
    int fbW = 0;
    int fbH = 0;
    ctx_.window->GetFramebufferSize(fbW, fbH);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbW, fbH);
    glClearColor(18.0f / 255.0f, 18.0f / 255.0f, 18.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void EditorApp::UpdateWindowTitle(Engine& engine) {
    std::string title = "Leon Editor";
    if (!projectOpen_) {
        title += " — Welcome";
        glfwSetWindowTitle(static_cast<GLFWwindow*>(engine.GetWindow().NativeHandle()), title.c_str());
        return;
    }
    if (!ctx_.projectDisplayName.empty()) {
        title += " — ";
        title += ctx_.projectDisplayName;
    } else if (!ctx_.projectName.empty()) {
        title += " — ";
        title += ctx_.projectName;
    }
    if (ctx_.piePlaying) {
        title += ctx_.piePaused ? " [PAUSED]" : " [PIE]";
    }
    if (!ctx_.levelPath.empty()) {
        title += " — ";
        title += ctx_.levelPath;
    } else if (ctx_.level != nullptr && !ctx_.level->Name().empty()) {
        title += " — ";
        title += ctx_.level->Name();
    }
    if (ctx_.dirty) {
        title += " *";
    }
    glfwSetWindowTitle(static_cast<GLFWwindow*>(engine.GetWindow().NativeHandle()), title.c_str());
}

void EditorApp::SaveEditorCamera(const Camera& camera) {
    editorCameraBackup_.SetMode(camera.Mode());
    editorCameraBackup_.SetTarget(camera.Target());
    editorCameraBackup_.SetEyeLocation(camera.EyeLocation());
    editorCameraBackup_.SetDistance(camera.Distance());
    editorCameraBackup_.SetYawPitch(camera.YawDegrees(), camera.PitchDegrees());
    editorCameraBackup_.SetFieldOfView(camera.FieldOfView());
    editorCameraSaved_ = true;
}

void EditorApp::RestoreEditorCamera(Camera& camera) const {
    if (!editorCameraSaved_) {
        return;
    }
    camera.SetMode(editorCameraBackup_.Mode());
    camera.SetTarget(editorCameraBackup_.Target());
    camera.SetEyeLocation(editorCameraBackup_.EyeLocation());
    camera.SetDistance(editorCameraBackup_.Distance());
    camera.SetYawPitch(editorCameraBackup_.YawDegrees(), editorCameraBackup_.PitchDegrees());
    camera.SetFieldOfView(editorCameraBackup_.FieldOfView());
}

void EditorApp::StartPie(Engine& engine) {
    if (ctx_.piePlaying) {
        return;
    }

    ctx_.pieNumberOfPlayers = std::clamp(ctx_.pieNumberOfPlayers, 1, leon::net::kMaxPlayers);
    ctx_.piePaused = false;
    ctx_.requestPiePauseToggle = false;
    ctx_.requestPieStart = false;
    pieShippingSessionOnly_ = false;

    // Unreal Number of Players = total game instances.
    // Listen Server / Client with N>1 → exactly N Shipping windows (editor does not add a Pie pawn).
    if ((ctx_.pieNetMode == EEditorPlayNetMode::ListenServer ||
         ctx_.pieNetMode == EEditorPlayNetMode::Client) &&
        ctx_.pieNumberOfPlayers > 1) {
        pieShippingSessionOnly_ = true;
        pieMode_ = nullptr;
        ctx_.pieNewWindow = false;
        ctx_.editorViewCamera = nullptr;
        engine.SetPlayInputWindow(nullptr);
        engine.SetPlayMouseLookActive(false);
        engine.SetCursorCaptured(false);
        ctx_.world = &world_;
        SpawnPieExtraInstances();
        ctx_.piePlaying = true;
        ctx_.ClearSelection();
        EditorLogInfo("PIE multiplayer session: " + std::to_string(ctx_.pieNumberOfPlayers) +
                      " Shipping window(s), editor stays in edit mode (Stop kills them)");
        EditorToast("Playing " + std::to_string(ctx_.pieNumberOfPlayers) +
                        " Shipping instances — editor stays editable; Stop closes them",
                    EEditorToastKind::Info, 5.0f);
        return;
    }

    SaveEditorCamera(engine.GetCamera());
    ctx_.pieNewWindow = ctx_.playMode == EEditorPlayMode::NewEditorWindow;

    if (ctx_.pieNewWindow) {
        if (!pieWindow_.CreateShared(engine.GetWindow(), 1280, 720, "Leon Play")) {
            std::cerr << "Editor: failed to open Play window; falling back to Selected Viewport\n";
            ctx_.pieNewWindow = false;
        } else {
            ApplyLeonWindowIcon(pieWindow_);
            engine.SetPlayInputWindow(&pieWindow_);
            ctx_.editorViewCamera = &editorCameraBackup_;
        }
    } else {
        engine.SetPlayInputWindow(nullptr);
        ctx_.editorViewCamera = nullptr;
    }

    pieMode_ = nullptr;
    pieUsesHostSession_ = false;
    pieRestoreLevelPath_ = ctx_.levelPath;
    ctx_.piePaintPlayOverlay = {};

    // Level override → project defaultGameMode → third-person.
    std::string gm = "third-person";
    if (ctx_.level != nullptr && !ctx_.level->GameMode().empty()) {
        gm = ctx_.level->GameMode();
    } else if (!ctx_.projectDefaultGameMode.empty()) {
        gm = ctx_.projectDefaultGameMode;
    }

    // Flow: PIE GameMode selection (Unreal-like)
    // 1. Explicit Default → free-look DefaultGameMode (no pack session)
    // 2. Known pack → GameHostSession + RegisterModes (same as Shipping)
    // 3. third-person / Pie / unknown → PieGameMode preview fallback
    // Prefer folder name; New Project copies keep fixed gameplay via templateId.
    PiePackInfo packInfo = FindPiePack(ctx_.projectName);
    if (!packInfo.known && !ctx_.projectTemplateId.empty()) {
        packInfo = FindPiePack(ctx_.projectTemplateId);
    }
    if (gm == "Default" || gm == "default") {
        pieMode_ = std::make_unique<DefaultGameMode>();
        ctx_.world = &pieMode_->GetWorld();
        engine.GetGameInstance().SetLevelTravelFn([this](Engine& e, std::string_view levelKey) -> bool {
            fs::path levelsDir;
            if (!ctx_.projectPath.empty()) {
                levelsDir = ProjectContentDirectory(ctx_.projectPath) / "Levels";
            } else if (!ctx_.levelPath.empty()) {
                levelsDir = fs::path(ctx_.levelPath).parent_path();
            }
            if (levelsDir.empty()) {
                return false;
            }
            const std::string needle = AsciiToLower(levelKey);
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(levelsDir, ec)) {
                if (ec || !entry.is_regular_file()) {
                    continue;
                }
                const fs::path path = entry.path();
                if (AsciiToLower(path.extension().string()) != ".llev") {
                    continue;
                }
                bool match = AsciiToLower(path.stem().string()) == needle;
                if (!match) {
                    LevelDocument doc;
                    if (LoadLeonLevelFile(path.string(), doc) && AsciiToLower(doc.name) == needle) {
                        match = true;
                    }
                }
                if (!match) {
                    continue;
                }
                const std::string resolved = path.lexically_normal().string();
                if (!LoadLevelFile(e, resolved)) {
                    return false;
                }
                ctx_.levelPath = resolved;
                return true;
            }
            return false;
        });
        pieMode_->OnEnter(engine, ctx_.levelPath);
    } else if (packInfo.known) {
        const std::string levelKey = CurrentEditorLevelKey(ctx_);
        if (!pieSession_.Start(engine, ctx_.projectName.c_str(), packInfo.registerModes, levelKey,
                               ctx_.projectPath)) {
            EditorToast("PIE: failed to start pack Runtime session", EEditorToastKind::Error, 5.0f);
            pieSession_.Stop();
            pieUsesHostSession_ = false;
            pieRestoreLevelPath_.clear();
            ctx_.piePaintPlayOverlay = {};
            ctx_.pieNewWindow = false;
            ctx_.editorViewCamera = nullptr;
            engine.SetPlayInputWindow(nullptr);
            engine.SetPlayMouseLookActive(false);
            engine.SetCursorCaptured(false);
            DestroyPiePresentResources();
            pieTarget_.Destroy();
            pieWindow_.Destroy();
            RestoreEditorCamera(engine.GetCamera());
            if (!ctx_.projectPath.empty()) {
                SetActiveContentRoot(ctx_.projectPath);
            }
            return;
        }
        pieUsesHostSession_ = true;
        if (GameMode* active = pieSession_.Router().GetActive()) {
            ctx_.world = &active->GetWorld();
        } else {
            ctx_.world = &world_;
        }
        ctx_.piePaintPlayOverlay = [this](int fbW, int fbH) {
            if (ctx_.engine == nullptr) {
                return;
            }
            ctx_.engine->PaintHudAndOverlay(fbW, fbH);
            pieSession_.DrawUi(fbW, fbH);
        };
        EditorLogInfo("PIE: pack Runtime session '" + ctx_.projectName + "' @ " +
                      (levelKey.empty() ? std::string("(default)") : levelKey));
    } else if (gm == "third-person" || gm == "Pie") {
        pieMode_ = std::make_unique<PieGameMode>();
        ctx_.world = &pieMode_->GetWorld();
        engine.GetGameInstance().SetLevelTravelFn([this](Engine& e, std::string_view levelKey) -> bool {
            fs::path levelsDir;
            if (!ctx_.projectPath.empty()) {
                levelsDir = ProjectContentDirectory(ctx_.projectPath) / "Levels";
            } else if (!ctx_.levelPath.empty()) {
                levelsDir = fs::path(ctx_.levelPath).parent_path();
            }
            if (levelsDir.empty()) {
                return false;
            }
            const std::string needle = AsciiToLower(levelKey);
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(levelsDir, ec)) {
                if (ec || !entry.is_regular_file()) {
                    continue;
                }
                const fs::path path = entry.path();
                if (AsciiToLower(path.extension().string()) != ".llev") {
                    continue;
                }
                bool match = AsciiToLower(path.stem().string()) == needle;
                if (!match) {
                    LevelDocument doc;
                    if (LoadLeonLevelFile(path.string(), doc) && AsciiToLower(doc.name) == needle) {
                        match = true;
                    }
                }
                if (!match) {
                    continue;
                }
                const std::string resolved = path.lexically_normal().string();
                if (!LoadLevelFile(e, resolved)) {
                    return false;
                }
                ctx_.levelPath = resolved;
                return true;
            }
            return false;
        });
        pieMode_->OnEnter(engine, ctx_.levelPath);
    } else {
        std::cerr << "Editor PIE: GameMode '" << gm
                  << "' has no linked pack gameplay — using third-person Pie preview.\n";
        EditorToast("PIE: '" + gm + "' → third-person preview (pack gameplay not linked)",
                    EEditorToastKind::Warning, 6.0f);
        pieMode_ = std::make_unique<PieGameMode>();
        ctx_.world = &pieMode_->GetWorld();
        pieMode_->OnEnter(engine, ctx_.levelPath);
    }

    ApplyPieNetMode(engine);
    // Single-instance PIE only (multi Shipping already returned above).
    ctx_.piePlaying = true;
    ctx_.ClearSelection();

    if (ctx_.pieNewWindow) {
        // Capture look on the play window only; keep the editor cursor free.
        engine.SetCursorCaptured(true);
        engine.SetPlayMouseLookActive(true);
        pieWindow_.Show();
        pieWindow_.Focus();
        engine.GetWindow().SetCursorCaptured(false);
        engine.GetWindow().MakeContextCurrent();
    } else {
        // Selected Viewport: hide + lock cursor (GLFW_CURSOR_DISABLED) so look isn't
        // clamped by screen edges. Esc / Pause restores the OS cursor.
        engine.SetCursorCaptured(true);
        engine.SetPlayMouseLookActive(true);
    }
}

void EditorApp::ApplyPieNetMode(Engine& engine) {
    // Flow: Unreal Play Net Mode
    // 1. Clamp player count
    // 2. Standalone → CloseNetSession
    // 3. Listen Server + 1 player → editor HostListen (preview bind)
    // 4. Listen Server + N>1 → editor stays offline (Shipping owns the listen host)
    // 5. Client → Join(pieClientAddress)
    ctx_.pieNumberOfPlayers = std::clamp(ctx_.pieNumberOfPlayers, 1, leon::net::kMaxPlayers);
    GameInstance& gi = engine.GetGameInstance();
    gi.CloseNetSession();
    switch (ctx_.pieNetMode) {
    case EEditorPlayNetMode::ListenServer:
        if (ctx_.pieNumberOfPlayers > 1) {
            // Port 7777 is used by the Shipping --listen host spawned below.
            EditorLogInfo("PIE preview local — Shipping Listen Server will host multiplayer");
            EditorToast("PIE preview local — Shipping Listen Server hosts multiplayer",
                        EEditorToastKind::Info, 4.0f);
            break;
        }
        if (gi.HostListen(static_cast<std::uint16_t>(leon::net::kDefaultPort))) {
            EditorToast("PIE Listen Server on port " + std::to_string(leon::net::kDefaultPort),
                        EEditorToastKind::Success, 3.0f);
            engine.AddOnScreenDebugMessage(
                "PIE Listen Server :" + std::to_string(leon::net::kDefaultPort), 5.0f,
                {0.45f, 0.9f, 0.55f});
        } else {
            EditorToast("PIE: failed to start Listen Server", EEditorToastKind::Error, 4.0f);
        }
        break;
    case EEditorPlayNetMode::Client: {
        const std::string addr =
            ctx_.pieClientAddress.empty() ? "127.0.0.1" : ctx_.pieClientAddress;
        if (gi.Join(addr, static_cast<std::uint16_t>(leon::net::kDefaultPort))) {
            EditorToast("PIE Client connecting to " + addr, EEditorToastKind::Info, 3.0f);
            engine.AddOnScreenDebugMessage("PIE Client → " + addr + ":" +
                                               std::to_string(leon::net::kDefaultPort),
                                           5.0f, {0.55f, 0.8f, 1.0f});
        } else {
            EditorToast("PIE: Join failed (" + addr + ")", EEditorToastKind::Error, 4.0f);
        }
        break;
    }
    case EEditorPlayNetMode::Standalone:
    default:
        break;
    }
}

void EditorApp::SpawnPieExtraInstances() {
    // Flow: Number of Players (Unreal multi-PIE via Shipping pack processes)
    // Listen Server + N: 1x --listen host now; (N-1)x --join after port bind (async tick)
    // Client + N: N-1 extra --join immediately
    pieSpawnedPids_.clear();
    piePendingClientSpawn_ = false;
    piePendingHostPortSeen_ = false;
    piePendingClientCount_ = 0;
    piePendingClientExe_.clear();
    piePendingJoinAddr_.clear();
    piePendingPlayMap_.clear();
    piePendingClientWaitSeconds_ = 0.0f;
    piePendingHostSettleSeconds_ = 0.0f;

    const std::string playMap = CurrentPiePlayMapKey(ctx_);
    EditorLogInfo("PIE Play settings: net=" +
                  std::string(ctx_.pieNetMode == EEditorPlayNetMode::ListenServer ? "ListenServer"
                              : ctx_.pieNetMode == EEditorPlayNetMode::Client     ? "Client"
                                                                                  : "Standalone") +
                  " players=" + std::to_string(ctx_.pieNumberOfPlayers) + " map=" + playMap);

    if (ctx_.pieNetMode == EEditorPlayNetMode::Standalone) {
        if (ctx_.pieNumberOfPlayers > 1) {
            EditorLogWarn("Standalone multi-instance: use Listen Server, or run Shipping copies");
            EditorToast("Standalone multi-instance: use Listen Server, or run Shipping copies "
                        "manually",
                        EEditorToastKind::Warning, 5.0f);
        }
        return;
    }

#ifndef _WIN32
    if (ctx_.pieNumberOfPlayers > 1) {
        EditorToast("Multi-player PIE spawn is Windows-only for now", EEditorToastKind::Warning,
                    4.0f);
    }
    return;
#else
    const int want = ctx_.pieNumberOfPlayers;
    if (want <= 1 && ctx_.pieNetMode != EEditorPlayNetMode::ListenServer) {
        return;
    }
    // Listen Server with 1 player: editor hosts only (no Shipping).
    if (ctx_.pieNetMode == EEditorPlayNetMode::ListenServer && want <= 1) {
        EditorLogInfo("PIE Listen Server x1: editor NetDriver only (no Shipping processes)");
        return;
    }

    const fs::path exe = FindPackShippingExecutable(ctx_.projectPath, ctx_.projectName);
    if (exe.empty()) {
        EditorLogError("Multiplayer PIE: Shipping exe not found under " + ctx_.projectPath +
                       "/Shipping (Build Game)");
        EditorToast("Multiplayer PIE needs a built Shipping game (Build Game)",
                    EEditorToastKind::Warning, 6.0f);
        return;
    }
    EditorLogInfo("PIE Shipping exe: " + exe.lexically_normal().string());

    const std::string joinAddr =
        ctx_.pieClientAddress.empty() ? "127.0.0.1" : ctx_.pieClientAddress;

    if (ctx_.pieNetMode == EEditorPlayNetMode::ListenServer) {
        // Unreal PIE: host opens the current map GameMode; clients join that map.
        const std::string hostArgs = "--listen --map " + playMap;
        std::uint32_t hostPid = 0;
        if (!SpawnDetachedProcess(exe, hostArgs, hostPid)) {
            EditorLogError("Failed to CreateProcess Shipping " + hostArgs);
            EditorToast("Failed to launch Shipping Listen Server", EEditorToastKind::Error, 4.5f);
            return;
        }
        pieSpawnedPids_.push_back(hostPid);
        EditorLogInfo("Launched Shipping Listen Server pid=" + std::to_string(hostPid) + " " +
                      hostArgs + " — waiting for UDP :" +
                      std::to_string(leon::net::kDefaultPort) + " before clients");
        EditorToast("Hosting '" + playMap + "' — clients join when ready…", EEditorToastKind::Info,
                    4.0f);

        piePendingClientSpawn_ = true;
        piePendingHostPortSeen_ = false;
        piePendingClientCount_ = want - 1;
        piePendingClientExe_ = exe.lexically_normal().string();
        piePendingJoinAddr_ = joinAddr;
        piePendingPlayMap_ = playMap;
        piePendingClientWaitSeconds_ = 0.0f;
        piePendingHostSettleSeconds_ = 0.0f;
        return;
    }

    // Client mode: Number of Players = total Shipping clients joining the address/map.
    const std::string joinArgs = "--join " + joinAddr + " --map " + playMap;
    int launched = 0;
    for (int i = 0; i < want; ++i) {
        std::uint32_t pid = 0;
        if (SpawnDetachedProcess(exe, joinArgs, pid)) {
            pieSpawnedPids_.push_back(pid);
            ++launched;
            EditorLogInfo("Launched Shipping client pid=" + std::to_string(pid) + " " + joinArgs);
        }
    }
    EditorToast("Launched " + std::to_string(launched) + " Shipping client(s) → '" + playMap + "'",
                launched > 0 ? EEditorToastKind::Success : EEditorToastKind::Error, 3.5f);
#endif
}

void EditorApp::TickPendingPieClientSpawns() {
#ifndef _WIN32
    return;
#else
    if (!piePendingClientSpawn_ || !ctx_.piePlaying) {
        return;
    }
    piePendingClientWaitSeconds_ += std::max(ctx_.deltaTime, 1.0f / 120.0f);
    constexpr float kHostWaitTimeout = 25.0f;
    // Host needs time to ServerTravel into the match map after binding.
    constexpr float kSettleAfterBind = 1.75f;
    const auto port = static_cast<std::uint16_t>(leon::net::kDefaultPort);

    if (!piePendingHostPortSeen_) {
        if (!IsUdpPortInUse(port)) {
            if (piePendingClientWaitSeconds_ >= kHostWaitTimeout) {
                EditorLogError("Shipping host did not bind :" + std::to_string(port) + " within " +
                               std::to_string(static_cast<int>(kHostWaitTimeout)) + "s");
                EditorToast("Shipping host did not bind :" + std::to_string(port) + " in time",
                            EEditorToastKind::Error, 6.0f);
                piePendingClientSpawn_ = false;
            }
            return;
        }
        piePendingHostPortSeen_ = true;
        piePendingHostSettleSeconds_ = 0.0f;
        EditorLogInfo("Shipping host bound UDP :" + std::to_string(port) +
                      " — spawning clients shortly");
        return;
    }

    piePendingHostSettleSeconds_ += std::max(ctx_.deltaTime, 1.0f / 120.0f);
    if (piePendingHostSettleSeconds_ < kSettleAfterBind) {
        return;
    }

    std::string joinArgs = "--join " + piePendingJoinAddr_;
    if (!piePendingPlayMap_.empty()) {
        joinArgs += " --map " + piePendingPlayMap_;
    }
    int launched = 0;
    for (int i = 0; i < piePendingClientCount_; ++i) {
        std::uint32_t pid = 0;
        if (SpawnDetachedProcess(fs::path(piePendingClientExe_), joinArgs, pid)) {
            pieSpawnedPids_.push_back(pid);
            ++launched;
            EditorLogInfo("Launched Shipping client pid=" + std::to_string(pid) + " " + joinArgs);
        } else {
            EditorLogError("Failed to CreateProcess Shipping client " + joinArgs);
        }
    }
    EditorLogInfo("PIE multiplayer: Listen Server + " + std::to_string(launched) +
                  " client(s) → '" + piePendingPlayMap_ + "'");
    EditorToast("Launched host + " + std::to_string(launched) + " client(s) → '" +
                    piePendingPlayMap_ + "'",
                launched > 0 ? EEditorToastKind::Success : EEditorToastKind::Error, 4.5f);
    piePendingClientSpawn_ = false;
    piePendingClientCount_ = 0;
#endif
}

void EditorApp::TerminatePieSpawnedProcesses() {
#ifdef _WIN32
    for (std::uint32_t pid : pieSpawnedPids_) {
        if (pid == 0) {
            continue;
        }
        HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (proc != nullptr) {
            (void)TerminateProcess(proc, 0);
            CloseHandle(proc);
        }
    }
#endif
    pieSpawnedPids_.clear();
    piePendingClientSpawn_ = false;
    piePendingHostPortSeen_ = false;
    piePendingClientCount_ = 0;
    piePendingClientExe_.clear();
    piePendingJoinAddr_.clear();
    piePendingPlayMap_.clear();
    piePendingClientWaitSeconds_ = 0.0f;
    piePendingHostSettleSeconds_ = 0.0f;
}

void EditorApp::StopPie(Engine& engine) {
    TerminatePieSpawnedProcesses();
    engine.GetGameInstance().CloseNetSession();
    const bool shippingOnly = pieShippingSessionOnly_;
    pieShippingSessionOnly_ = false;

    const bool hadLocalMode = pieMode_ != nullptr;
    const bool hadSession = pieUsesHostSession_;
    if (!ctx_.piePlaying || (!hadLocalMode && !hadSession)) {
        ctx_.piePlaying = false;
        ctx_.piePaused = false;
        ctx_.requestPieStop = false;
        ctx_.requestPiePauseToggle = false;
        ctx_.pieNewWindow = false;
        ctx_.editorViewCamera = nullptr;
        ctx_.piePaintPlayOverlay = {};
        ctx_.world = &world_;
        pieUsesHostSession_ = false;
        pieRestoreLevelPath_.clear();
        engine.SetPlayInputWindow(nullptr);
        engine.SetPlayMouseLookActive(false);
        engine.SetCursorCaptured(false);
        DestroyPiePresentResources();
        pieTarget_.Destroy();
        pieWindow_.Destroy();
        if (shippingOnly) {
            EditorLogInfo("PIE multiplayer session stopped (Shipping processes terminated)");
        }
        return;
    }

    if (hadSession) {
        pieSession_.Stop();
        pieUsesHostSession_ = false;
    }
    if (hadLocalMode) {
        pieMode_->OnExit(engine);
        pieMode_.reset();
        engine.GetGameInstance().SetLevelTravelFn({});
    }

    ctx_.piePaintPlayOverlay = {};
    ctx_.world = &world_;
    world_.Clear();
    ctx_.piePlaying = false;
    ctx_.piePaused = false;
    ctx_.requestPieStop = false;
    ctx_.requestPiePauseToggle = false;
    ctx_.pieNewWindow = false;
    ctx_.editorViewCamera = nullptr;
    engine.SetPlayInputWindow(nullptr);
    engine.SetPlayMouseLookActive(false);
    DestroyPiePresentResources();
    pieTarget_.Destroy();
    pieWindow_.Destroy();
    engine.GetWindow().SetCursorCaptured(false);
    engine.GetWindow().MakeContextCurrent();

    // Restore edit-time content root + level (travel may have replaced engine.GetLevel()).
    if (!ctx_.projectPath.empty()) {
        SetActiveContentRoot(ctx_.projectPath);
    } else {
        SetActiveContentRoot({});
    }
    const std::string restorePath =
        !pieRestoreLevelPath_.empty() ? pieRestoreLevelPath_ : ctx_.levelPath;
    pieRestoreLevelPath_.clear();
    if (!restorePath.empty() && LoadLevelFile(engine, restorePath)) {
        ctx_.levelPath = restorePath;
        ReloadLevelLightmapsForPath(engine.GetLevel(), restorePath);
        ctx_.level = &engine.GetLevel();
    }

    RestoreEditorCamera(engine.GetCamera());
    // Prefer FreeLook for editor navigation after PIE.
    if (engine.GetCamera().Mode() != ECameraMode::FreeLook) {
        const glm::vec3 eye = engine.GetCamera().GetCameraLocation();
        engine.GetCamera().SetMode(ECameraMode::FreeLook);
        engine.GetCamera().SetEyeLocation(eye);
    }
}

void EditorApp::DestroyPiePresentResources() {
    if (piePresentFbo_ == 0 && piePresentAttachedTex_ == 0) {
        return;
    }
    if (pieWindow_.NativeHandle() != nullptr) {
        pieWindow_.MakeContextCurrent();
        if (piePresentFbo_ != 0) {
            glDeleteFramebuffers(1, &piePresentFbo_);
            piePresentFbo_ = 0;
        }
        piePresentAttachedTex_ = 0;
        if (ctx_.window != nullptr) {
            ctx_.window->MakeContextCurrent();
        }
    } else {
        piePresentFbo_ = 0;
        piePresentAttachedTex_ = 0;
    }
}

void EditorApp::PresentPieColorTexture(unsigned int colorTexture, int srcW, int srcH, int dstW,
                                       int dstH) {
    if (colorTexture == 0 || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) {
        return;
    }

    pieWindow_.MakeContextCurrent();
    if (piePresentFbo_ == 0) {
        glGenFramebuffers(1, &piePresentFbo_);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, piePresentFbo_);
    if (piePresentAttachedTex_ != colorTexture) {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               colorTexture, 0);
        piePresentAttachedTex_ = colorTexture;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, srcW, srcH, 0, 0, dstW, dstH, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    pieWindow_.SwapBuffers();
    if (ctx_.window != nullptr) {
        ctx_.window->MakeContextCurrent();
    }
}

void EditorApp::RenderPieWindow(Engine& engine) {
    if (!ctx_.piePlaying || !ctx_.pieNewWindow || pieWindow_.NativeHandle() == nullptr) {
        return;
    }
    if (pieWindow_.ShouldClose()) {
        ctx_.requestPieStop = true;
        return;
    }

    int fbW = 0;
    int fbH = 0;
    pieWindow_.GetFramebufferSize(fbW, fbH);
    if (fbW <= 0 || fbH <= 0) {
        return;
    }

    // Play window owns look capture; keep editor chrome unlocked.
    if (!ctx_.piePaused && pieWindow_.IsFocused() && !pieWindow_.IsCursorCaptured()) {
        pieWindow_.SetCursorCaptured(true);
    }
    engine.GetWindow().SetCursorCaptured(false);

    // Flow: New Window PIE present
    // 1. Render the scene on the editor GL context into a shared color texture (FBOs/VAOs
    //    are not shareable — drawing on the play context left a black window).
    // 2. On the play context, blit that texture into the default framebuffer and swap.
    engine.GetWindow().MakeContextCurrent();
    if (!pieTarget_.EnsureSize(fbW, fbH)) {
        return;
    }
    engine.GetCamera().SetPerspective(engine.GetCamera().FieldOfView(), pieWindow_.Aspect(), 0.1f,
                                      100.0f);
    engine.GetRenderer().SetDrawFramebuffer(pieTarget_.fbo());
    engine.GetRenderer().BeginFrame(fbW, fbH);
    // Skeletal draws were queued by gameplay Tick immediately before this call.
    engine.GetRenderer().DrawScene(engine.GetLevel(), engine.GetCamera());
    if (ctx_.piePaintPlayOverlay) {
        ctx_.piePaintPlayOverlay(fbW, fbH);
    }
    engine.GetRenderer().SetDrawFramebuffer(0);
    PresentPieColorTexture(pieTarget_.colorTexture(), pieTarget_.width(), pieTarget_.height(), fbW,
                           fbH);
}

void EditorApp::Run(Engine& engine) {
    if (!imguiReady_) {
        std::cerr << "EditorApp not initialized\n";
        return;
    }

    // Default editor navigation to FreeLook (RMB + WASD).
    {
        Camera& cam = engine.GetCamera();
        const glm::vec3 eye = cam.GetCameraLocation();
        cam.SetMode(ECameraMode::FreeLook);
        cam.SetEyeLocation(eye);
    }

    auto previous = std::chrono::steady_clock::now();
    while (engine.IsRunning() && !engine.GetWindow().ShouldClose()) {
        const auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - previous).count();
        previous = now;
        if (deltaTime > 0.1f) {
            deltaTime = 0.1f;
        }

        engine.GetWindow().PollEvents();
        if (ctx_.piePlaying && ctx_.pieNewWindow && pieWindow_.NativeHandle() != nullptr) {
            engine.GetInput().Update(pieWindow_);
        } else {
            engine.GetInput().Update(engine.GetWindow());
        }
        (void)engine.GetRenderer().ReloadShaders(false);

        ctx_.deltaTime = deltaTime;

        if (projectOpen_) {
            if (ctx_.requestCloseProject) {
                CloseProject(engine);
            }
            if (ctx_.requestPieStart && !ctx_.piePlaying) {
                StartPie(engine);
            }
            if (ctx_.piePlaying) {
                TickPendingPieClientSpawns();
            }
            if (ctx_.requestPiePauseToggle && ctx_.piePlaying) {
                ctx_.piePaused = !ctx_.piePaused;
                ctx_.requestPiePauseToggle = false;
                if (ctx_.piePaused) {
                    engine.SetCursorCaptured(false);
                    engine.SetPlayMouseLookActive(false);
                } else if (ctx_.pieNewWindow) {
                    engine.SetCursorCaptured(true);
                    engine.SetPlayMouseLookActive(true);
                    pieWindow_.Focus();
                } else {
                    engine.SetCursorCaptured(true);
                    engine.SetPlayMouseLookActive(true);
                }
            }
            if (ctx_.requestPieStop && ctx_.piePlaying) {
                StopPie(engine);
            }
            if (ctx_.requestBuildLights && ctx_.level != nullptr && !ctx_.piePlaying) {
                ctx_.requestBuildLights = false;
                (void)BakeLevelLightmaps(*ctx_.level, &ctx_.buildLightsStatus, ctx_.levelPath);
                ctx_.MarkDirty();
                std::cout << ctx_.buildLightsStatus << '\n';
            } else if (ctx_.requestBuildLights) {
                ctx_.requestBuildLights = false;
            }
            {
                const bool esc =
                    ctx_.piePlaying && ctx_.pieNewWindow && pieWindow_.NativeHandle() != nullptr
                        ? pieWindow_.IsKeyPressed(leon::EKey::Escape)
                        : engine.GetWindow().IsKeyPressed(leon::EKey::Escape);
                if (ctx_.piePlaying && esc && !escapeWasDown_) {
                    StopPie(engine);
                }
                escapeWasDown_ = esc;
            }

            if (ctx_.piePlaying && (pieMode_ != nullptr || pieUsesHostSession_)) {
                if (ctx_.piePaused) {
                    engine.SetPlayMouseLookActive(false);
                    if (engine.IsCursorCaptured()) {
                        engine.SetCursorCaptured(false);
                    }
                } else if (ctx_.pieNewWindow) {
                    const bool focused = pieWindow_.NativeHandle() != nullptr && pieWindow_.IsFocused();
                    engine.SetPlayMouseLookActive(focused);
                    if (focused && !engine.IsCursorCaptured()) {
                        engine.SetCursorCaptured(true);
                    }
                } else {
                    // Keep look + hidden cursor for the whole Selected Viewport session.
                    // (Hover-only look left the OS cursor visible and edge-clamped.)
                    engine.SetPlayMouseLookActive(true);
                    if (!engine.IsCursorCaptured()) {
                        engine.SetCursorCaptured(true);
                    }
                }
                // Selected Viewport: tick before ImGui so the Viewport DrawScene sees skinned
                // draws. New Window: tick after ImGui (below) — editor panels also call DrawScene
                // and would clear the skeletal queue before RenderPieWindow.
                if (!ctx_.piePaused && !ctx_.pieNewWindow) {
                    if (pieUsesHostSession_) {
                        pieSession_.HandleUiInput();
                        engine.TickPlayAudio();
                        pieSession_.Tick(deltaTime);
                        engine.TickPlayHud(deltaTime);
                        if (GameMode* active = pieSession_.Router().GetActive()) {
                            ctx_.world = &active->GetWorld();
                        }
                    } else if (pieMode_ != nullptr) {
                        engine.TickPlayAudio();
                        pieMode_->Tick(engine, deltaTime);
                        engine.TickPlayHud(deltaTime);
                    }
                    if (ctx_.pieNetMode != EEditorPlayNetMode::Standalone) {
                        engine.GetGameInstance().GetNetDriver().Poll();
                    }
                }
            }
        }

        BeginImGuiFrame();
        if (!projectOpen_) {
            welcome_.Draw(projects_);
            if (welcome_.HasPendingOpen()) {
                const std::string path = welcome_.PendingOpenPath();
                welcome_.ClearPendingOpen();
                (void)OpenProject(engine, path);
            }
        } else {
            layout_.Draw(ctx_);
        }
        EndImGuiFrame();

        if (projectOpen_ && ctx_.piePlaying && (pieMode_ != nullptr || pieUsesHostSession_) &&
            !ctx_.piePaused && ctx_.pieNewWindow) {
            if (pieUsesHostSession_) {
                pieSession_.HandleUiInput();
                engine.TickPlayAudio();
                pieSession_.Tick(deltaTime);
                engine.TickPlayHud(deltaTime);
                if (GameMode* active = pieSession_.Router().GetActive()) {
                    ctx_.world = &active->GetWorld();
                }
            } else if (pieMode_ != nullptr) {
                engine.TickPlayAudio();
                pieMode_->Tick(engine, deltaTime);
                engine.TickPlayHud(deltaTime);
            }
            if (ctx_.pieNetMode != EEditorPlayNetMode::Standalone) {
                engine.GetGameInstance().GetNetDriver().Poll();
            }
        }
        RenderPieWindow(engine);

        UpdateWindowTitle(engine);
        engine.GetWindow().SwapBuffers();
    }

    if (ctx_.piePlaying) {
        StopPie(engine);
    }
}

} // namespace leon::editor
