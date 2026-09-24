#include "GameApplication.h"

#include "GameHostSession.h"
#include "RuntimeInput.h"

#include <algorithm>
#include <iostream>
#include <thread>
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

bool FGameApplication::Init(int Argc, char** Argv, const char* PackName, const FRegisterModesFunction& RegisterModes,
                            bool bDedicatedByDefault) {
    // bDedicatedByDefault is set by server executables; CLI flags work on the client exe too.
    // Console strings stay ASCII: Windows cmd often is not UTF-8 (em dash / arrows mojibake).
    bDedicated = bDedicatedByDefault || WantsDedicatedCli(Argc, Argv);
    const bool bListenHost = !bDedicated && (HasFlag(Argc, Argv, "--listen") || HasFlag(Argc, Argv, "--host"));
    const bool bShowStats = HasFlag(Argc, Argv, "--show-stats");
    const std::uint16_t NetPort = ParsePort(Argc, Argv, static_cast<std::uint16_t>(Leon::Net::DefaultPort));
    TickHz = ParseTickHz(Argc, Argv, 60.0f);
    const std::string JoinAddress = ParseJoinAddress(Argc, Argv);
    const std::string PlayMap = ParsePlayMap(Argc, Argv);
    const std::string Title =
        bDedicated ? std::string("Leon (Dedicated) - ") + PackName : std::string("Leon - ") + PackName;

    Engine = std::make_unique<UGameEngine>();
    if (bDedicated) {
        if (!Engine->InitializeHeadless()) {
            std::cerr << "Failed to initialize headless engine\n";
            Engine.reset();
            return false;
        }
    } else if (!Engine->Initialize(1280, 720, Title.c_str())) {
        std::cerr << "Failed to initialize engine\n";
        Engine.reset();
        return false;
    }
    if (bShowStats && !bDedicated) {
        Engine->SetHudStatsVisible(true);
    }
    if (!bDedicated) {
        WireDefaultInput(*Engine);
    }

    // Shipping: empty preferred key -> pack defaultLevel inside Session.Start.
    if (!Session.Start(*Engine, PackName, RegisterModes, {})) {
        Engine->Shutdown();
        Engine.reset();
        return false;
    }
    bStarted = true;

    if (bDedicated) {
        Engine->GetGameInstance().RequestDedicatedStart(NetPort);
        std::cout << "Starting dedicated server for pack '" << PackName << "' on port " << NetPort << " @ " << TickHz
                  << " Hz (headless - Ctrl+C or RequestQuit to stop)\n";
        NextHeadlessTick = std::chrono::steady_clock::now();
    } else {
        // Flow: CLI Play session (Unreal-like)
        // --listen/--host [--map Key] -> HostListen -> Lobby or match map
        // --join <ip> [--map Key] -> Join -> Lobby or match map
        if (!PlayMap.empty()) {
            Engine->GetGameInstance().SetPendingPlayMap(PlayMap);
            std::cout << "Pending play map: " << PlayMap << '\n';
        }
        if (bListenHost) {
            Engine->GetGameInstance().RequestListenStart(NetPort);
            std::cout << "Pending listen host on port " << NetPort << '\n';
        } else if (!JoinAddress.empty()) {
            Engine->GetGameInstance().SetPendingJoinAddress(JoinAddress);
            std::cout << "Pending join address: " << JoinAddress << '\n';
        }
        Engine->Start();
    }
    LastFrameTime = std::chrono::steady_clock::now();
    return true;
}

bool FGameApplication::Tick() {
    if (!Engine) {
        return false;
    }
    if (bDedicated) {
        // Fixed-timestep simulation (no render / present), paced to TickHz.
        const float StepSeconds = 1.0f / (TickHz < 1.0f ? 1.0f : TickHz);
        if (!Engine->IsRunning()) {
            return false;
        }
        Session.Tick(StepSeconds);
        using FClock = std::chrono::steady_clock;
        NextHeadlessTick += std::chrono::duration_cast<FClock::duration>(std::chrono::duration<double>(StepSeconds));
        const auto Now = FClock::now();
        if (NextHeadlessTick < Now) {
            NextHeadlessTick = Now; // fell behind: resync instead of spiralling
        } else {
            std::this_thread::sleep_until(NextHeadlessTick);
        }
        return Engine->IsRunning();
    }

    const auto Now = std::chrono::steady_clock::now();
    const float DeltaTime = std::min(std::chrono::duration<float>(Now - LastFrameTime).count(), 0.1f);
    LastFrameTime = Now;
    return Engine->Tick(DeltaTime, [this](float Dt) { Session.Tick(Dt); }, [this]() { Session.HandleUiInput(); },
                        [this](int Width, int Height) { Session.DrawUi(Width, Height); });
}

void FGameApplication::Exit() {
    if (bStarted) {
        Session.Stop();
        bStarted = false;
    }
    if (Engine) {
        Engine->Shutdown();
        Engine.reset();
    }
}

int FGameApplication::Run(int Argc, char** Argv, const char* PackName, const FRegisterModesFunction& RegisterModes,
                          bool bDedicatedByDefault) {
    if (!Init(Argc, Argv, PackName, RegisterModes, bDedicatedByDefault)) {
        return 1;
    }
    while (Tick()) {
    }
    Exit();
    return 0;
}
