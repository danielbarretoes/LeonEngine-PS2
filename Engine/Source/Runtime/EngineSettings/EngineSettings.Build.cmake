# EngineSettings: the project's map, game mode and general settings as config classes (Unreal: Runtime/EngineSettings).
# UGameMapsSettings reads [/Script/EngineSettings.GameMapsSettings] of the Engine config, UGeneralProjectSettings
# [/Script/EngineSettings.GeneralProjectSettings] of the Game config.
leon_module(EngineSettings
	PUBLIC_DEPENDENCIES Core CoreUObject
)
