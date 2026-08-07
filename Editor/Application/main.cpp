/// Leon Editor host — Unreal-like Level editor (Dear ImGui). Separate from `leon-engine` gameplay.
#include <exception>
#include <iostream>
#include <leon/editor/EditorApp.h>
#include <leon/Engine.h>

// NOLINTNEXTLINE(bugprone-exception-escape) — Engine/Editor may throw on I/O; process exit is fine.
int main() {
    try {
        leon::Engine engine;
        if (!engine.Initialize(1600, 900, "Leon Editor")) {
            std::cerr << "Failed to initialize engine for editor\n";
            return 1;
        }

        leon::editor::EditorApp editor;
        if (!editor.Initialize(engine)) {
            std::cerr << "Failed to initialize Leon Editor\n";
            engine.Shutdown();
            return 1;
        }

        editor.Run(engine);
        editor.Shutdown();
        engine.Shutdown();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << '\n';
        return 1;
    }
}
