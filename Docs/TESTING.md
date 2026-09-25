# Testing

What runs automatically and what a person still has to check by hand. Build and test commands:
[SETUP.md](SETUP.md#run-the-tests); coding rules for tests: [CODING_STANDARD.md](CODING_STANDARD.md).

## Automated

| Check | Command | Passes when |
| --- | --- | --- |
| Automation tests (Win64) | `Engine\Build\BatchFiles\RunTests.bat [-automation=<filter>]` | `Automation: N test(s), N passed, 0 failed` |
| LeonHeaderTool golden tests (run by `RunTests.bat` too) | `Engine\Intermediate\Build\HostTools\Win64\LeonHeaderTool.exe -Test` | `LeonHeaderTool -Test: N of N golden cases passed` |
| Core, CoreUObject, Json and Projects on PS2 | `Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build` | `TestPAL: PASSED (73 test(s), 0 failed)` in the EE log (77 on Win64) |
| Format, banned APIs (G4), Win64 build | `Engine\Build\BatchFiles\Lint.bat` | `Lint OK` |
| Frame capture | `LeonGame.exe "-map=<.llev>" "-Screenshot=<file.bmp>" "-ExitAfterFrames=N"` | the BMP matches a reference capture byte for byte |

The golden tests (`System.Engine.Golden.*`, `System.AIModule.Golden.*`, `System.JoltPhysics.Golden.*`) replay
movement, traces, navigation, cameras, shadows and reflections against tables recorded before P7 moved the world to
UE's axes, so any change of sign or unit fails them.

`-Screenshot=<file.bmp>` saves frame `-ExitAfterFrames=N` (default 60) as a 24-bit BMP and exits. `-AxesGizmo` turns
the axes gizmo on from the start (see below); captures without it do not change.

## Axes gizmo

**F6** in `LeonGame` (or `-AxesGizmo` on the command line) draws two things over the frame, X red, Y green and Z
blue as in UE:

- 1 m axes at the world origin, drawn in front of the scene;
- a 96-pixel gizmo in the bottom-left corner showing the view orientation: the world axes through the camera
  rotation only, projected orthographically. An axis pointing into the screen shrinks to a dot; the axis nearest the
  viewer is drawn last.

`FDebugDraw::AddAxes(Origin or FTransform, Length = 100)` and `FDebugDraw::AddViewAxes(View)` (Renderer,
`Public/Debug/DebugDraw.h`) draw them; `FSceneRenderer::SetAxesGizmoEnabled` switches the gizmo. It is off by default.

## Manual checklist: axes and units (P7)

Run `Engine\Binaries\Win64\LeonGame.exe -AxesGizmo` (the Starter level, free-look camera; the cursor is captured,
close the window to quit). The world is X forward, Y right, Z up, left-handed, 1 unit = 1 cm.

- [ ] **Gizmo colours**: red, green and blue lines leave the world origin; blue points straight up. In the corner
  gizmo blue points up the screen whenever the view is level.
- [ ] **Handedness**: look straight down (pull the mouse towards you until the pitch stops at -89°) and turn until red
  points up the screen: green must point to the **right**. With green on the left the world is right-handed.
- [ ] **Mouse X**: moving the mouse right turns the view right (clockwise seen from above; the yaw grows). The view
  turns about 0.3° per pixel: the default game mode and the engine's free look both apply the same 0.15° per pixel,
  as before P7.
- [ ] **Mouse Y**: moving the mouse away from you looks up (positive pitch); the pitch stops at ±89°.
- [ ] **WASD**: turn until red points into the screen (the corner red line shrinks to a dot). **W** moves towards
  +X (the origin axes come closer if you are behind them), **S** away, **D** to the right (+Y), **A** to the left.
  The movement follows the view, including its pitch.
- [ ] **Q / E**: **E** moves up (+Z, the floor drops away), **Q** down.
- [ ] **Shadows**: shadows fall away from the sun and stay attached to the objects that cast them; **F1** draws the
  shadow volume (yellow) around the level, not beside it.
- [ ] **Mirror**: on a level with a `PlanarMirror=true` material ([ASSET_FORMATS.md](ASSET_FORMATS.md)), objects above
  the mirror appear upside down under them, with left and right kept (a red object on the left of a blue one stays on
  the left in the reflection).
- [ ] **Free-look camera**: the view never rolls while turning or looking up and down, and the Starter level opens
  with the same view as before P7 (the frame captures match).
- [ ] **Orbit camera**: `LeonGame` always enters the default game mode (free look), so the orbit camera cannot be
  driven there. Its mapping (`(-Pitch, Yaw + 180, 0)` from the legacy angles) is covered by
  `System.Engine.Golden.OrbitCameraNdc` and the camera tests. The PS2 ThirdPerson orbit boom (right stick) keeps its
  own frame and did not change in P7.
- [ ] **Jump**: `LeonGame` has no character. The jump (+Z at `JumpZVelocity`, 700 cm/s) is covered by
  `System.Engine.Golden.JumpArc`. On PS2, Cross still jumps up in ThirdPerson.
- [ ] **Sound panning**: the audio device converts the listener and sound positions (Y and Z swap, ×0.01 to metres),
  but no level or game mode plays a positioned sound yet, so there is nothing to listen to. When one does, a sound on
  +Y of a listener looking along +X must come from the **right** speaker.
