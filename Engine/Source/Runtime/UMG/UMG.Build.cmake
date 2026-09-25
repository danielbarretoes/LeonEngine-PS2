# UMG: Widgets (Unreal: Runtime/UMG). Widgets paint through Engine's FCanvas.
leon_module(UMG
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core CoreUObject SlateCore
	PRIVATE_DEPENDENCIES ApplicationCore InputCore
	# UE: UMG and Engine reference each other (Engine's AHUD keeps UUserWidgets, UMG paints through Engine's FCanvas).
	# Engine's reflected AHUD needs UMG's types first, so the ordered edge is Engine -> UMG and this one is circular.
	CIRCULAR_DEPENDENCIES Engine
)
