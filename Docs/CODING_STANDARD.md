# LeonEngine — coding standard

LeonEngine follows the **Epic Games coding standard (Unreal Engine 4.27)**, plus the Leon-specific rules
and deviations listed here. When this page is silent, do what Epic's standard says; when the two disagree,
this page wins. A local copy of Epic's standard may be kept under `Docs/UNREAL_ENGINE/` (not tracked).

**Also:** [ARCHITECTURE.md](ARCHITECTURE.md) · [BUILD.md](BUILD.md) ·
[UnrealEngine427/LeonMapping.md](UnrealEngine427/LeonMapping.md) (rename tables and deviations) ·
[ASSET_FORMATS.md](ASSET_FORMATS.md)

The rules are enforced by tools where possible:

| Tool | File | What it enforces |
| --- | --- | --- |
| clang-format | `.clang-format` | Layout: tabs, Allman braces, 120 columns, include order |
| clang-tidy | `.clang-tidy` | `readability-identifier-naming` (PascalCase, `E` enum prefix) + bug-prone checks; shown by clangd |
| Compiler | LeonBuildTool `CompileEnvironment.cmake` | Warnings (`/W4`, `-Wall -Wextra`), shadowing as an error |
| EditorConfig | `.editorconfig` | Tabs, UTF-8, final newline, line endings |
| Gate G4 | `Engine/Build/BatchFiles/CheckBannedApis.ps1` | Banned APIs and the legacy coordinate bridge (§4) |

---

## 1. Naming

All identifiers are English (U.S. spelling), **PascalCase**, with no underscores between words.

### 1.1 Type prefixes

| Prefix | Use | Examples |
| --- | --- | --- |
| `A` | Classes derived from `AActor` — **only** those | `AActor`, `APawn`, `ACharacter`, `APlayerController`, `AGameModeBase`, `AHUD` |
| `U` | Classes that are `UObject`s in UE (components, assets, subsystems, widgets, engine objects). CoreUObject's types, the gameplay framework (P12: `UWorld`, `ULevel`, `UGameInstance`, the components), the engine and its settings (P13: `UEngine`, `UGameEngine`, `UGameViewportClient`, `ULocalPlayer`, `UPlayerInput`, `UInputSettings`, `UGameMapsSettings`), the assets (P14: `UTexture2D`, `UStaticMesh`, `UMaterial`, `USkeleton`, `USkeletalMesh`, `UAnimSequence`, `UBlendSpace1D`, `USoundWave`, `UDataAsset`, `UCommandlet`, `UAssetImportData`), the editor module's factories and commandlets (P14: `UFactory`, `UTextureFactory`, `UImportAssetsCommandlet`, `UCookCommandlet`, ...), `UUserWidget` and `UAnimInstance` derive from `UObject`. A few `U` types are still **naming only** until their phase: `UNavigationSystem` and the behavior tree lite (`UBehaviorTree`, `UBTNode`, `UBlackboardComponent`) | `UObject`, `UClass`, `UWorld`, `ULevel`, `UActorComponent`, `UCharacterMovementComponent`, `UUserWidget`, `UGameEngine`, `UTexture2D` |
| `F` | Every other class or struct | `FEngineLoop`, `FTicker`, `FPaths`, `FSceneRenderer`, `FPhysScene`, `FHitResult`, `FPS2RHI` |
| `T` | Class templates | `TArray`, `TMap`, `TSharedPtr`, `TDelegate`, `TOptional` |
| `E` | Enums (prefer `enum class`, sized when stored) | `EKeys`, `EPhysicsBackend`, `EPostProcessQuality`, `ENetMsg` |
| `I` | Abstract interfaces (no data members) | `IModuleInterface`, `IInputInterface`, `IPhysicsBackend` |
| `G` | Global variables | `GEngineLoop`, `GDynamicRHI`, `GPrimaryGameModuleName` |

