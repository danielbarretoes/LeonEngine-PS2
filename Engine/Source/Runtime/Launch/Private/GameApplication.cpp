#include <leon/runtime/GameApplication.h>

#include <leon/runtime/GameHostSession.h>
#include <leon/runtime/RuntimeInput.h>

#include <iostream>
#include <leon/Engine.h>
#include <leon/net/NetProtocol.h>
#include <string>

namespace leon::runtime {
namespace {

[[nodiscard]] bool HasFlag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::string(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool WantsDedicatedCli(int argc, char** argv) {
    return HasFlag(argc, argv, "--dedicated") || HasFlag(argc, argv, "--server");
}

[[nodiscard]] std::uint16_t ParsePort(int argc, char** argv, std::uint16_t fallback) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] != nullptr ? argv[i] : "";
        if ((arg == "--port" || arg == "-p") && i + 1 < argc && argv[i + 1] != nullptr) {
            try {
                const int parsed = std::stoi(argv[++i]);
                if (parsed > 0 && parsed < 65536) {
                    return static_cast<std::uint16_t>(parsed);
                }
            } catch (...) {
            }
        }
    }
    return fallback;
}

[[nodiscard]] float ParseTickHz(int argc, char** argv, float fallback) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] != nullptr ? argv[i] : "";
        if ((arg == "--tick" || arg == "-t") && i + 1 < argc && argv[i + 1] != nullptr) {
            try {
                const float hz = std::stof(argv[++i]);
                if (hz >= 1.0f && hz <= 240.0f) {
                    return hz;
                }
            } catch (...) {
            }
        }
    }
    return fallback;
}

[[nodiscard]] std::string ParseJoinAddress(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] != nullptr ? argv[i] : "";
        if ((arg == "--join" || arg == "-j") && i + 1 < argc && argv[i + 1] != nullptr) {
            return argv[++i];
        }
    }
    return {};
}

[[nodiscard]] std::string ParsePlayMap(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] != nullptr ? argv[i] : "";
        if ((arg == "--map" || arg == "-m") && i + 1 < argc && argv[i + 1] != nullptr) {
            return argv[++i];
        }
    }
    return {};
}

} // namespace

int GameApplication::Run(int argc, char** argv, const char* packName,
                         const std::function<void(Engine&, GameplayRouter&)>& registerModes,
                         bool dedicatedByDefault) {
    // dedicatedByDefault is set by *-server mains; CLI flags work on the client exe too.
    // Console strings stay ASCII: Windows cmd often is not UTF-8 (em dash / arrows mojibake).
    const bool dedicated = dedicatedByDefault || WantsDedicatedCli(argc, argv);
    const bool listenHost =
        !dedicated && (HasFlag(argc, argv, "--listen") || HasFlag(argc, argv, "--host"));
    const bool showStats = HasFlag(argc, argv, "--show-stats");
    const std::uint16_t netPort =
        ParsePort(argc, argv, static_cast<std::uint16_t>(net::kDefaultPort));
    const float tickHz = ParseTickHz(argc, argv, 60.0f);
    const std::string joinAddress = ParseJoinAddress(argc, argv);
    const std::string playMap = ParsePlayMap(argc, argv);
    const std::string title =
        dedicated ? std::string("Leon (Dedicated) - ") + packName : std::string("Leon - ") + packName;

    Engine engine;
    if (dedicated) {
        if (!engine.InitializeHeadless()) {
            std::cerr << "Failed to initialize headless engine\n";
            return 1;
        }
    } else if (!engine.Initialize(1280, 720, title.c_str())) {
        std::cerr << "Failed to initialize engine\n";
        return 1;
    }
    if (showStats && !dedicated) {
        engine.SetHudStatsVisible(true);
    }
    if (!dedicated) {
        WireDefaultInput(engine);
    }

    GameHostSession session;
    // Shipping: empty preferred key → pack defaultLevel inside session.Start.
    if (!session.Start(engine, packName, registerModes, {})) {
        engine.Shutdown();
        return 1;
    }

    if (dedicated) {
        engine.GetGameInstance().RequestDedicatedStart(netPort);
        std::cout << "Starting dedicated server for pack '" << packName << "' on port " << netPort
                  << " @ " << tickHz << " Hz (headless - Ctrl+C or RequestQuit to stop)\n";
        engine.RunHeadless([&](float dt) { session.Tick(dt); }, tickHz);
    } else {
        // Flow: CLI Play session (Unreal-like)
        // --listen/--host [--map Key] → HostListen → Lobby or match map
        // --join <ip> [--map Key] → Join → Lobby or match map
        if (!playMap.empty()) {
            engine.GetGameInstance().SetPendingPlayMap(playMap);
            std::cout << "Pending play map: " << playMap << '\n';
        }
        if (listenHost) {
            engine.GetGameInstance().RequestListenStart(netPort);
            std::cout << "Pending listen host on port " << netPort << '\n';
        } else if (!joinAddress.empty()) {
            engine.GetGameInstance().SetPendingJoinAddress(joinAddress);
            std::cout << "Pending join address: " << joinAddress << '\n';
        }
        engine.Run([&](float dt) { session.Tick(dt); }, [&]() { session.HandleUiInput(); },
                   [&](int w, int h) { session.DrawUi(w, h); });
    }

    session.Stop();
    engine.Shutdown();
    return 0;
}

} // namespace leon::runtime
