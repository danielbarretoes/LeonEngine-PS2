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
| `U` | Classes that are `UObject`s in UE (components, assets, subsystems, widgets, engine objects). CoreUObject's types (`UObject`, `UClass`, `UPackage`, …) and its test fixtures derive from `UObject`; the engine's `U` classes are **naming only** until they become `UCLASS` types (P12) | `UObject`, `UClass`, `UGameEngine`, `UWorld`, `ULevel`, `UActorComponent`, `UCharacterMovementComponent`, `UTexture2D`, `UUserWidget`, `UCookCommandlet` |
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
T* CreateDefaultSubobject(ArgsType&&... Args)
{
	TUniquePtr<T> Owned = MakeUnique<T>(Forward<ArgsType>(Args)...);
	…
}
```

### 1.3 Module API macros

- Every public top-level class or struct (in `Public/` or `Classes/`) carries `<MODULE>_API`, the module name
  in upper case: `class ENGINE_API UWorld`, `struct CORE_API FPaths`, `class LAUNCH_API FEngineLoop`.
  Exported globals and free functions use it too (`extern RHI_API FDynamicRHI* GDynamicRHI;`,
  `CORE_API bool IsEngineExitRequested();`).
- LeonBuildTool defines `<MODULE>_API` as empty (static linking), but it is still required so the code keeps
  the UE shape. Private classes (`FGLFWWindow`, `FPS2Window`, `FGameApplication`) do not use it.

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
  `LegacyCoordinateConversion.h` outside the legacy bridge (its own files, the `.llev` reader and saver, the `.lmesh`
  reader, `Private/Tests` and `Engine/Public/Tests/LegacyGolden.h`). A violation prints
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
    (MeshUtilities); legacy `.llev` / version-1 `.lmesh` data goes through `FLegacyCoordinateConversion` in its reader
    only (G4 enforces it); the Jolt and miniaudio boundaries swap Y and Z and scale by 0.01 inside their own files.
    Engine code never holds legacy (Y-up, metre) values.
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
    constructor only. Objects are not deleted by hand (garbage collection arrives in P10).
  - Test types with `Cast<T>` / `CastChecked<T>` / `IsA<T>()`, never `dynamic_cast` (no RTTI, D17).
  - A class whose children may not declare a constructor gives itself an `FObjectInitializer` constructor: the
    generated default constructor calls `Super(ObjectInitializer)`.
  - A reflected Core struct is declared in `CoreUObject/Public/UObject/NoExportTypes.h` (`USTRUCT(noexport)` in
    `#if !CPP`); the generated code checks the declaration against the C++ type at compile time.

---

## 5. Namespaces

- There is **no `namespace leon`** and no global engine namespace: types live at global scope like UE, and
  the prefixes keep them distinct.
- `Leon::<Area>` is only for free functions, constants and plain structs that UE would put in a namespace: `Leon::InputActions` (action name constants), `Leon::PS2` (private helpers inside `PS2RHI/Private`).
- File-local helpers go in an anonymous namespace in the `.cpp`.
- No `using namespace` at global scope (inside a function body is acceptable, e.g.
  `using namespace Leon::InputActions;`).

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
| Content kind folders | PascalCase | `Engine/Content/Materials`, `Textures`, `LevelTemplates` |
| Materials | `M_<Name>.lmat` | `M_Default.lmat`, `M_WorldGrid.lmat` |
| Textures | `T_<Name>_<Suffix>` (`_D` diffuse, `_N` normal) | `T_Default_D.png` |
| Level templates / levels | PascalCase `.llev` | `Blank.llev`, `Starter.llev` |
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
- Reflected test types (`UCLASS` / `USTRUCT` fixtures) go in `<Module>/Private/Tests/*.h`; LeonHeaderTool compiles them
  into the test targets only (the `<Module>.Tests` unit).

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
