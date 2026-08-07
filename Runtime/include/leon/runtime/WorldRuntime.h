#pragma once



#include <leon/gameplay/GameplayRouter.h>

#include <leon/level/LevelDirector.h>

#include <string>

#include <string_view>



namespace leon {

class Engine;

}



namespace leon::runtime {



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



} // namespace leon::runtime


