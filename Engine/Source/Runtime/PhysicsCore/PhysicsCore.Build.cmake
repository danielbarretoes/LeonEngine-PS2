# PhysicsCore: Physics types and backend interface (Unreal: Runtime/PhysicsCore). Reflected since P17: the collision
# channels, responses and their container are UENUMs / a USTRUCT that components save and the config names.
leon_module(PhysicsCore
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core CoreUObject
)
