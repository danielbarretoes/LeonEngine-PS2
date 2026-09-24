#pragma once



#include "GameFramework/GameplayRouter.h"

#include "Level/LevelDirector.h"

#include <string>

#include <string_view>




class Engine;







/// Level load orchestration + world tick glue (no ownership of Actor/GameMode types).

class WorldRuntime {

public:

    [[nodiscard]] bool Initialize(Engine& engine, const std::string& shaderDirectory);

    [[nodiscard]] bool LoadPack(Engine& engine, const std::string& packDirectory,

                                std::string_view preferredLevelKey = {});

    void Tick(Engine& engine, GameplayRouter& gameplay, float deltaTime);

    void HandleUiInput(Engine& engine);

    void DrawUi(int framebufferWidth, int framebufferHeight);

    void Shutdown();



    [[nodiscard]] LevelDirector& Director() { return director_; }

    [[nodiscard]] const LevelDirector& Director() const { return director_; }



private:

    LevelDirector director_;

};





