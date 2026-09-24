#pragma once



#include "GameFramework/GameplayRouter.h"

#include "Level/LevelDirector.h"

#include <string>

#include <string_view>




class UGameEngine;







/// Level load orchestration + world tick glue (no ownership of Actor/GameMode types).

class FWorldRuntime {

public:

    [[nodiscard]] bool Initialize(UGameEngine& engine, const std::string& shaderDirectory);

    [[nodiscard]] bool LoadPack(UGameEngine& engine, const std::string& packDirectory,

                                std::string_view preferredLevelKey = {});

    void Tick(UGameEngine& engine, FGameplayRouter& gameplay, float deltaTime);

    void HandleUiInput(UGameEngine& engine);

    void DrawUi(int framebufferWidth, int framebufferHeight);

    void Shutdown();



    [[nodiscard]] FLevelDirector& Director() { return director_; }

    [[nodiscard]] const FLevelDirector& Director() const { return director_; }



private:

    FLevelDirector director_;

};





