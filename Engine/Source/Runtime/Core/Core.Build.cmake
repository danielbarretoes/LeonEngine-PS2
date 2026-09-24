# Core: HAL, platform abstraction, module manager, paths, math (Unreal: Runtime/Core).
leon_module(Core
	PUBLIC_DEPENDENCIES_Desktop GLM
	# std::filesystem / glm helpers are desktop-only for now.
	EXCLUDE_SOURCES_PS2 Private/Misc/FileIO.cpp Private/Misc/Paths.cpp Private/Math/Transform.cpp
)
