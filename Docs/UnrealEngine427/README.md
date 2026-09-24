# Unreal Engine 4.27 knowledge base

LeonEngine mirrors the **Unreal Engine 4.27** source layout, module architecture and coding standard, built with
CMake through **LeonBuildTool** (our UnrealBuildTool). The reference version is **pinned to 4.27** — do not mix in
UE5 layouts unless a page here says so explicitly.

Local reference checkout (source only, `Setup.bat` not run — no `Engine/Content`, no binaries):

```
C:\Users\DanielBarreto\Desktop\Code\danielbarretoes\UnrealEngine
```

Use these pages instead of walking the checkout. Open the checkout only to read a specific file listed here.

| Page | What it answers |
| --- | --- |
| [SourceLayout.md](SourceLayout.md) | Where things live in UE 4.27: `Engine/Source` categories, module anatomy, platform folders, Launch, RHI, input, plugins, config, templates |
| [KeyHeaders.md](KeyHeaders.md) | Exact paths of the headers we mirror (Actor, Character, SpringArm, World, DynamicRHI, LaunchEngineLoop, …) |
| [LeonMapping.md](LeonMapping.md) | LeonEngine module ↔ UE module, Leon type ↔ UE type, and every intentional deviation |
| [NextSteps.md](NextSteps.md) | What is not mirrored yet (Core containers, CoreUObject reflection, config reader, …) — input for the next plan |

Coding standard: [Docs/CODING_STANDARD.md](../CODING_STANDARD.md) (Epic's standard + Leon deviations).
