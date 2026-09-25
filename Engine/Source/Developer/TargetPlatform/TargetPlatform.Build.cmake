# TargetPlatform: the platforms the cook targets (Unreal: Developer/TargetPlatform, with the per-platform
# <Platform>TargetPlatform modules folded in): ITargetPlatform, ITargetPlatformManagerModule, Win64 and PS2.
leon_module(TargetPlatform
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core
)