- Use the UE name when a type mirrors a UE type (`GenericApplication` keeps UE's unprefixed name).
  [LeonMapping.md](UnrealEngine427/LeonMapping.md) lists every mapping.
- Pick `U` for a new type only when its UE homologue is a `UObject`; do not use `A` for anything that is not
  an actor. Plain data and helpers are `F`.
- Typedefs take the prefix of what they alias (`typedef FWindowsPlatformMemory FPlatformMemory;`); delegate types
  declared with `DECLARE_DELEGATE*` are `F` (`DECLARE_DELEGATE_RetVal_OneParam(bool, FTickerDelegate, float);`).
- Enumerators are PascalCase (`EPhysicsBackend::Arcade`). UE enumerators with underscores are kept as UE
  spells them (`EKeys::Gamepad_FaceButton_Bottom`); `.clang-tidy` whitelists `Gamepad_*`.

### 1.2 Members, functions, parameters and locals

| Kind | Rule | Examples |
| --- | --- | --- |
| Member variables (public or private) | PascalCase, no `m_`, no trailing `_` | `ExitCode`, `MainWindow`, `LastFrameCycles` |
| Methods and free functions | PascalCase verbs; a function that returns a value names the value | `StartupModule()`, `PollGameDeviceState()`, `LoadLevelFile()` |
| Parameters and locals | PascalCase | `DeltaTime`, `NowCycles`, `ModuleManager` |
| Constants (`constexpr`, `static const`) | PascalCase, no `k` prefix | `MainWindowWidth`, `MaxPointLights`, `InvalidTexture`, `MaxOnScreenMessages` |
| Booleans (members, params, locals) | `b` prefix | `bHeadless`, `bCursorCaptured`, `bEnabled` |
| Macros | `UPPER_SNAKE_CASE` | `IMPLEMENT_MODULE`, `COMPILED_PLATFORM_HEADER`, `PLATFORM_DESKTOP` |

**Accessors and questions**

- Bool-returning functions ask a question: `Is…`, `Has…`, `Should…`, `Can…`
  (`IsEngineExitRequested()`, `IsGamepadConnected()`, `ShouldClose()`, `HasPath()`).
- An accessor whose natural name would collide with a member becomes `GetX()`
  (`UButton::GetId()` returns member `Id`; `FEngineLoop::GetExitCode()` returns `ExitCode`).

**`In` / `Out` parameters**

- A parameter the function writes through a reference/pointer is prefixed `Out`
  (`GetSkinMatrices(TArray<FMatrix>& OutSkin)`, `GetStaticallyLinkedModules(int32& OutCount)`).
  A bool out-parameter is `bOutX`.
- A parameter that would shadow a member is prefixed `In` (`SetActorLocation(const FVector& InLocation)`
  → `Location = InLocation;`, `FGenericWindow::Create(int InWidth, int InHeight, …)`).
- A local that would shadow a member or a namespace-scope name is prefixed `Local`
  (`const USkeleton* LocalSkeleton = GetSkeleton();`).

**Templates**

- Template parameters are descriptive with a `Type` suffix where it helps; variadic packs are
  `template <typename... ArgsType>` with `ArgsType&&... Args`:

```cpp
template <typename T, typename... ArgsType>
TUniquePtr<T> MakeUnique(ArgsType&&... Args)
{
	return TUniquePtr<T>(new T(Forward<ArgsType>(Args)...));
}
```

### 1.3 Module API macros

- Every public top-level class or struct (in `Public/` or `Classes/`) carries `<MODULE>_API`, the module name
  in upper case: `class ENGINE_API UWorld`, `struct CORE_API FPaths`, `class LAUNCH_API FEngineLoop`.
  Exported globals and free functions use it too (`extern RHI_API FDynamicRHI* GDynamicRHI;`,
  `CORE_API bool IsEngineExitRequested();`).
- LeonBuildTool defines `<MODULE>_API` as empty (static linking), but it is still required so the code keeps
  the UE shape. Private classes (`FGLFWWindow`, `FPS2Window`, `FOpenGLDynamicRHI`) do not use it.

### 1.4 Static classes vs free functions

Sets of related free functions become a static class **only where UE has that homologue**
(`FPaths`, `FFileHelper`, `FCString`, `FParse`, `FJsonSerializer`, `UGameplayStatics`, `FCookRecipe`, `FPS2RHI`). Otherwise
keep PascalCase free functions, as UE does with `DrawDebugLine` (`LoadLevelFile`, `LoadObj`,
`CreatePhysicsBackend`).

---

## 2. Files, folders and includes

- **File name = type name without its prefix**: `GameEngine.h` → `UGameEngine`, `Actor.h` → `AActor`,
  `Ticker.h` → `FTicker`, `DynamicRHI.h` → `FDynamicRHI`. When mirroring UE, use UE's file name even if it
  differs (`LaunchEngineLoop.h` → `FEngineLoop`, `InputCoreTypes.h` → `EKeys`). No snake_case C++ files.
- **Module anatomy** (UE): `Public/` for headers other modules include, `Classes/` for public gameplay classes
  (`Engine/Classes/GameFramework/Actor.h`), `Private/` for sources, private headers, platform subfolders and
  `Tests/`. Group by area as UE does (`HAL/`, `GenericPlatform/`, `Misc/`, `Modules/`, `GameFramework/`,
  `Components/`, `Kismet/`, …).
- One module implementation file registers the module: `Private/<Module>Module.cpp` with
  `IMPLEMENT_MODULE(FDefaultModuleImpl, <Module>)` or a custom `IModuleInterface`.
- `#pragma once` in every header.
- **Includes**:
  - Engine and game headers: quotes, **module-relative** path from `Public/`, `Classes/` or `Private/`:
    `#include "HAL/PlatformTime.h"`, `#include "GameFramework/Actor.h"`, `#include "LaunchEngineLoop.h"`.
    Never `../` paths.
  - Third-party and system headers: angle brackets (`<glad/glad.h>`, `<cstring>`).
  - Order (enforced by clang-format `IncludeBlocks: Regroup`): the file's own header first, then engine
    headers, then third-party, then standard / SDK headers, one blank line between blocks. A reflected header's
    `"<Header>.generated.h"` sorts after the other engine headers (LeonHeaderTool requires it last).
  - Include what you use; prefer forward declarations in headers; include the specific header, not a
    catch-all.
- Tests live in `<Module>/Private/Tests/` and are compiled only into targets with `COLLECT_AUTOMATION_TESTS`
  (`LeonAutomationTests`, `TestPAL`); see §10.

---

## 3. Formatting

Formatting is **not** a matter of taste: run the formatter.

- `Engine\Build\BatchFiles\FormatCode.bat` formats every `.cpp` / `.h` / `.inl` under `Engine\Source`,
  `Engine\Platforms`, `Engine\Plugins` and `Game\` (skips `ThirdParty`, `Intermediate`, `Binaries`; never
  touches GLSL). `FormatCode.bat --check` is a dry run that fails if anything needs formatting.
- `Engine\Build\BatchFiles\Lint.bat` = format check + banned-API check (`CheckBannedApis.ps1`, gate G4; §4) + Win64
  Development build of `LeonAutomationTests`, `LeonCook`, `LeonGame` and `BlankProgram` (warnings on, shadowing as
  errors).
- `.clang-format` (Epic style): tabs for indentation (width 4), **Allman braces** everywhere (including
  one-line functions), 120 columns, `public:` aligned with `class`, constructor initializers and base lists
  broken before the comma, `PointerAlignment: Left` (`FShaderType* Ptr`), namespaces indented.
- Always use braces, even for single statements. `switch` cases are indented; use `break` or a
  "falls through" comment.
- ThirdParty folders carry their own `.clang-format` with formatting disabled; never reformat third-party
  code. Bulk reformatting commits go into `.git-blame-ignore-revs`.

```cpp
int32 FEngineLoop::PreInit(int32 ArgC, char* ArgV[])
{
	ArgCount = ArgC;
	Args = ArgV;

	FModuleManager::Get().StartupStaticallyLinkedModules();
	return 0;
}
```

---

## 4. Language

- **C++ standard.** All engine and game code is **C++17** on every platform (Win64, Linux, PS2), as in UE 4.27 and
  the EE toolchain; LeonBuildTool registers C++17 for each platform. Do not use C++20 features.
- **Shadowing is a compile error** (UE: `ShadowVariableWarningLevel = Error`): MSVC (Win64)
  `/we4456 /we4457 /we4458 /we4459`, PS2 GCC `-Werror=shadow`. The Linux host flags
  (`-Wall -Wextra -Wpedantic`) do not include it yet, so verify on Win64 or PS2. Resolve shadowing with the
  `In` / `Local` prefixes (§1.2), never by disabling the diagnostic.
- Fix warnings (`/W4`, `-Wall -Wextra`); suppressing one is a last resort.
- **No RTTI and no C++ exceptions** (plan decision D17, UE's `bUseRTTI` / `bEnableExceptions` defaults). LeonBuildTool
  compiles Leon code with `/GR-`, no `/EH` flag and `_HAS_EXCEPTIONS=0` on MSVC, `-fno-rtti -fno-exceptions` on GCC /
  Clang (the PS2 toolchain passes them to everything). No `dynamic_cast` or `typeid`: test `UObject` types with
  `Cast<T>` / `IsA<T>()`, and give a plain class hierarchy a virtual query instead. No `try` / `catch` / `throw`:
  report failures with return values, `check` / `ensure` and `UE_LOG`. Third-party libraries keep their own flags
  (`leon_third_party_cxx_defaults` restores exceptions and RTTI for one that needs them), and `LeonHeaderTool`, a
  std-only host program like UE's UnrealHeaderTool, uses exceptions for its parse aborts.
- `nullptr`, `override` + `virtual` on overrides (`virtual void StartupModule() override;`), `final` on
  classes not designed for derivation, `static_assert` for compile-time checks, `enum class` for new enums.
- `auto` only where Epic allows it: lambdas, verbose iterator / `std::chrono` types, template code.
- Const-correctness: const methods, const references for input parameters, `const` locals that do not change.
- Default member initializers are fine (`int32 ExitCode = 0;`).
- Interfaces (`I*`) have no data members.
- **Containers and strings.** New code that depends on Core (including PS2 code) uses the UE types from
  `CoreMinimal.h`: `TArray`, `TMap`, `TSet`, `FString`, `FName`, `FText`, `TUniquePtr` / `TSharedPtr`,
  `TFunction` and delegates. `TCHAR` is UTF-8 `char` on every platform, so write literals with `TEXT("...")`. Element
  types stored in UE containers must be relocatable with `memmove` (no pointers into themselves).
- **Banned APIs (gate G4).** Every module uses the UE types and Core math (`FVector`, `FRotator`, `FQuat`,
  `FMatrix`, `FTransform`, `FMath`, …) since P6. `Engine\Build\BatchFiles\CheckBannedApis.ps1` (run by `Lint.bat` and
  CI) scans `Engine\Source`, `Engine\Platforms`, `Engine\Plugins` and `Game`, ignoring comments, and rejects: glm and
  nlohmann (use Core math and the `Json` module); `std::vector`, `std::string`, `std::map`, `std::unordered_map`,
  `std::function`, `std::shared_ptr`, `std::unique_ptr` (use `TArray`, `FString`, `TMap`, `TFunction`,
  `TSharedPtr`, `TUniquePtr`); `<iostream>`, `std::cout`, `std::cerr`, `std::clog` and the `printf` family (use
  `UE_LOG`, `FString::Printf`, `FCString`). They are allowed only where Core wraps the C and C++ libraries (D2):
  ThirdParty folders, the platform HAL sources (`Private/Windows`, `Private/Linux`, the PS2 Core extension), the
  `printf` family inside `Runtime/Core/Private`, `LeonHeaderTool` (a std-only host program) and the test
  program mains (`LeonAutomationTestsMain.cpp`, `TestPAL/Private`). A third-party library's own types stay in the
  file that calls it (Jolt, tinyobjloader, ufbx, cgltf). Do not add aliases that pretend to be UE types
  (`using FVector = glm::vec3` is not allowed). G4 also rejects the legacy math bridges removed in P7 (`LegacyGL`,
  `FLegacyTransform`, `LegacyAxes`, tests included) and `FLegacyCoordinateConversion` /
  `LegacyCoordinateConversion.h` outside the tests (`Public/Tests` and `Private/Tests` folders: the converter itself,
  the golden adapters and the tests; since P15 no runtime or editor code holds legacy data). A violation prints
  `<file>:<line>: G4 <rule>: <code> -> <what to use>`.
- **Math is float.** No `double` arithmetic in engine code (the EE FPU is single precision); PS2 builds fail on an
  implicit float to double promotion (`-Werror=double-promotion`), so cast explicitly where a `double` is really
  meant (`Printf` arguments, `FTicker`'s clock).
- **Coordinates** (details: [ARCHITECTURE.md §6, Coordinates](ARCHITECTURE.md#coordinates)). The world is UE's:
  X forward, Y right, Z up, left-handed, 1 unit = 1 cm.
  - Write lengths, speeds and offsets in centimetres (`JumpZVelocity = 700.0f`, `MaxStepHeight = 35.0f`) and say so
    in the comment; masses stay in kg. `FVector::UpVector`, `ForwardVector` and `RightVector` are the world axes;
    vertical is `Z` (`FloorZ`, `VelocityZ`, `QuerySupportZ`), horizontal is `X` / `Y` (`VelXY`, `ClampPositionXY`).
  - Rotations are `FRotator` / `FQuat` / `FTransform` in UE's sense: a positive yaw turns right (from +X toward +Y),
    a positive pitch looks up. Build a right vector with `Up ^ Forward` (left-handed), never `Forward ^ Up`.
  - Matrices are `FMatrix` in UE's row-vector order (`Model * View * Projection`); build views with
    `MakeViewMatrix` / `MakeLookAtView` (`RenderCore/Public/ViewMatrices.h`) and projections with `FPerspectiveMatrix`
    / `FOrthoMatrix`. OpenGL code applies `ToGLClipSpace` (`RenderCore/Public/GLClipSpace.h`) once, after the
    projection, and uploads with `FShader::SetMat4(Name, const FMatrix&)`.
  - Data from outside the world is converted where it enters: importers end with `FImportCoordinateConversion`
    (MeshUtilities; the map importer converts node transforms and light directions with it too); the Jolt and
    miniaudio boundaries swap Y and Z and scale by 0.01 inside their own files. Engine code never holds legacy (Y-up,
    metre) values: only the golden tests convert their legacy tables, with the test-only `FLegacyCoordinateConversion`
    (G4 enforces it).
  - Keep a triangle's index order when converting data (every basis change has determinant −1 and keeps the winding
    on screen); a tangent's `w` flips with the basis.
- **Logging.** Log through `UE_LOG(<Category>, <Verbosity>, TEXT("..."), ...)` with a category
  (`DECLARE_LOG_CATEGORY_EXTERN` + `DEFINE_LOG_CATEGORY` for a module-wide one, `DEFINE_LOG_CATEGORY_STATIC` inside
  one `.cpp`), not `printf` / `std::cout`. On PS2 the log reaches the EE console.
- **Assertions.** `check` / `checkf` for invariants (stripped in Shipping), `verify` when the expression must run in
  every build, `checkNoEntry` for unreachable paths, `ensure` / `ensureMsgf` for recoverable failures that should be
  reported.
- **Files.** Open, read and list files through `IFileManager::Get()`, `FFileHelper` or
  `FPlatformFileManager::Get().GetPlatformFile()`, and build paths with `FPaths` (`FPaths::ProjectContentDir() /
  "Maps"`). No `fopen`, `std::fstream` or `std::filesystem` in engine code: on the PS2 the only backend is the
  platform file layer, and later a pak file layer sits on top of it.
- **Settings and flags.** A tunable goes in the config (`GConfig->GetFloat(Section, Key, Value, GGameIni)`, section
  `/Script/<Module>.<Class>` as UE names it) with the compiled value as the default, so the code still works when the
  file cannot be read (PS2 without the PCSX2 host filesystem). Command-line switches are read with
  `FParse::Param` / `FParse::Value` on `FCommandLine::Get()` (`-name` / `-name=value`), never from `argv`.
- Use the Core fixed-width types (`int32`, `uint64`, …) from `CoreTypes.h` in engine APIs, not `std::uint8_t` and
  the like.
- **UObjects** (CoreUObject, [README](../Engine/Source/Runtime/CoreUObject/README.md)). A reflected header includes
  `"<Header>.generated.h"` as its **last** include (clang-format keeps it there) and puts `GENERATED_BODY()` first in
  every `UCLASS` / `USTRUCT`. LeonHeaderTool's supported subset is in its
  [README](../Engine/Source/Programs/LeonHeaderTool/README.md).
  - Create objects with `NewObject<T>(Outer, …)`, never `new`, and subobjects with `CreateDefaultSubobject` inside the
    constructor only. Objects are never deleted by hand: the garbage collector destroys what nothing references;
    `MarkPendingKill` asks for an object to go at the next collection (it is then collected even if referenced, and
    the strong references to it are cleared).
  - **GC safety.** A `UObject*` member of a `UObject` or `USTRUCT` is a `UPROPERTY()` (containers and structs of them
    too): otherwise the collector does not see it, may destroy the object and leave the pointer dangling. A class that
    must keep other references declares a static `AddReferencedObjects(UObject* InThis, FReferenceCollector&)` that
    calls `Super::AddReferencedObjects` and reports them. Code that is not a `UObject` and keeps objects alive
    derives from `FGCObject` (reporting them in `AddReferencedObjects`) or holds a `TStrongObjectPtr`. Use
    `TWeakObjectPtr` (or a `UPROPERTY` `TWeakObjectPtr`) for references that must not keep the object alive, and
    check it before use. A local `UObject*` is only safe until the next `CollectGarbage`, which runs at safe points
    only: the world teardown (`UEngine::LoadMap`, `UGameEngine::PreExit`, a test's `FScopedTestWorld`), a level
    (re)load, `obj gc`, the engine's timer after the world tick (`UEngine::ConditionalCollectGarbage`); never inside a
    constructor or a tick.
  - **Gameplay objects** (P12). Spawn actors with `UWorld::SpawnActor<T>(…)` (never `NewObject` or a local
    `AActor`), destroy them with `Destroy()`: the actor ends play and leaves its level at once and the next collection
    frees it, clearing every `UPROPERTY` that points at it (check actor pointers with `IsValid` or
    `IsPendingKillPending` when a destroy may have happened this frame). Give actors their components in the
    constructor: `CreateDefaultSubobject<T>(TEXT("Name"))`, `SetupAttachment(Parent, Socket)`, the root through
    `RootComponent` (a subclass with its own root skips `AActor::DefaultSceneRootName` with
    `ObjectInitializer.DoNotCreateDefaultSubobject`). A component added later is `NewObject<T>(Actor)` followed by
    `RegisterComponent()`. Overrides call `Super`: first in `BeginPlay`, `PostInitializeComponents`, `OnRegister`,
    last in `EndPlay` and `Destroyed`.
  - Config values of a class are `UPROPERTY(Config)` members of a `UCLASS(Config=<File>)` (read automatically into the
    class default object; section `/Script/<Module>.<Class>`), not hand-written `GConfig` reads, once the class is a
    `UObject`.
  - Console commands are `UFUNCTION(Exec)` members (`CallFunctionByNameWithArguments`) of an object on the player's
    Exec chain (the player input, the player controller, the pawn, the game mode, the game state, the world settings,
    the game instance), or an `FSelfRegisteringExec` outside UObjects.
  - **Input** (P13). Keys are `FKey`s (`EKeys::SpaceBar`), never raw codes. Gameplay binds named actions and axes
    (`BindAction`, `BindAxis`) in `APawn::SetupPlayerInputComponent` or `APlayerController::SetupInputComponent`; the
    keys that drive them are mappings in `BaseInput.ini` / a project's `DefaultInput.ini`
    (`[/Script/Engine.InputSettings]`), not code.
  - Test types with `Cast<T>` / `CastChecked<T>` / `IsA<T>()`, never `dynamic_cast` (no RTTI, D17).
  - A class whose children may not declare a constructor gives itself an `FObjectInitializer` constructor: the
    generated default constructor calls `Super(ObjectInitializer)`.
  - A reflected Core struct is declared in `CoreUObject/Public/UObject/NoExportTypes.h` (`USTRUCT(noexport)` in
    `#if !CPP`); the generated code checks the declaration against the C++ type at compile time.
  - **Saving.** The `UPROPERTY`s of an object are saved in packages as tagged properties (what differs from the
    archetype); anything else a class must save goes in a `Serialize(FArchive& Ar)` override that calls
    `Super::Serialize(Ar)` first and then serializes the same members in the same order whether `Ar` loads or saves.
    Serialize `FName` and `UObject*` through `Ar` (the linker turns them into table indices), never as raw bytes or
    pointers, and large payloads through an `FByteBulkData`. Save output must be deterministic (D13): no addresses,
    times or iteration over hashes (a `TMap` / `TSet` iterates in insertion order, which is fine). When a native
    format changes, add an `ELeonPackageVersion` value (`Core/Public/UObject/ObjectVersion.h`) and guard the new data
    with `if (Ar.UEVer() >= VER_LEON_<Change>)`; renaming or retyping a `UPROPERTY` needs nothing (tagged properties
    skip what they cannot load and convert numbers and enums).
  - Load objects with `LoadObject<T>(nullptr, TEXT("/Game/Path/Asset.Asset"))` or a `TSoftObjectPtr`
    (`LoadSynchronous`), name packages with long package names (`/Game/...`, `/Engine/...`) and convert to files
    only through `FPackageName`.
  - **Assets** (P14). Code holds an asset through a `UPROPERTY` (`UStaticMeshComponent::StaticMesh`,
    `UMeshComponent::OverrideMaterials`, a material's maps), never a copy of its data or a `TSharedPtr`; a default the
    engine needs is a `UPROPERTY(Config)` / `GlobalConfig` `FSoftObjectPath` of its config class (`UEngine`'s
    `DefaultMaterialName`), not a path in code. An asset's big arrays are bulk data in its native tail
    (`FByteBulkData`; `SerializeBulkPayload` for arrays it keeps on the CPU). Its GPU copy is the Renderer's: call
    `UpdateResource` / `InitResources` after changing its data, and release it in `BeginDestroy`. Assets load from
    packages (`LoadObject`, `TSoftObjectPtr`); run-time code never reads an image, sound or mesh source file: LeonEd's
    factories import them. An imported asset class keeps an editor-only `UPROPERTY(Instanced) UAssetImportData*
    AssetImportData` (inside `#if WITH_EDITORONLY_DATA`), and an import records nothing that changes between runs
    (no timestamps, no absolute paths under the source root), so reimports are reproducible (gate G5).
  - **Editor code** (P14) lives in `Engine/Source/Editor` modules (`TYPE Editor`, LeonEd): factories derive from
    `UFactory`, commandlets from `UCommandlet` (`U<Name>Commandlet`, found by `-run=<Name>`). Runtime and game modules
    never depend on an editor module (LeonBuildTool rejects it in a game target).

---

## 5. Namespaces

- There is **no `namespace leon`** and no global engine namespace: types live at global scope like UE, and
  the prefixes keep them distinct.
- `Leon::<Area>` is only for free functions, constants and plain structs that UE would put in a namespace: `Leon::PS2` (private helpers inside `PS2RHI/Private`).
- File-local helpers go in an anonymous namespace in the `.cpp`.
- No `using namespace` at global scope (inside a function body is acceptable, e.g.
  `using namespace Leon::PS2;`).

---

## 6. Platform code

- Platform-specific code lives only in platform folders: `Private/Windows`, `Private/Linux`,
  `Private/Desktop` (GLFW / OpenGL), `Public/<Platform>/` for HAL headers, and the platform extension
  `Engine/Platforms/PS2/Source/...`. LeonBuildTool drops foreign platform folders automatically.
- **No `PLATFORM_PS2` checks outside PS2 folders** (and no `PLATFORM_WINDOWS` / `_WIN32` outside Windows
  folders). Extend the HAL instead (`FPlatformTime`, `FPlatformMemory`, `FPlatformMath`,
  `FPlatformApplicationMisc`, `FPlatformEngineLoopHooks`), or add a capability macro with a default in
  `HAL/Platform.h` that the platform header overrides (`PLATFORM_DESKTOP`, `PLATFORM_64BITS`).
- Include platform headers through `COMPILED_PLATFORM_HEADER(PlatformMemory.h)` from a `HAL/` header.
- Dependencies that only exist on some platforms use suffixed keywords in the `.Build.cmake`
  (`PRIVATE_DEPENDENCIES_Desktop GLFW`) or the extension's `leon_module_extend`.
- Known exceptions to remove: `Core/Private/HAL/MallocAnsi.cpp` and `Core/Private/Misc/OutputDeviceRedirector.cpp`
  still test `PLATFORM_WINDOWS`.

---

## 7. Comments

- Doc comments on public API in Epic's JavaDoc style (`/** … */`) above the declaration; explain intent,
  units and ranges, not the implementation. Older code uses `///`, which is acceptable when touching it.
- Mention the UE homologue when a type mirrors one (`/** … (UE: FEngineLoop). */`).
- No commented-out code; debug code is either useful and polished or not committed.

---

## 8. Content naming

| Asset | Convention | Example |
| --- | --- | --- |
| Content folders | PascalCase, UE's where one exists | `Engine/Content/EngineMaterials`, `EngineResources`, `BasicShapes`, `Maps` |
| Assets (packages) | `<Prefix>_<Name>.lasset` with UE's prefixes: `SM_` static mesh, `SK_` skeletal mesh, `SKEL_` skeleton, `A_` animation, `BS_` blend space, `T_` texture, `M_` material, `S_` sound wave; maps `<Name>.lmap`; long package name = content path without extension | `/Engine/EngineMaterials/M_Default` → `Engine/Content/EngineMaterials/M_Default.lasset` |
| Textures | `T_<Name>_<Suffix>` (`_D` diffuse, `_N` normal: imported linear) | `T_Default_D`, `T_Default_Bump_N` |
| Source art | outside `Content`: `<Engine or Project>/SourceArt/`, folders mirroring the package paths, plus `ImportList.ini` | `Engine/SourceArt/EngineMaterials/T_Default_D.png`, `Engine/SourceArt/Maps/AxisTest.glb` |
| Maps | `<Name>.lmap` in `Content/Maps` (UE's `Maps` folder), no prefix; an imported map's meshes and materials in `Maps/<Name>/Meshes` and `Maps/<Name>/Materials` | `/Engine/Maps/Template_Default`, `/Engine/Maps/AxisTest/Meshes/SM_RedCube` |
| Map source nodes (Blender objects) | the map importer's prefixes ([LEVELS.md](LEVELS.md#naming-conventions)): `UCX_<Mesh>_<NN>`, `COL_`, `Clip_`, `PlayerStart_<Tag>`, `NavWaypoint`, a project's own (ShooterGame: `BombSite_<A\|B>`, `BuyZone_<CT\|T>`) | `UCX_CrateStack_01`, `PlayerStart_CT`, `BombSite_A` |
| Source art scripts | a script that builds source art in Blender sits next to its output, `snake_case.py`, Blender's modules only, run headless (`blender --background --factory-startup --python <script>`); it saves the `.blend` and exports the `.glb` | `Game/ShooterGame/SourceArt/Maps/make_de_leon.py` |
| Source art licenses | a project's `SourceArt/LICENSES.md` lists every file with its origin and license (ShooterGame: CC0 only) | `Game/ShooterGame/SourceArt/LICENSES.md` |
| GLSL shaders (`Engine/Shaders`) | snake_case | `blinn_phong.vert`, `post_composite.frag` |

File formats: [ASSET_FORMATS.md](ASSET_FORMATS.md).

---

## 9. Checklist for a change

1. Types carry the right prefix; members / functions / params / locals are PascalCase; bools start with `b`.
2. Public classes carry `<MODULE>_API`; file name matches the type without its prefix.
3. Includes are quoted and module-relative; no `../`.
4. Platform code is in a platform folder; no new `PLATFORM_*` checks in shared code.
5. The code is C++17 and uses no banned API (§4).
6. `Engine\Build\BatchFiles\FormatCode.bat` has been run; `Lint.bat` passes (format, `CheckBannedApis.ps1`, build).
7. Tests for new behaviour live in `<Module>/Private/Tests/` and pass with `RunTests.bat` (Core changes: also
   `TestPAL` on PS2).
8. New Core-dependent code logs with `UE_LOG` and a category and asserts with `check` / `ensure`.
9. World values are in UE space and centimetres; data from other spaces is converted only at its boundary (§4,
   Coordinates).

---

## 10. Tests

- **Automation tests** (UE): `IMPLEMENT_SIMPLE_AUTOMATION_TEST(F<Name>Test, "System.<Module>.<Area>.<Name>", <Flags>)`
  in `<Module>/Private/Tests/<Area>Test.cpp` (`<Area>Tests.cpp` in the modules migrated in P5 / P6), with
  `bool F<Name>Test::RunTest(const FString& Parameters)` using `TestEqual` / `TestTrue` / `TestNotNull` / …; wrap the
  file in `#if WITH_DEV_AUTOMATION_TESTS`. An error logged during a test fails it unless the test declares it with
  `AddExpectedError` (the two tests that feed `DeserializeLeonLevel` a bad buffer do). Every test follows this form
  (`System.Core.Containers.Array`, `System.Engine.PhysScene.…`, `System.JoltPhysics.Step.…`); Catch2 is gone.
- `RunTests.bat` runs all of them (`LeonAutomationTests`, `-automation=<filter>`); `TestPAL` runs the Core,
  CoreUObject, Json and Projects tests on every platform, including PS2.
- A game project's tests are named `<Project>.<Area>.<Name>` (`ShooterGame.Spawn.BotFill`), live in its module's
  `Private/Tests/` and run in the project's test program (`<Project>Tests.Target.cmake`: `LAUNCH_MODULE
  LeonAutomationTests`, `COLLECT_AUTOMATION_TESTS`, `AUTOMATION_TEST_MODULES <Project>`), which `RunTests.bat` builds
  and runs after the engine's.
- Reflected test types (`UCLASS` / `USTRUCT` fixtures) go in `<Module>/Private/Tests/*.h`; LeonHeaderTool compiles them
  into the test targets only (the `<Module>.Tests` unit).
- A test that needs actors creates its world with `FScopedTestWorld` (`Engine/Public/Tests/ScopedTestWorld.h`):
  `FScopedTestWorld TestWorld; UWorld& World = *TestWorld;`. At the end of the scope the world ends play, is destroyed
  and the garbage is collected, so no test leaves objects behind. Other objects come from `NewObject<T>()`; a
  `UObject` is never a local by value.

```cpp
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArrayTest, "System.Core.Containers.Array",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FArrayTest::RunTest(const FString& Parameters)
{
	TArray<int32> Values = {3, 1, 2};
	Values.Sort();
	TestEqual(TEXT("First after Sort"), Values[0], 1);
	return true;
}

#endif
```
