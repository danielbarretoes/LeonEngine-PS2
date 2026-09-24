#include "GameApplication.h"

#include "GameHostSession.h"
#include "RuntimeInput.h"

#include <iostream>
#include "Engine/GameEngine.h"
#include "Net/NetProtocol.h"
#include <string>

namespace {

[[nodiscard]] bool HasFlag(int Argc, char** Argv, const char* Flag) {
    for (int I = 1; I < Argc; ++I) {
        if (Argv[I] != nullptr && std::string(Argv[I]) == Flag) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool WantsDedicatedCli(int Argc, char** Argv) {
    return HasFlag(Argc, Argv, "--dedicated") || HasFlag(Argc, Argv, "--server");
}

[[nodiscard]] std::uint16_t ParsePort(int Argc, char** Argv, std::uint16_t Fallback) {
    for (int I = 1; I < Argc; ++I) {
        const std::string Arg = Argv[I] != nullptr ? Argv[I] : "";
        if ((Arg == "--port" || Arg == "-p") && I + 1 < Argc && Argv[I + 1] != nullptr) {
            try {
                const int Parsed = std::stoi(Argv[++I]);
                if (Parsed > 0 && Parsed < 65536) {
                    return static_cast<std::uint16_t>(Parsed);
                }
            } catch (...) {
            }
        }
    }
    return Fallback;
}

[[nodiscard]] float ParseTickHz(int Argc, char** Argv, float Fallback) {
    for (int I = 1; I < Argc; ++I) {
        const std::string Arg = Argv[I] != nullptr ? Argv[I] : "";
        if ((Arg == "--tick" || Arg == "-t") && I + 1 < Argc && Argv[I + 1] != nullptr) {
            try {
                const float Hz = std::stof(Argv[++I]);
                if (Hz >= 1.0f && Hz <= 240.0f) {
                    return Hz;
                }
            } catch (...) {
            }
        }
    }
    return Fallback;
}

[[nodiscard]] std::string ParseJoinAddress(int Argc, char** Argv) {
    for (int I = 1; I < Argc; ++I) {
        const std::string Arg = Argv[I] != nullptr ? Argv[I] : "";
        if ((Arg == "--join" || Arg == "-j") && I + 1 < Argc && Argv[I + 1] != nullptr) {
            return Argv[++I];
        }
    }
    return {};
}

[[nodiscard]] std::string ParsePlayMap(int Argc, char** Argv) {
    for (int I = 1; I < Argc; ++I) {
        const std::string Arg = Argv[I] != nullptr ? Argv[I] : "";
        if ((Arg == "--map" || Arg == "-m") && I + 1 < Argc && Argv[I + 1] != nullptr) {
            return Argv[++I];
        }
    }
    return {};
}

} // namespace

int FGameApplication::Run(int Argc, char** Argv, const char* PackName,
                         const std::function<void(UGameEngine&, FGameplayRouter&)>& RegisterModes,
                         bool bDedicatedByDefault) {
    // dedicatedByDefault is set by *-server mains; CLI flags work on the client exe too.
    // Console strings stay ASCII: Windows cmd often is not UTF-8 (em dash / arrows mojibake).
    const bool bDedicated = bDedicatedByDefault || WantsDedicatedCli(Argc, Argv);
    const bool bListenHost =
        !bDedicated && (HasFlag(Argc, Argv, "--listen") || HasFlag(Argc, Argv, "--host"));
    const bool bShowStats = HasFlag(Argc, Argv, "--show-stats");
    const std::uint16_t NetPort =
        ParsePort(Argc, Argv, static_cast<std::uint16_t>(Leon::Net::DefaultPort));
    const float TickHz = ParseTickHz(Argc, Argv, 60.0f);
    const std::string JoinAddress = ParseJoinAddress(Argc, Argv);
    const std::string PlayMap = ParsePlayMap(Argc, Argv);
    const std::string Title =
        bDedicated ? std::string("Leon (Dedicated) - ") + PackName : std::string("Leon - ") + PackName;

    UGameEngine Engine;
    if (bDedicated) {
        if (!Engine.InitializeHeadless()) {
            std::cerr << "Failed to initialize headless engine\n";
            return 1;
        }
    } else if (!Engine.Initialize(1280, 720, Title.c_str())) {
        std::cerr << "Failed to initialize engine\n";
        return 1;
    }
    if (bShowStats && !bDedicated) {
        Engine.SetHudStatsVisible(true);
    }
    if (!bDedicated) {
        WireDefaultInput(Engine);
    }

    FGameHostSession Session;
    // Shipping: empty preferred key → pack defaultLevel inside session.Start.
    if (!Session.Start(Engine, PackName, RegisterModes, {})) {
        Engine.Shutdown();
        return 1;
    }

    if (bDedicated) {
        Engine.GetGameInstance().RequestDedicatedStart(NetPort);
        std::cout << "Starting dedicated server for pack '" << PackName << "' on port " << NetPort
                  << " @ " << TickHz << " Hz (headless - Ctrl+C or RequestQuit to stop)\n";
        Engine.RunHeadless([&](float Dt) { Session.Tick(Dt); }, TickHz);
    } else {
        // Flow: CLI Play session (Unreal-like)
        // --listen/--host [--map Key] → HostListen → Lobby or match map
        // --join <ip> [--map Key] → Join → Lobby or match map
        if (!PlayMap.empty()) {
            Engine.GetGameInstance().SetPendingPlayMap(PlayMap);
            std::cout << "Pending play map: " << PlayMap << '\n';
        }
        if (bListenHost) {
            Engine.GetGameInstance().RequestListenStart(NetPort);
            std::cout << "Pending listen host on port " << NetPort << '\n';
        } else if (!JoinAddress.empty()) {
            Engine.GetGameInstance().SetPendingJoinAddress(JoinAddress);
            std::cout << "Pending join address: " << JoinAddress << '\n';
        }
        Engine.Run([&](float Dt) { Session.Tick(Dt); }, [&]() { Session.HandleUiInput(); },
                   [&](int W, int H) { Session.DrawUi(W, H); });
    }

    Session.Stop();
    Engine.Shutdown();
    return 0;
}

