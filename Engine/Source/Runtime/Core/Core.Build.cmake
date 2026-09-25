# Core: HAL, platform abstraction, module manager, paths, math (Unreal: Runtime/Core).
leon_module(Core
	PUBLIC_DEPENDENCIES_Desktop GLM
	PUBLIC_SYSTEM_LIBRARIES_Windows psapi ole32
	# The glm migration bridge is desktop-only until P6.
	EXCLUDE_SOURCES_PS2 Private/Migration/LegacyTransform.cpp
)
