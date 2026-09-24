# JoltPhysics plugin

Runtime plugin that adds a [Jolt Physics](https://github.com/jrouwe/JoltPhysics) backend to `FPhysScene`. Without it, every scene uses the built-in Arcade backend (AABB traces + character movement), and asking for Jolt falls back to Arcade with a one-time message.

## What it provides

| Piece | Location | Role |
| --- | --- | --- |
| `FJoltPhysicsModule` | `Source/JoltPhysics/Private/JoltPhysicsModule.cpp` | `StartupModule` calls `RegisterPhysicsBackendFactory(EPhysicsBackendKind::Jolt, &CreateJoltPhysicsBackend)`; `ShutdownModule` unregisters it |
| `CreateJoltPhysicsBackend()` | `Source/JoltPhysics/Public/JoltPhysicsBackend.h` | Factory returning an `FJoltPhysicsBackend` |
| `FJoltPhysicsBackend` | `Source/JoltPhysics/Private/JoltPhysicsBackend.cpp` | `IPhysicsBackend` implementation: rigid-body world (box, sphere, capsule and triangle-mesh shapes, up to 4096 bodies, single-threaded job system) and narrow-phase line / sphere / capsule traces |

`RegisterPhysicsBackendFactory` and `IPhysicsBackend` live in `Engine/Source/Runtime/PhysicsCore/Public/IPhysicsBackend.h`; the registry is implemented in the `Engine` module. Game code selects the backend with `EPhysicsBackend::Jolt` (`FPhysScene(EPhysicsBackend::Jolt)`, `UWorld::SetPhysicsBackend`, `AGameModeBase::SetPhysicsBackend`); the default is `EPhysicsBackend::Arcade`.

The module also exports the transitional define `LEON_WITH_JOLT=1` to whatever links it (the automation tests check it).

## Descriptor

`JoltPhysics.leonplugin`: version `5.3.0`, category `Physics`, `"EnabledByDefault": false`, one `Runtime` module `JoltPhysics` with `"PlatformAllowList": [ "Win64" ]`. `JoltPhysics.Build.cmake` also restricts the module to `PLATFORMS Win64` and depends on `Core`, `PhysicsCore` (public), `Engine` and `JoltLib` (private).

## Enabling it

The plugin is off by default, so a target only gets it when asked. Either:

- in a target's `*.Target.cmake`:

  ```cmake
  leon_target(MyGame TYPE Game
  	PLATFORMS Win64
  	ENABLE_PLUGINS JoltPhysics
  )
  ```

- or for every target of a project, in its `.leonproject`:

  ```json
  "Plugins": [ { "Name": "JoltPhysics", "Enabled": true } ]
  ```

Target settings override project settings, which override `EnabledByDefault` (`DISABLE_PLUGINS` / `"Enabled": false` turn it off). Program targets ignore `EnabledByDefault` and only get plugins they enable. On platforms outside the allow-list (PS2, Linux) the module is skipped even when the plugin is enabled.

Currently only `LeonAutomationTests` enables it. `LeonGame` and `LeonCook` do not, and `Game/ThirdPerson` (PS2) cannot use it.

## JoltLib (ThirdParty)

`Source/ThirdParty/JoltLib/JoltLib.Build.cmake` is an external module for Jolt Physics **5.3.0** (Win64 only). LeonBuildTool downloads the release archive once, checks it against a pinned SHA-256, and extracts it to `Source/ThirdParty/JoltLib/JoltPhysics-5.3.0/` (git-ignored):

- URL: `https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v5.3.0.tar.gz`
- SHA-256: `e7f9621e480646c434150e1fbe3a9410f4ec4b04ffe54791e0678326b741b918`

`Setup.bat` / `Setup.sh` download it together with every other pinned dependency; a build that needs it downloads it on demand. The archive is cached in `Engine/Intermediate/ThirdPartyDownloads/`. The build uses Jolt's own CMake project (`Build/`) with its tests, samples, viewer and install rules turned off and links the `Jolt` library target. Jolt is MIT-licensed (`JoltPhysics-5.3.0/LICENSE`).

## Tests

`Source/JoltPhysics/Private/Tests/JoltPhysicsTests.cpp` runs in `LeonAutomationTests`, tagged `[jolt]`: backend name, `UWorld::SetPhysicsBackend`, gravity and resting on the floor / a static box / a triangle mesh, and line and sphere traces. Without `LEON_WITH_JOLT` the file instead checks that a Jolt request falls back to Arcade.

```bat
Engine\Build\BatchFiles\RunTests.bat "[jolt]"
```
