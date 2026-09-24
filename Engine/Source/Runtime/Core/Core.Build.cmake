# Core: HAL, platform abstraction, module manager, paths, math (Unreal: Runtime/Core).
leon_module(Core
	PUBLIC_DEPENDENCIES_Desktop GLM
	PUBLIC_SYSTEM_LIBRARIES_Windows psapi
	# std::filesystem / glm helpers are desktop-only for now.
	EXCLUDE_SOURCES_PS2 Private/Misc/FileHelper.cpp Private/Misc/Paths.cpp Private/Math/Transform.cpp
)
