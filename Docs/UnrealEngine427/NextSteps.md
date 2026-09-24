# Next steps toward UE 4.27 parity

Things the UE-layout refactor intentionally left out. Each item names the UE 4.27 location to mirror.

## Core (next plan)

- **Containers:** `TArray`, `TMap`, `TSet`, `TSparseArray` — `Runtime/Core/Public/Containers/`.
- **Strings:** `FString`, `FName`, `FText`, `TCHAR`/`TEXT()` — `Containers/UnrealString.h`, `UObject/NameTypes.h`,
  `Internationalization/Text.h`. On PS2, `TCHAR` should stay `char` (no wide strings on EE).
- **Math:** native `FVector`, `FRotator`, `FQuat`, `FMatrix`, `FTransform` (Z-up, UE units = cm) — `Math/`.
  Replaces glm in shared modules and lets the gameplay framework compile on PS2.
- **Logging:** `UE_LOG`, `DECLARE_LOG_CATEGORY_EXTERN`, `FOutputDevice` — `Logging/LogMacros.h`.
- **Assertions:** `check`, `ensure`, `verify` — `Misc/AssertionMacros.h`.
- **Delegates:** `DECLARE_DELEGATE*`, `TMulticastDelegate` — `Delegates/`.
- **Memory:** `FMemory`, `FMalloc` (PS2: custom allocator over EE RAM) — `HAL/UnrealMemory.h`, `HAL/MallocAnsi.h`.
- **Config:** `FConfigCacheIni`, `GConfig` — `Misc/ConfigCacheIni.h`; load `Engine/Config` + `<Project>/Config`.
- **Command line / parse:** `FCommandLine`, `FParse` — `Misc/CommandLine.h`, `Misc/Parse.h`.

## CoreUObject

- `UObject`, `UClass`, `UStruct`, `UProperty`/`FProperty` — `Runtime/CoreUObject/Public/UObject/`.
- A reflection generator (UnrealHeaderTool homologue) — LeonBuildTool would run it before compiling a module.
- `NewObject`, `CreateDefaultSubobject`, garbage collection (`GarbageCollection.h`), `TWeakObjectPtr`,
  `TSubclassOf`, `ConstructorHelpers`.
- Once available: turn the naming-only `A`/`U` classes into real `UCLASS` types.

## Engine / platform

- Gameplay framework on PS2 (needs native math + containers first); then `Game/ThirdPerson` can use
  `AThirdPersonCharacter : ACharacter` like TP_ThirdPerson.
- Renderer through RHI command lists instead of direct GL calls; break the Engine ↔ Renderer cycle.
- `UNavigationSystemBase` seam so NavigationSystem can move to its own module.
- `PS2TargetPlatform` Developer module (cook formats for PS2: textures, LPS2 meshes).
- Texture mipmaps on PS2 (GS MIPTBP registers) — fixes floor moiré in ThirdPerson.
- AutomationTool homologue (`RunLAT`: build → cook → stage).
