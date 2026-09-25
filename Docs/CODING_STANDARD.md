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

---

## 1. Naming

All identifiers are English (U.S. spelling), **PascalCase**, with no underscores between words.

### 1.1 Type prefixes

| Prefix | Use | Examples |
| --- | --- | --- |
| `A` | Classes derived from `AActor` — **only** those | `AActor`, `APawn`, `ACharacter`, `APlayerController`, `AGameModeBase`, `AHUD` |
| `U` | Classes that are `UObject`s in UE (components, assets, subsystems, widgets, engine objects). **Naming only**: there is no `UObject` base, reflection or GC yet | `UGameEngine`, `UWorld`, `ULevel`, `UActorComponent`, `UCharacterMovementComponent`, `UTexture2D`, `UUserWidget`, `UCookCommandlet` |
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
  (`GetSkinMatrices(std::vector<glm::mat4>& OutSkin)`, `GetStaticallyLinkedModules(int32& OutCount)`).
  A bool out-parameter is `bOutX`.
- A parameter that would shadow a member is prefixed `In` (`SetActorLocation(const glm::vec3& InLocation)`
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
	auto Owned = std::make_unique<T>(std::forward<ArgsType>(Args)...);
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
(`FPaths`, `FFileHelper`, `FCString`, `FJsonUtils`, `UGameplayStatics`, `FCookRecipe`, `FPS2RHI`). Otherwise
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
  - Third-party and system headers: angle brackets (`<glm/vec3.hpp>`, `<memory>`).
  - Order (enforced by clang-format `IncludeBlocks: Regroup`): the file's own header first, then engine
    headers, then third-party, then standard / SDK headers, one blank line between blocks.
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
- `Engine\Build\BatchFiles\Lint.bat` = format check + Win64 Development build of `LeonAutomationTests`,
  `LeonCook`, `LeonGame` and `BlankProgram` (warnings on, shadowing as errors).
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

- **C++ standard.** Code that compiles for PS2 (modules with no `PLATFORMS` restriction such as Core,
  InputCore, RHI, ApplicationCore and Launch, the PS2 extension and the PS2 game) must be **C++17** — the EE
  toolchain's standard; LeonBuildTool builds such module libraries as C++17 on every platform. Desktop-only
  modules (`PLATFORMS Desktop` / `Win64`) are **C++20**. C++20 features in shared code break the PS2 build.
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
- **Standard library and math (deviation).** Modules above Core keep `std::` containers, `std::string`,
  `std::unique_ptr` / `std::function` and **glm** on desktop until they migrate (P5–P6). Core has UE's math
  (`FVector`, `FRotator`, `FQuat`, `FMatrix`, `FTransform`, `FMath`, …); new code that only depends on Core uses it.
  Where Core math meets glm code, convert explicitly with `ToGlm` / `FromGlm` (`Migration/GlmInterop.h`, desktop
  only). Do not add aliases that pretend to be UE types (`using FVector = glm::vec3` is not allowed). See
  [NextSteps.md](UnrealEngine427/NextSteps.md).
- **Math is float.** No `double` arithmetic in engine code (the EE FPU is single precision); PS2 builds fail on an
  implicit float to double promotion (`-Werror=double-promotion`), so cast explicitly where a `double` is really
  meant (`Printf` arguments, `FTicker`'s clock).
- **World axes until P7.** Core math uses UE's axes (X forward, Y right, Z up), but the world is still Y-up in metres.
  Take world directions from `LegacyAxes` (`Migration/LegacyAxes.h`: `Up`, `Forward`, `Right`, `UnitsPerMetre`), not
  from `FVector::UpVector`, `ForwardVector` or `RightVector`, so the P7 switch finds every use.
- **Logging.** Log through `UE_LOG(<Category>, <Verbosity>, TEXT("..."), ...)` with a category
  (`DECLARE_LOG_CATEGORY_EXTERN` + `DEFINE_LOG_CATEGORY` for a module-wide one, `DEFINE_LOG_CATEGORY_STATIC` inside
  one `.cpp`), not `printf` / `std::cout`. On PS2 the log reaches the EE console.
- **Assertions.** `check` / `checkf` for invariants (stripped in Shipping), `verify` when the expression must run in
  every build, `checkNoEntry` for unreachable paths, `ensure` / `ensureMsgf` for recoverable failures that should be
  reported.
- Use the Core fixed-width types (`int32`, `uint64`, …) from `CoreTypes.h` in engine APIs; `std::uint8_t`
  style types remain in older code.

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
- Known exception to remove: `Core/Private/Misc/Paths.cpp` still tests `PLATFORM_WINDOWS`.

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
5. Shared (PS2-capable) modules stay C++17.
6. `Engine\Build\BatchFiles\FormatCode.bat` has been run; `Lint.bat` passes.
7. Tests for new behaviour live in `<Module>/Private/Tests/` and pass with `RunTests.bat` (Core changes: also
   `TestPAL` on PS2).
8. New Core-dependent code logs with `UE_LOG` and a category and asserts with `check` / `ensure`.

---

## 10. Tests

- **Automation tests** (UE): `IMPLEMENT_SIMPLE_AUTOMATION_TEST(F<Name>Test, "System.<Module>.<Area>", <Flags>)` in
  `<Module>/Private/Tests/<Area>Test.cpp`, with `bool F<Name>Test::RunTest(const FString& Parameters)` using
  `TestEqual` / `TestTrue` / `TestNotNull` / …; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`. An error logged
  during a test fails it unless the test declares it with `AddExpectedError`. Core's tests follow this form
  (`System.Core.Containers.Array`, `System.Core.HAL.Memory`, …).
- **Catch2** remains for the modules not migrated yet (RenderCore, Renderer, PhysicsCore, AnimationCore, Engine,
  AIModule, MeshUtilities, JoltPhysics); their files are `<Topic>Tests.cpp`. They move to automation tests with the
  module migration (P5–P6).
- `RunTests.bat` runs both kinds (`LeonAutomationTests`); `TestPAL` runs the automation tests on every platform,
  including PS2.

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
