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
| `T` | Class templates | *(none yet; reserved for Core containers)* |
| `E` | Enums (prefer `enum class`, sized when stored) | `EKeys`, `EPhysicsBackend`, `EPostProcessQuality`, `ENetMsg` |
| `I` | Abstract interfaces (no data members) | `IModuleInterface`, `IInputInterface`, `IPhysicsBackend` |
| `G` | Global variables | `GEngineLoop`, `GDynamicRHI`, `GPrimaryGameModuleName` |

- Use the UE name when a type mirrors a UE type (`GenericApplication` keeps UE's unprefixed name).
  [LeonMapping.md](UnrealEngine427/LeonMapping.md) lists every mapping.
- Pick `U` for a new type only when its UE homologue is a `UObject`; do not use `A` for anything that is not
  an actor. Plain data and helpers are `F`.
- Typedefs take the prefix of what they alias (`typedef FWindowsPlatformMemory FPlatformMemory;`,
  `using FTickerDelegate = std::function<…>;`).
- Enumerators are PascalCase (`EPhysicsBackend::Arcade`). UE enumerators with underscores are kept as UE
  spells them (`EKeys::Gamepad_FaceButton_Bottom`); `.clang-tidy` whitelists `Gamepad_*`.

### 1.2 Members, functions, parameters and locals

| Kind | Rule | Examples |
| --- | --- | --- |
| Member variables (public or private) | PascalCase, no `m_`, no trailing `_` | `ExitCode`, `MainWindow`, `LastFrameCycles` |
| Methods and free functions | PascalCase verbs; a function that returns a value names the value | `StartupModule()`, `PollGameDeviceState()`, `LoadLevelFile()` |
| Parameters and locals | PascalCase | `DeltaTime`, `NowCycles`, `ModuleManager` |
| Constants (`constexpr`, `static const`) | PascalCase, no `k` prefix | `MainWindowWidth`, `CurrentProtocolVersion`, `InvalidTexture`, `MaxOnScreenMessages` |
| Booleans (members, params, locals) | `b` prefix | `bDedicated`, `bCursorCaptured`, `bEnabled` |
| Macros | `UPPER_SNAKE_CASE` | `IMPLEMENT_MODULE`, `COMPILED_PLATFORM_HEADER`, `PLATFORM_DESKTOP` |

**Accessors and questions**

- Bool-returning functions ask a question: `Is…`, `Has…`, `Should…`, `Can…`
  (`IsEngineExitRequested()`, `IsGamepadConnected()`, `ShouldClose()`, `HasPath()`).
- An accessor whose natural name would collide with a member becomes `GetX()`
  (`FEnvironmentMap::GetId()` returns member `Id`; `FEngineLoop::GetExitCode()` returns `ExitCode`).

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
- Tests live in `<Module>/Private/Tests/<Topic>Tests.cpp` (Catch2) and are compiled only into
  `LeonAutomationTests`.

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
- **Standard library and math (deviation).** Until the Core containers and math land, use `std::`
  containers, `std::string`, `std::unique_ptr` / `std::function` and **glm** on desktop. PS2 code uses plain
  floats and `FPlatformMath`. Do not add aliases that pretend to be UE types (`using FVector = glm::vec3` is
  not allowed). See [NextSteps.md](UnrealEngine427/NextSteps.md).
- Use the Core fixed-width types (`int32`, `uint64`, …) from `CoreTypes.h` in engine APIs; `std::uint8_t`
  style types remain in older code.

---

## 5. Namespaces

- There is **no `namespace leon`** and no global engine namespace: types live at global scope like UE, and
  the prefixes keep them distinct.
- `Leon::<Area>` is only for free functions, constants and plain protocol structs that UE would put in a
  namespace: `Leon::Net` (NetCore protocol / snapshot codec, `Net/NetUtil.h`, `Net/RootReplication.h`),
  `Leon::InputActions` (action name constants), `Leon::PS2` (private helpers inside `PS2RHI/Private`).
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
- Known exceptions to remove: `Core/Private/Misc/Paths.cpp` and `Engine/Private/Net/NetUtil.cpp` still test
  `_WIN32`.

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
| Content kind folders | PascalCase | `Engine/Content/Materials`, `Textures`, `Hdr`, `LevelTemplates` |
| Materials | `M_<Name>.lmat` | `M_Default.lmat`, `M_WorldGrid.lmat` |
| Textures | `T_<Name>_<Suffix>` (`_D` diffuse, `_N` normal) | `T_Default_D.png` |
| Level templates / levels | PascalCase `.llev` | `Blank.llev`, `Starter.llev` |
| HDR environment maps | PascalCase, no spaces | `AutumnFieldPuresky1k.hdr` |
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
7. Tests for new behaviour live in `<Module>/Private/Tests/` and pass with `RunTests.bat`.
