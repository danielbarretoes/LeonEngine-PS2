# InputCore: the input keys, FKey and EKeys (Unreal: Runtime/InputCore). FKey is a reflected struct (config text
# `Key=SpaceBar`), so InputCore depends on CoreUObject; every platform, the PS2 included.
leon_module(InputCore
	PUBLIC_DEPENDENCIES Core CoreUObject
)
