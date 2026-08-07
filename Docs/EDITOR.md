# Leon Editor — panels & authoring

**Audience:** level / content authors  
**Also:** [ASSET_FORMATS.md](ASSET_FORMATS.md) · [LEVELS.md](LEVELS.md) · [SETUP.md](SETUP.md)

Day-to-day build: `Scripts\build-fast.bat` → `Editor\build-fast\LeonEngine.exe`.

IDE / clangd setup (compile DB, false red squiggles): [SETUP.md — IDE / clangd](SETUP.md#ide--clangd-cursor).

---

## Layout (Unreal-like)

ImGui **docking**: drag tab titles to split / re-dock. **Window** menu shows or hides panels; closed panels can be reopened from there.

Typical workspace:

| Panel | Role |
| --- | --- |
| Viewport | Perspective + orthographic views, gizmos, PIE |
| Content Browser | Assets under project / `Engine/Assets` |
| Outliner | Scene hierarchy / selection |
| Details | Selected actor / component properties |
| World Settings | Per-level defaults (GameMode Override, environment) + **Post Process** (project preference in `leon.game.json`) |
| Project Settings | Pack-wide `leon.game.json` options (Display Name, **Game Default Map**, Default GameMode, Post Process) — Edit → Project Settings |
| Output Log | Persistent log stream |
| Console | Log + command line (`` ` ``) |
| Material Editor | One dockable tab per open `.lmat` |

---

## Console

Open with **backtick** (`` ` ``) or **Window → Console**.

| Command | Effect |
| --- | --- |
| `help` | List commands |
| `clear` | Clear console buffer |
| `openmat <path>` | Open Material Editor for a `.lmat` |
| `focus` | Focus selection in viewport |
| `stat` | Toggle / print simple stats |

Messages also appear in **Output Log**. Engine / cook / import errors should show up in both when routed through `EditorOutputLog`.

---

## Material Editor

- **Open:** double-click `.lmat` in Content Browser, Details → **Edit Material…**, or `openmat`.
- **UI:** parameters (BaseColor, Metallic, Roughness, …) + texture paths; live preview when GL context allows.
- **Save:** writes `.lmat` and invalidates the material cache so viewports refresh.
- Multiple materials → multiple dockable tabs (same pattern as Unreal asset editors). Dirty tabs show `*` in the title.

New blank materials: Content Browser → **New Material…** → `Materials/M_*.lmat`.

### Save / Save All (Unreal-like)

| Action | Shortcut | Behavior |
| --- | --- | --- |
| **Save Asset** | Ctrl+S | Focused dirty Material Editor tab, else dirty current level |
| **Save All** | Ctrl+Shift+S | All dirty `.lmat` docs + dirty level |

Also available from **File**, Content Browser toolbar, and right-click (**Save Asset** when dirty; empty area **Save All**). Unsaved dialogs cover materials + level.

---

## Content Browser

- Filters by extension (meshes, materials, anims, …).
- Dirty levels/materials show `*` next to the name.
- Toolbar: **Save** / **Save All**; asset RMB: **Save Asset**, **Rename** (F2), **Delete** (Del).
- **Move:** drag an asset onto a Content folder (same as Unreal Content Browser). Name collisions are blocked.
- Rename / Move / Delete run **Fix Up References** immediately (open level + pack `.llev` / `.lmat` soft paths). No redirector assets.
- **Delete Assets** dialog shows reference count; **Force Delete** clears soft refs when referenced.
- Engine content (`leon:Engine/…`) is read-only (Rename / Delete / Move disabled).
- **New Folder…**, pickers for mesh / material assignment.
- Import entry points live under **File → Import…** (see [ASSET_FORMATS.md](ASSET_FORMATS.md#import--cook)).

Supported content extensions match the cheat sheet in ASSET_FORMATS (`.lmat`, `.lmesh`, `.lskel`, `.lskm`, `.lanim`, textures, levels).

**Not yet:** Fix Up for `.lskm` / `.lchar` / blendspaces; Duplicate; Reference Viewer.

---

## Viewport & editing

| Feature | Notes |
| --- | --- |
| Multi-select | Outliner / viewport; stable `editorId` |
| Transform | ImGuizmo; grid / snap; **End** cycles modes where wired |
| Ortho views | Top / Front / Side style viewports |
| Show Grid | View → Show Grid (also World Settings). World-aligned; follows the camera (not pinned at origin) |
| View Mode | Unreal-like **Lit** (default) / **Player Collision** (collision wireframes only). View → View Mode, Alt+5 / Alt+6 |
| Undo / Redo | `EditorHistory` |
| Dup / Copy / Paste / Delete | `EditorCommands` (paste preserves `meshPath` when applicable) |
| Build Lights | Bakes `.lm` beside the level |
| Build Paths | Preview-only NavMesh bake from level collision (`NavigationSystem`) — **not saved to disk**. Use F3 in PIE/shipping for debug draw |
| Place Actors | Cube/Sphere/Plane/**BlockingVolume**/**StaticMesh…** + **PlayerStart**, **TriggerVolume**, **PainCausingVolume**, **AISpawnPoint** |
| Volume Details | Trigger: cost / radius / payload / consume; Pain: DPS / interval; AISpawn: transform + tag |
| PIE | N=1: in-process pack Runtime (`GameHostSession` + `RegisterModes`) on the **open level** — same GameModes / travel / HUD as Shipping. `Default` → free-look. Unknown projects → `PieGameMode` preview. N>1 Listen/Client → Shipping processes. Selected Viewport forces Lit during play |
| Play settings | Unreal-like **Number of Players** (1–4) + **Net Mode**. **N>1 Listen/Client:** exactly **N Shipping** windows with `--map <current level>` (e.g. Courtyard → `coop-tp` match, skip Lobby). Host: `--listen --map X`; clients: `--join 127.0.0.1 --map X`. Editor stays editable. **N=1:** editor Pie as before. Stop kills Shipping PIDs |
| Stats overlay | Toolbar / View → Show Stats — FPS + GPU pass ms (Shadow / Planar / Color / AO / Post) |

Save warns if StaticMesh tags still use legacy Zombies strings (`Door:…`, `Lava`) — prefer typed TriggerVolume / PainCausingVolume (see [LEVELS.md](LEVELS.md)).

### Post process / quality

Post Process settings in **World Settings** are a **project preference** stored under `editorPostProcess` in `leon.game.json` (not per-level). Opening a project applies them to the editor renderer; changing Combo/Checkbox/DragFloat values writes the file.

Forward renderer post stack (when enabled): HDR scene color → half-res SSAO + blur → ACES tonemap (× World Exposure) → FXAA → present.

| Quality | Typical settings |
| --- | --- |
| Off | No post; shadow map 1024 |
| Low (default) | SSAO 8 samples, no FXAA, shadow 1024 |
| Medium | SSAO 16 samples, FXAA, shadow 2048 |
| High | SSAO 32 samples, FXAA, Early-Z, shadow 2048 |

Tune AO radius / intensity / bias in **World Settings → Post Process**. Runtime AO multiplies lit color (complements lightmaps; lower intensity if double-darkening).

---

## Levels

- New Level from **level templates** (`Engine/Assets/LevelTemplates/`).
- Save / load binary `.llev` levels + optional lightmaps — [LEVELS.md](LEVELS.md).
- Project hub: New From Template uses **project** templates under `Templates/` (Blank, ThirdPerson).

---

## Related code

| Area | Path |
| --- | --- |
| Dock layout | `Editor/Application/EditorLayout.cpp` |
| Console | `Editor/Panels/ConsolePanel.*`, `EditorOutputLog` |
| Material Editor | `Editor/Panels/MaterialEditorPanel.*` |
| Content Browser | `Editor/Panels/ContentBrowser*` |
| History / commands | `EditorHistory`, `EditorCommands` |
