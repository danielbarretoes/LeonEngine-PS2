#pragma once



#include "GameFramework/GameplayRouter.h"

#include "Level/LevelDirector.h"

#include <string>

#include <string_view>




class UGameEngine;







/// Level load orchestration + world tick glue (no ownership of Actor/GameMode types).

class FWorldRuntime {

public:

    [[nodiscard]] bool Initialize(UGameEngine& Engine, const std::string& ShaderDirectory);

    [[nodiscard]] bool LoadPack(UGameEngine& Engine, const std::string& PackDirectory,

                                std::string_view PreferredLevelKey = {});

    void Tick(UGameEngine& Engine, FGameplayRouter& Gameplay, float DeltaTime);

    void HandleUiInput(UGameEngine& Engine);

    void DrawUi(int FramebufferWidth, int FramebufferHeight);

    void Shutdown();



    [[nodiscard]] FLevelDirector& GetDirector() { return Director; }

    [[nodiscard]] const FLevelDirector& GetDirector() const { return Director; }



private:

    FLevelDirector Director;

};





