# JoltPhysics plugin: Jolt rigid-body backend registered with FPhysScene at module startup.
leon_module(JoltPhysics
	PLATFORMS Win64
	PUBLIC_DEPENDENCIES Core PhysicsCore
	PRIVATE_DEPENDENCIES Engine JoltLib
	# Transitional: the Jolt automation tests check this define.
	PUBLIC_DEFINITIONS LEON_WITH_JOLT=1
)
