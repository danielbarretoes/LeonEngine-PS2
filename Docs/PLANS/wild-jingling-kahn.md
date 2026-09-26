# Core, CoreUObject, formatos UE (.lproj/.lplugin/.ini/.lasset/.lmap/.lpak) y juego objetivo ShooterGame (clon offline de Counter-Strike)

## Contexto

El refactor a la estructura de UE 4.27 ya está hecho (rama `refactor/ue-structure`, versión 0.11.0). Lo que falta para que el motor sea UE "de verdad":

**Core**
- Hoy es un esqueleto HAL: typedefs, PlatformMemory/Time/Math, FModuleManager, FTicker, FPaths/FFileHelper sobre std::filesystem y un FTransform sobre glm.
- No hay contenedores, strings, logging, asserts, config, archivos ni FName.
- El motor usa ~1.180 `glm::`, ~750 `std::string`, ~300 `std::vector`, ~250 `cout`, nlohmann y 12 `dynamic_cast`. En PS2 no hay RTTI ni excepciones.

**CoreUObject**
- No existe: no hay reflexión, ni UClass, ni GC, ni paquetes.
- Los prefijos U/A son solo nombres.

**Formatos**
- Son propios y dispersos: `.leonproject/.leonplugin`, `.llev`, `.lmesh`, `.lmat`, `.lskel/.lskm/.lanim/.lchar`, `leon.game.json`, `.lm`, y PNG/WAV cargados en runtime.
- Tampoco hay rutas virtuales (`/Game`, `/Engine`) ni empaquetado.

**HDR**
- `Engine/Content/Hdr/AutumnFieldPuresky1k.hdr` y el feature de environment map (skybox + IBL) nunca llegan a cargarse: el nivel pide otra ruta.

**Juego**
- En Win64, un módulo de juego no tiene forma de registrarse: `RegisterModes` es una lambda vacía.
- Al framework le falta lo básico de un FPS: equipos, rondas, trazas contra personajes, crouch/aceleración, armas y bots con visión.

**Objetivo del plan**
- Core nativo estilo UE con migración total a convención UE Z-up en centímetros.
- CoreUObject con reflexión (LeonHeaderTool, homólogo de UHT).
- Config jerárquica, UE_LOG, paquetes `.lasset`/`.lmap`, `.lpak`, y los descriptores renombrados a `.lproj`/`.lplugin`.
- Eliminar los HDR y el feature de environment map.
- Sobre esa base, el juego objetivo **Game/ShooterGame**: clon offline de Counter-Strike, Win64 primero y PS2 después.
- Lightmaps e iluminación estática quedan para después.

**Decisiones del usuario**
- ShooterGame corre en Win64 primero; PS2 es un hito posterior.
- Core nativo con migración total y convención Z-up en cm.
- LeonHeaderTool en C++. Para PS2 se añade g++ al contenedor Docker.
- Se quitan los HDR y el feature completo.
- Los mapas se hacen en Blender, se exportan a glTF y un commandlet los importa a `.lmap`.
- Primer hito: el bucle CS completo en 1 mapa.
- Todos los formatos legados se reemplazan por `.lasset`/`.lmap`.
- El networking se retira, con un tag git de archivo para recuperarlo.

## Decisiones de diseño

| # | Decisión |
|---|---|
| D1 | `TCHAR = ANSICHAR` (UTF-8) en todas las plataformas; `TEXT(x)` es `x`. Los wide strings solo existen dentro del HAL de Windows. Desviación documentada, pensada para el EE. |
| D2 | La API pública de Core no expone tipos `std::`. Core puede usar internamente `<type_traits>/<new>/<cstring>/<cstdio>/<cmath>`. Prohibidos `<iostream>/<string>/<functional>/<memory>/<sstream>` (el tamaño del ELF de PS2 importa). |
| D3 | Contenedores UE: `TArray` (int32 Num/Max, allocator policies con Inline/Fixed), `TSparseArray`, `TSet`, `TMap = TSet<TPair>`, `TArrayView`. Sin excepciones: un fallo de reserva dispara `check`. |
| D4 | `FString` con semántica UE: `==` sin distinguir mayúsculas. En la migración se revisan las comparaciones que deben distinguirlas. |
| D5 | **FName** estilo UE: sin distinguir mayúsculas, sufijo numérico, 8 bytes, entradas permanentes. Pool por constantes de plataforma. PS2: bloques de 16 KB con máximo 256 KB y 4.096 buckets. Desktop: bloques de 64 KB. Un desbordamiento es error fatal y queda registrado. |
| D6 | Matemática solo en float: `FMatrix` row-major (`v*M`), subida a GLSL sin transponer. `-Werror=double-promotion` en PS2. |
| D7 | Ejes UE: X adelante, Y derecha, Z arriba, levógiro, 1 unidad = 1 cm. Conversores únicos por fuente de datos: `FLegacyCoordinateConversion` (Y-up metros → UE, temporal) y `FImportCoordinateConversion` (glTF/FBX → UE). Jolt sigue en metros y convierte en su frontera. |
| D8 | Capas de config UE: `Engine/Config/Base<T>.ini` → `Engine/Platforms/<P>/Config/<P><T>.ini` → `<Proj>/Config/Default<T>.ini` → `<Proj>/Platforms/<P>/Config/<P><T>.ini` → `<Proj>/Saved/Config/<Plat>/<T>.ini` (solo desktop). Operadores `+ - . !`. Tipos Engine/Game/Input/Editor. |
| D9 | Todo el acceso a archivos pasa por `IPlatformFile`/`FPlatformFileManager`. Backends: Win32, POSIX y PS2 (newlib sobre `host:`/`cdrom0:`/`mass:`). Encima, `FPakPlatformFile`. |
| D10 | LeonHeaderTool: programa C++17 que solo usa std. Se compila en un árbol de herramientas de host (`Engine/Intermediate/Build/HostTools/<Host>/`) antes de configurar cualquier target; en Docker el host es Alpine con g++. |
| D11 | GC UE4 mark-and-sweep stop-the-world sobre FProperty, con raíces y `FGCObject`. Solo corre en puntos seguros: LoadMap, reinicio de ronda y temporizador. |
| D12 | Los subobjetos por defecto se construyen de nuevo por instancia y luego se aplican las propiedades cargadas (sin instanciar arquetipos). Desviación documentada. |
| D13 | Paquete `.lasset` (y `.lmap` con `PKG_ContainsMap`): un solo archivo (sin `.uexp`) con summary `'LEON'`, tablas de nombres/imports/exports, propiedades etiquetadas como delta contra el CDO y bulk data al final. Guardado determinista (tabla de nombres ordenada, GUID derivado del nombre, sin timestamps). |
| D14 | Win64 fuera de Shipping usa `WITH_EDITORONLY_DATA=1` y carga `Content/*.lasset` sin cocinar. PS2 y Shipping solo cargan paquetes cocinados. |
| D15 | El importador de mapas crea únicamente clases del motor. El sentido de juego va en `PlayerStartTag` y `AActor::Tags` (`BombSite`+`A`, `BuyZone`+`CT`), así el motor sigue sin conocer el juego. |
| D16 | Los volúmenes (`ATriggerVolume`, `ABlockingVolume`) son cajas, no BSP. Desviación documentada. |
| D17 | Sin RTTI ni excepciones también en Win64 para los módulos Leon, una vez eliminados `dynamic_cast` y `try/catch`, igual que los valores por defecto de UE. |
| D18 | Arranque del juego: target con `WITH_ENGINE=1`, `GEngine` según `[/Script/Engine.Engine] GameEngine=`, mapa de `GameDefaultMap`, y game mode con precedencia `?game=` → `AWorldSettings::DefaultGameMode` → `GlobalDefaultGameMode` (TSubclassOf). El módulo de juego solo declara `IMPLEMENT_PRIMARY_GAME_MODULE` y sus clases se encuentran por reflexión. |

**Se retira**
- Environment map y HDR (decisión B).
- Loader `.lm` de lightmaps: vuelve como `<Map>_BuiltData.lasset` en la fase de iluminación estática.
- **Networking**: NetCore, UNetDriver, ENet y el flujo host/join, con tag `archive/net-enet-0.11`. El modo headless se conserva.
- nlohmann en runtime (se reemplaza por un módulo Json nativo con API UE).
- `LevelCatalog`, `LevelDirector`, `WorldRuntime`, `GameHostSession`, `GameplayRouter` y los packs `leon.game.json`, sustituidos por `UEngine::LoadMap` y el game mode por config.
- `ContentValidator` (la validación pasa a import, cook y un commandlet de validación).
- Formatos `.lchar/.lskel/.lskm/.lanim` y blend space JSON. El importador FBX se conserva y se mueve a Developer.
- `UInputMappingContext`, que es un concepto de UE5; se sustituye por `UInputSettings` + `UInputComponent` de 4.27.
- NavMesh de rejilla de un solo piso, sustituido por un grafo de waypoints.

**Se mantienen como puente temporal**
- Los lectores `.llev`, `.lmesh`, `.lmat` y PNG, hasta que el pipeline nuevo los reemplace (fases P14-P15). Son el smoke visual de Win64 durante la migración.

## Puertas de verificación (cada fase termina con build + tests + commit)

- **G1 Win64:** `Lint.bat` (formato y build de LeonAutomationTests/LeonCook/LeonGame/BlankProgram) y `RunTests.bat` en verde.
- **G2 PS2:** ThirdPerson y BlankProgram compilan en Docker. PCSX2 a 60 FPS con Draw3D `boxes=343 tris=278 emit=254`.
- **G3 (desde P2):** presupuesto PS2. Tamaño del ELF, `.bss` y pico de heap registrados en `Engine/Platforms/PS2/Documentation/Budgets.md`.
- **G4 (desde P6):** `CheckBannedApis.ps1`, llamado desde Lint y la CI. Rechaza `glm::`, `nlohmann`, `std::vector|string|map|function|shared_ptr|unique_ptr`, `iostream` y `printf` fuera de ThirdParty, las internas del HAL, LeonHeaderTool y los mains de test.
- **G5 (desde P14):** reimport reproducible. `LeonCook -run=ImportAssets -reimport -all` seguido de `git diff --exit-code -- */Content` no deja diferencias.
- **G6 (desde P17):** smoke headless `ShooterGame -nullrhi` con código de salida 0.

## Fases

| Fase | Contenido | Tamaño | Hito |
|---|---|---|---|
| P0 | `.lproj` / `.lplugin` | S | 0.11.1 |
| P1 | Poda: HDR/env map, lightmaps, net, capa de packs/sesión, formatos esqueléticos | M | 0.12.0 |
| P2 | Core base: asserts, UE_LOG, FMemory, contenedores, FString/FName/FText mínimo, delegates, TestPAL | L | |
| P3 | Core math (FVector/FRotator/FQuat/FMatrix/FTransform/FMath...) | M | |
| P4 | IPlatformFile, FPaths, FArchive, FConfigCacheIni/GConfig, FCommandLine/FParse, CRC/GUID/MD5, Json nativo, Projects `.lproj` | L | 0.13.0 |
| P5 | Migración de módulos bajos a tipos UE (misma semántica Y-up/metros) | L | |
| P6 | Migración de Renderer/Engine/AI/Developer/Jolt; fuera glm, nlohmann, try/catch y C++20; G4 | XL | |
| P7 | Cambio a Z-up en centímetros | L | 0.14.0 |
| P8 | LeonHeaderTool + integración en LeonBuildTool (puede ir en paralelo con P5-P7) | M | |
| P9 | CoreUObject: UObject/UClass/UStruct/UEnum/UFunction, FProperty, NewObject, Cast, CDO | L | |
| P10 | GC, punteros weak/soft, TSubclassOf, `UPROPERTY(Config)`/LoadConfig, `UFUNCTION(Exec)` | M | |
| P11 | UPackage, linker y formato `.lasset` | L | 0.15.0 |
| P12 | Framework de gameplay como UObjects (`Cast<>` en lugar de `dynamic_cast`; RTTI y excepciones off) | L/XL | |
| P13 | Reestructura de Engine: niveles como actores, frontera FScene/IRendererModule (fin del ciclo Engine↔Renderer), UEngine::LoadMap, input UE 4.27 por config | XL | 0.16.0 |
| P14 | Clases de asset + módulo editor `LeonEd` (factories, commandlets de import) + migración del contenido | L/XL | |
| P15 | `.lmap` + importador de mapas glTF; se borra `.llev` | L | |
| P16 | Cook, `.lpak`, LeonPak, staging, BuildCookRun | M | 0.17.0 |
| P17 | ShooterGame: arranque y FPS básico | L | |
| P18 | Armas, daño, view model, efectos | L | |
| P19 | Rondas, economía, menú de compra, bomba, HUD | L | |
| P20 | Bots y navegación por waypoints | L | |
| P21 | Partidas de bots automatizadas, presupuestos, docs | M | 0.20.0 |

### P0 · `.lproj` / `.lplugin` (S)
- `git mv` de `Game/ThirdPerson/ThirdPerson.leonproject` → `.lproj` y de `Engine/Plugins/Runtime/JoltPhysics/JoltPhysics.leonplugin` → `.lplugin`.
- LeonBuildTool: `System/ProjectDescriptor.cmake`, `System/PluginDescriptor.cmake` (glob `*.lplugin`), `CMakeLists.txt` y `LeonBuildTool.cmake`. Pasar un `.leonproject` da un error explícito: "renamed to .lproj".
- También se actualizan:
  - las líneas de uso de BatchFiles `.bat`/`.sh`;
  - el filtro de `RunPCSX2.ps1`;
  - `ci.yml`, `.vscode/settings.json` y `.editorconfig`;
  - el comentario de `ModuleManager.h` y toda la documentación.
- Se mantienen los nombres de campo de `.uproject`/`.uplugin`.

### P1 · Poda (M, commits por área)
- **Env map:** se borran:
  - `Renderer/{Public,Private}/EnvironmentMap.*`, `Engine/Shaders/skybox.*` y `Engine/Content/Hdr/`;
  - `FResourceCache::LoadEnvMap`, el pase de skybox y los uniforms de entorno de `SceneRenderer.cpp`;
  - los samplers cubemap de `blinn_phong.frag`, que se queda con `fakeEnvironment`;
  - los campos de entorno de `ULevel`, el factor de exposición del nivel (`SceneRenderer.cpp`) y las comprobaciones HDR de `ContentValidator`.

  El lector `.llev` salta los campos de entorno y el escritor escribe un placeholder, así que la versión del formato no cambia.
- **Lightmaps:** se borran `LightmapIO.*`, sus campos y el camino `uLightmap`.
- **Net:** primero el tag `archive/net-enet-0.11`. Después se borran NetCore, ThirdParty/ENet, `NetDriver.h`, `Engine/{Public,Private}/Net`, los tests de net, la lógica host/join de `UGameInstance` y los flags de CLI.
- **Sesión/packs:** se borran `LevelCatalog`, `LevelDirector`, `WorldRuntime`, `GameHostSession`, `GameplayRouter`, `LevelAnimation`, la resolución de packs y `ContentValidator`, junto con sus tests. `FGameApplication` carga `-map=<.llev>` (por defecto `Starter.llev`) y activa `ADefaultGameMode`.
- **Esqueletal:** la importación FBX esquelética pasa de AnimationCore/Engine a `Developer/MeshUtilities/Private/FbxSkeletalImport.cpp`. Se borran los formatos `CookedSkeletal`, los modos `character` y `anim` de LeonCook y la dependencia UFBX de AnimationCore.
- Se borran también `EditorId` y el código muerto (ArenaCamera si no se usa).
- Se actualiza la documentación.
- Se guarda, fuera del repo, una captura de referencia de `LeonGame -map=Starter.llev`.

### P2 · Core base (L)
Todo en `Engine/Source/Runtime/Core/Public`:
- `CoreTypes` + `TCHAR`/`TEXT`, `Misc/CoreMiscDefines.h` (`INDEX_NONE`).
- `Misc/AssertionMacros.h`: check/checkf/verify/ensure.
- `Logging/{LogMacros,LogCategory,LogVerbosity}.h`, `Misc/OutputDevice*.h` con `GLog`. Cada plataforma tiene su dispositivo de consola (Windows: stdout + OutputDebugString; PS2: printf a la consola del EE).
- `HAL/{UnrealMemory,MemoryBase,MallocAnsi}.h`. El malloc de PS2 cuenta uso y pico para `FStatsOverlay`.
- `Templates/*` (UnrealTemplate, TypeHash, Function, UniquePtr, SharedPointer, Tuple), `Misc/Optional.h` y `Algo/*`.
- `Containers/*` (Array, ArrayView, BitArray, SparseArray, Set, Map, UnrealString, StringConv). `Ticker.h` pasa a apoyarse en TArray y un delegate.
- `UObject/NameTypes.h` + `UnrealNames.inl`, `Misc/{CString,Char}.h`.
- `Internationalization/Text.h`: FText mínimo (FromString, AsNumber, Format y LOCTEXT literal).
- `Delegates/*`: TDelegate/TMulticastDelegate (BindStatic/Lambda/Raw/SP).
- Nuevo programa `Engine/Source/Programs/TestPAL` (homólogo de UE, sin Catch2) que ejecuta los self-tests de Core en Win64 y en PS2 (el resultado se lee en el log del EE).
- Canario: ThirdPerson pasa de `printf` a `UE_LOG(LogThirdPerson, …)`.
- Probar `-ffunction-sections -fdata-sections --gc-sections` en PS2 y medir el efecto.

### P3 · Core math (M)
- El FTransform actual (glm) pasa a llamarse `FLegacyTransform`, solo desktop, con renombrado clang-tidy.
- `Math/*`: FMath, FVector/2D/4, FIntPoint/IntVector, FRotator, FQuat, FMatrix y derivadas (Rotation/Translation/Scale/Perspective/Ortho), FPlane, FBox, FSphere, FBoxSphereBounds, FTransform nativo, FColor/FLinearColor y FRandomStream.
- `FBox`/`FPlane` de RenderCore se sustituyen por los de Core.
- Puentes temporales, solo desktop: `Migration/GlmInterop.h` (ToGlm/FromGlm explícitos) y `Migration/LegacyAxes.h`. Hasta P7 está prohibido usar `FVector::UpVector` y similares.
- `Transform.cpp` deja de estar excluido en PS2.
- Tests con valores de referencia contra glm y los mismos valores en TestPAL sobre PS2.

### P4 · Servicios de plataforma de Core (L)
- `IPlatformFile`/`IFileHandle`/`FPlatformFileManager` + backends Windows, Linux y PS2 (`Engine/Platforms/PS2/Source/Runtime/Core/Private/PS2PlatformFile.cpp`, de solo lectura, base dir tomado de `argv[0]`).
- `IFileManager`, `FArchive`/`FMemoryReader`/`FMemoryWriter`/`FBufferArchive`.
- `FPaths` reescrito sobre FString con la API UE (EngineDir, ProjectDir, ProjectContentDir, ProjectSavedDir, Combine...) y sin std::filesystem. `ResolveAssetPath` sobrevive solo como `ResolveLegacyContentPath` hasta P15.
- `FFileHelper` (Load/Save Array/String, escritura atómica), `FCommandLine`, `FParse`, `FApp`, `FCrc`, `FGuid`, `FMD5`, `FDateTime` mínimo, `FOutputDeviceFile` (`<Proj>/Saved/Logs`).
- `FConfigCacheIni`/`GConfig`/`GEngineIni`… con las capas de D8.
- Orden de `FEngineLoop::PreInit`: CommandLine → PlatformFile → Paths → Config → Log → módulos.
- Módulo **Json** nativo (Dom JsonObject/JsonValue, JsonReader/Writer/Serializer) sin excepciones.
- Módulo **Projects**: `FProjectDescriptor` (`.lproj`) y `FPluginDescriptor` (`.lplugin`).
- Canario PS2: ThirdPerson lee sus parámetros de `DefaultGame.ini` por `host:`. Si PCSX2 no tiene HostFs, usa los valores compilados y se comporta igual. Queda documentado en `RunPCSX2.ps1`.

### P5 · Migración de los módulos bajos (L; un commit por módulo)
- Orden: ApplicationCore, InputCore, RHI, OpenGLDrv, Launch (partes compartidas), PhysicsCore, RenderCore, AnimationCore, AudioMixer, SlateCore, UMG.
- Equivalencias:

  | Antes | Después |
  |---|---|
  | glm | F-math, sin cambiar el sentido Y-up/metros |
  | std::vector | TArray |
  | std::string | FString (texto/rutas) o FName (identificadores) |
  | unordered_map | TMap |
  | std::function | TFunction / delegates |
  | smart pointers | TUniquePtr / TSharedPtr |
  | cout/printf | UE_LOG con categorías |

- Las fronteras con módulos aún sin migrar usan los puentes de interop.
- Renombrados UE baratos en este punto: `FCapsuleShape` → `FCollisionShape`, campos de `FHitResult` y `UTextBlock::SetText(FText)`.
- El número de tests no cambia. Cualquier fallo se trata como bug de la migración.

### P6 · Migración de los módulos altos (XL)
- Orden: Renderer; Engine por áreas (Components/GameFramework, física, cámara/input, lectores legados sobre FArchive/FConfigFile, GameEngine/HUD); después AIModule, MeshUtilities, Cooker, LeonCook, Jolt y Launch desktop.
- Se borran ThirdParty GLM y NlohmannJson, `Migration/*`, `FLegacyTransform` y todos los try/catch.
- Lo que era C++20 pasa a C++17 (`std::span` → TArrayView, `std::numbers` → PI, `starts_with` → StartsWith). `CXX_STANDARD 17` en los módulos de Engine.
- Entra G4 (`CheckBannedApis.ps1`).
- Se compara visualmente Starter.llev con la captura de P1.

### P7 · Z-up en centímetros (L)
- **Antes:** tests "golden" de trayectorias en Y-up metros: CMC al caminar, saltar, subir escalones y en rampas; trazas; brazo de cámara.
- **Cambio:**
  - `FLegacyCoordinateConversion` en los lectores `.llev`/`.lmesh`.
  - CMC: gravedad en −Z, FindFloor, alturas en cm.
  - Física: `QuerySupportZ`.
  - `FRotator ControlRotation`.
  - Nav en el plano XY.
  - Listener de audio.
  - Renderer: matriz de vista UE, proyección GL en Renderer, una única decisión de `glFrontFace`, sombras, plano de reflexión, frustum y debug draw.
  - Import con D7 y Jolt escalado ×0.01.
- **Después:** los mismos escenarios deben coincidir con `Convert(golden)` dentro de tolerancia, más un gizmo de ejes y pruebas manuales de WASD y ratón.

### P8 · LeonHeaderTool (M; en paralelo con P5-P7)
- `Engine/Source/Programs/LeonHeaderTool/` con su propio `CMakeLists.txt`: Tokenizer, HeaderParser, TypeModel, CodeGenerator, Manifest. Tests golden con `-Test`.
- Subconjunto UE que reconoce: UCLASS/USTRUCT/UENUM(+UMETA)/UPROPERTY/UFUNCTION/GENERATED_BODY y `#if WITH_EDITORONLY_DATA`.
  - Especificadores que interpreta: Config, Transient, Abstract, `Config=`, Exec. El resto (`meta`, BlueprintType…) se acepta y se ignora.
  - Tipos de propiedad: primitivos, FString/FName/FText, enums/TEnumAsByte, UObject*, TSubclassOf, TSoftObjectPtr/TSoftClassPtr/TWeakObjectPtr, TArray/TMap/TSet, USTRUCTs y arrays fijos.
  - Errores con formato `file(line): error:`. Solo escribe un archivo si su contenido cambia.
- LeonBuildTool:
  - `System/HostTools.cmake` compila el árbol de herramientas de host antes de configurar y pasa `-DLEON_HEADER_TOOL=`.
  - `Configuration/ReflectionRules.cmake`: un módulo es reflejado si incluye `*.generated.h`. Genera un `.lhtmanifest` y un `add_custom_command` con stamp, y el include público `<tree>/Inc/<Module>`.
  - Salidas: `Inc/<Module>/<Header>.generated.h`, `<Header>.gen.cpp` y `<Module>.init.gen.cpp`.
  - `_leon_write_module_init` añade un puntero `RegisterReflection` a `FStaticallyLinkedModuleInfo` (`ModuleManager.h`), para que el enlace estático no descarte el registro.
- `DockerEntry.sh`, el Dockerfile y la CI instalan `g++ musl-dev`.

### P9 · CoreUObject (L)
Nuevo módulo `Engine/Source/Runtime/CoreUObject` (todas las plataformas, C++17):
- `ObjectMacros.h` (GENERATED_BODY al estilo UE), UObjectBase(Utility), Object.
- `Class.h`: UField/UStruct/UScriptStruct/UClass/UEnum/UFunction.
- `Field.h` y `UnrealType.h` (FProperty y todos sus tipos).
- `UObjectGlobals.h`: NewObject, FObjectInitializer, CreateDefaultSubobject, Find/StaticFindObject, MakeUniqueObjectName.
- UObjectArray/Hash/Iterator, Package, `NoExportTypes.h` (FVector, FRotator, FTransform, FColor, FGuid…), `Templates/{Casts,SubclassOf}.h`.
- Registro en orden de dependencia: RegisterReflection → paquetes `/Script/<Module>` → offsets de propiedades → CDOs → `StartupModule`.
- `GUObjectArray`: 8.192 objetos en PS2 (~96 KB) y 131.072 en desktop. Presupuesto de reflexión en PS2 ≤ 400 KB.
- LeonHeaderTool procesa también `Private/Tests/*.h` para los targets de tests. TestPAL incluye CoreUObject.

### P10 · GC y referencias (M)
- `GarbageCollection.h`, `GCObject.h`, WeakObjectPtr, SoftObjectPath/Ptr, StrongObjectPtr, AddToRoot, BeginDestroy/FinishDestroy.
- `LoadConfig`/`SaveConfig`/`ReloadConfig` para `UCLASS(Config=…)` y `UPROPERTY(Config)`, con sección `/Script/<Module>.<Class>`, arrays `+X=` y structs en texto.
- `UFUNCTION(Exec)` con thunks generados y `CallFunctionByNameWithArguments`.
- Delegates `BindUObject`/`AddUObject`.

### P11 · Paquetes (L)
- `PackageFileSummary`, `ObjectResource` (FObjectImport/Export, FPackageIndex), Linker/LinkerLoad/LinkerSave, SavePackage, PropertyTag, `FByteBulkData`.
- `Misc/PackageName.h`: puntos de montaje `/Engine/` → `EngineContentDir` y `/Game/` → `ProjectContentDir`, `/Script/<Module>`, extensiones `.lasset`/`.lmap`.
- LoadPackage/LoadObject/StaticLoadObject/CreatePackage. `ELeonPackageVersion` en `Core/Public/UObject/ObjectVersion.h`. Carga síncrona.
- Layout del archivo:
  - Summary `'LEON'`: versión, flags (ContainsMap/Cooked/FilterEditorOnly), offsets y cuentas de nombres/imports/exports/soft refs, GUID, versión del motor, plataforma de cocinado y offset del bulk.
  - Tablas.
  - Datos de cada export: propiedades etiquetadas con terminador `None` y luego `Serialize` nativo.
  - Bulk data al final.
- Tests: referencias duras y soft, evolución de esquema, import ausente, guardado determinista y TestPAL sobre archivos en memoria.

### P12 · Framework como UObjects (L/XL)
- El POD de `Level.h` pasa a llamarse `FLevelStaticMesh`.
- Pasan a UCLASS:
  - AActor, UActorComponent y USceneComponent (Relative*, ComponentToWorld, Attach con socket).
  - Componentes nuevos: UPrimitiveComponent y UCapsule/Box/Sphere/UStaticMeshComponent (real).
  - Componentes existentes: USkeletalMeshComponent, UCameraComponent, USpringArmComponent y UCharacterMovementComponent (con bases UMovement/UPawnMovementComponent).
  - Clases de framework: APawn, ACharacter, AController (ControlRotation), APlayerController, AAIController, AGameModeBase + AGameMode (MatchState), AGameStateBase/AGameState, APlayerState, AHUD, UGameInstance, UWorld, ULevel (`TArray<AActor*>` como UPROPERTY) y UPlayerInput.
- `UWorld::SpawnActor(Class, Loc, Rot, FActorSpawnParameters)` con NewObject. `Destroy` marca el actor y el GC lo recoge.
- `Cast<>` sustituye los 12 `dynamic_cast`. RTTI y excepciones off en Win64 (D17).

### P13 · Reestructura de Engine (XL)
1. **Niveles como actores:** AStaticMeshActor, APlayerStart (PlayerStartTag), ATriggerVolume/ABlockingVolume/APainCausingVolume (caja), ADirectionalLight/APointLight, ATargetPoint y AWorldSettings (DefaultGameMode, KillZ). El lector `.llev` genera actores.
2. **Frontera de render:**
   - En Engine: `SceneInterface.h`, `PrimitiveSceneProxy.h`, `RendererInterface.h` (IRendererModule), `SceneView.h` y `CanvasTypes.h`.
   - Renderer implementa FRendererModule y FScene, y tiene su propia caché de GPU.
   - **Desaparece el ciclo Engine↔Renderer.** UMG pinta con FCanvas.
3. **Motor:**
   - UEngine/UGameEngine (`UCLASS(Config=Engine)`, `GEngine`), UGameViewportClient (input y comandos Exec de consola).
   - `UEngine::Browse(FURL)`/`LoadMap` y la precedencia de game mode de D18.
   - Nuevo módulo EngineSettings: UGameMapsSettings y UGeneralProjectSettings.
4. **Input 4.27:**
   - UInputSettings (Config=Input) con Action/AxisMappings.
   - `FKey` como USTRUCT en InputCore, que pasa a depender de CoreUObject.
   - UInputComponent BindAction/BindAxis, `SetupPlayerInputComponent` y ejes MouseX/MouseY.
   - `BaseInput.ini` se carga de verdad.
5. **Launch:**
   - PreInit crea la aplicación, la ventana y el RHI en desktop y en PS2 (`RHIInit()`).
   - Init crea `GEngine` a partir de la config.
   - Se borran FGameApplication y RuntimeInput. LeonGame queda como el homólogo de UE4Game (`-project=`).
6. **Primer uso real de UObject en PS2:** ThirdPerson arranca UObject a través de InputCore. Se vigilan G2 y G3.

### P14 · Assets e importación (L/XL)
- **Clases de asset en Engine:**
  - UTexture2D, UStaticMesh (con UBodySetup), UMaterialInterface/UMaterial (modelos de sombreado fijos con parámetros y texturas; sin grafo de nodos, desviación documentada).
  - USkeleton con sockets, USkeletalMesh, UAnimSequence (pistas por hueso), UBlendSpace1D, USoundWave (PCM16), UDataAsset y UCommandlet.
- `FResourceCache` desaparece:
  - los assets se cargan con LoadObject/TSoftObjectPtr y `PostLoad` los sube a la caché del renderer;
  - los defaults del motor salen de la config (`DefaultMaterialName=/Engine/EngineMaterials/M_Default.M_Default`, sonidos de UI).
- **Nuevo módulo editor `Engine/Source/Editor/LeonEd`** (tipo Editor, solo desktop):
  - Factories: UTextureFactory (stb se mueve aquí), UStaticMesh/USkeletalMesh/UAnimSequenceFactory (glTF/FBX/OBJ vía MeshUtilities) y USoundFactory.
  - UAssetImportData (ruta de origen, MD5, ajustes).
  - Commandlets: ImportAssets, ResavePackages, ValidateAssets y MigrateLegacyContent (temporal). `UCookCommandlet` se mueve aquí y `Developer/Cooker` desaparece.
  - LeonCook pasa a invocarse como `LeonCook <Proj>.lproj -run=<Commandlet>`.
- **Importación sin editor:**
  - Un asset: `-run=ImportAssets -source=SourceArt/... -dest=/Game/...`.
  - En lote: `-importlist=SourceArt/ImportList.ini`.
  - Reimportar todo: `-reimport -all`.
  - Nombres con prefijos UE: SM_, SK_, SKEL_, A_, BS_, T_, M_, S_.
  - El arte fuente del motor vive en `Engine/SourceArt/`.
- **Migración de contenido:**
  - `T_Default_D.png` → `/Engine/EngineMaterials/T_Default_D`.
  - Los 3 `.lmat` → `M_*.lasset`.
  - Los WAV de UI se recuperan de `ad7e00e:Projects/Zombies/Content` si su licencia lo permite.
  - Después se borran LeonMeshFormat, LeonMaterialFormat, la carga de PNG/WAV en runtime y STB en runtime.
- `.gitattributes`: `*.lasset *.lmap *.lpak binary`.

### P15 · `.lmap` e importación de mapas glTF (L)
- `.lmap`: paquete con UWorld, `ULevel PersistentLevel`, AWorldSettings, actores y componentes, y `PKG_ContainsMap`. `LoadMap("/Game/Maps/X")`.
- `UGLTFMapFactory`: `-run=ImportAssets -type=Map -source=SourceArt/Maps/de_leon.glb -dest=/Game/Maps/de_leon`. Las reglas van en `DefaultEditor.ini [/Script/LeonEd.MapImportSettings]`.
- Convenciones de nombres en Blender:

  | Nodo | Resultado |
  |---|---|
  | Malla | AStaticMeshActor. Las mallas compartidas se convierten en un único `SM_` en `/Game/Maps/<Map>/Meshes` |
  | `UCX_*` | Colisión convexa |
  | `COL_*` | Solo colisión, invisible |
  | `Clip_*` | ABlockingVolume |
  | `PlayerStart_CT` / `PlayerStart_T` | APlayerStart con PlayerStartTag |
  | `BombSite_A` / `BombSite_B` | ATriggerVolume, Tags [BombSite, A] |
  | `BuyZone_CT` / `BuyZone_T` | ATriggerVolume, Tags [BuyZone, CT] |
  | `NavWaypoint*` | ANavigationWaypoint (extras `links`/`flags`) |
  | `KHR_lights_punctual` | Luces |
  | Material PBR | UMaterial + texturas |

  Unidades ×100 y ejes según D7. El proyecto valida que existan los tags requeridos.
- Migración: `Blank.llev` → `/Engine/Maps/Entry.lmap` y `Starter.llev` → `/Engine/Maps/Template_Default.lmap`. **Después se borran** LeonLevelFormat, LevelLoader, la conversión legada, `ResolveLegacyContentPath` y `LevelTemplates/`.
- `Engine/SourceArt/Maps/AxisTest.glb` sirve para detectar espejados.
- Tests con un glTF de fixture.

### P16 · Cook, `.lpak` y staging (M)
- Nuevo módulo Runtime **PakFile**: `IPlatformFilePak.h` (FPakPlatformFile, FPakFile, FPakEntry y pie FPakInfo con magic `'LPAK'`).
  - Índice ordenado por hash.
  - Se montan los `<Proj>/Content/Paks/*.lpak`.
  - Alineación opcional a 2048 para CD.
  - Sin compresión ni cifrado por ahora.
- **`Programs/LeonPak`** (homólogo de UnrealPak): `-create=<resp>`, `-list`, `-extract`, `-test`, `-align=`.
- `-run=Cook -TargetPlatform=Win64|PS2`:
  - Sigue el cierre de dependencias (imports y soft refs) más `DirectoriesToAlwaysCook`.
  - Escribe en `<Proj>/Saved/Cooked/<Plat>/`.
  - Usa `Developer/TargetPlatform` (Win64 identidad, PS2 stub).
  - Incluye ini y shaders en el staging.
- **`Engine/Build/BatchFiles/BuildCookRun.bat`** (`-build -cook -stage -pak`) deja el resultado en `<Proj>/Saved/StagedBuilds/Win64/`. Shipping solo lee del pak.

### P17 · ShooterGame: arranque y FPS básico (L)
- Estructura de `Game/ShooterGame/`:
  - `ShooterGame.lproj`.
  - `Config/Default{Engine,Game,Input,Editor}.ini`, con `GameDefaultMap=/Game/Maps/de_leon` y `GlobalDefaultGameMode=/Script/ShooterGame.ShooterGameMode`.
  - `Source/ShooterGame.Target.cmake` (Game, Win64), `Source/ShooterGame/ShooterGame.Build.cmake` y `Source/ShooterGameTests.Target.cmake`.
  - `SourceArt/`: `de_leon.blend/.glb`, `ImportList.ini` y `LICENSES.md` (solo CC0).
  - `Content/`.
- En el motor, genérico:
  - Cámara en primera persona (`bUsePawnControlRotation`) y sensibilidad del ratón por config.
  - CMC: MaxAcceleration, GroundFriction y BrakingDeceleration; crouch con comprobación de uncrouch; modificador de andar; air control; hook `GetMaxSpeed`.
  - Canales de colisión con respuestas. Las cápsulas de los pawns y los componentes de forma pasan a ser cuerpos consultables en FPhysScene, así que las trazas ya golpean personajes.
- Juego: AShooterGameMode (spawns por equipo), AShooterCharacter, AShooterPlayerController y AShooterHUD (retícula).
- Primer blockout de `de_leon`: 2 sitios de bomba, spawns, zonas de compra y waypoints.
- Entra G6: `ShooterGame -nullrhi -ExecCmds=…` carga el mapa, crea 10 pawns y sale con 0.

### P18 · Armas y daño (L)
- AShooterWeapon (slot, munición, parámetros con `UPROPERTY(Config)`).
- Instant (hitscan): spread según movimiento y salto, retroceso con FRandomStream, cadencia, recarga, caída con la distancia, multiplicador de headshot y penetración de armadura. Cubre pistola y rifle.
- AWP: zoom por FOV, overlay de mira y penalización al moverse.
- Proyectil + granada HE: `UProjectileMovementComponent` en el motor y daño radial con línea de visión.
- API de daño UE: `TakeDamage(FDamageEvent, AController*, AActor*)` con `UDamageType`.
- Al morir: arma soltable y `ASpectatorPawn`.
- Motor:
  - Pase de view model (limpieza de depth, FOV propio).
  - Muzzle flash con luz breve, pool de 64 impactos tipo decal y trazadores.
  - `GetSocketTransform` y `PlaySoundAtLocation`.
- Arte: T y CT a partir del Bot FBX recuperado (`ee1cdde^:Templates/ThirdPerson`), armas y sonidos CC0.
- Tests: daño/armadura, headshots, spread determinista.

### P19 · Reglas, economía, bomba y HUD (L)
- **`AShooterGameMode : AGameMode`**
  - Máquina de estados: Freeze → Live → RoundEnd → siguiente ronda o fin de partida.
  - Valores por config: tiempos, bomba 40 s, desactivar 10 s (5 s con kit), dinero inicial 800 y máximo 16.000, recompensas con racha de derrotas, bonus por plantar y premio por kill según arma.
  - Relleno de equipos 5v5 con bots. Los supervivientes conservan sus armas. La bomba se asigna a un T al azar.
- **Estado:** AShooterGameState (marcador, ronda, tiempo, estado de la bomba, kill feed) y AShooterPlayerState (equipo, dinero, K/D).
- **Compra:** zonas por tag, equipo y tiempo, más un menú UMG (pistola, rifle, AWP, HE, kevlar + casco).
- **AShooterBomb:** llevar, soltar y recoger. Plantar quieto 3 s dentro de un BombSite. Pitido, desactivación y explosión.
- **HUD:** vida, armadura, dinero, munición, tiempo, marcador, bomba, mensajes, retícula dinámica, scoreboard con Tab y kill feed.
- **Exec:** `mp_restartgame`, `bot_add_t/ct`, `bot_kick`, `give` y `god`.
- Tests deterministas de rondas, economía, plantado/desactivación y matriz de victorias.

### P20 · Bots (L)
- **Motor:**
  - ANavigationWaypoint + UWaypointNavigationData, autoenlazados en el import (sweeps de cápsula, escalón y salto) y guardados en el `.lmap`.
  - A* en `FindPathToLocationSynchronously` y PathFollowing-lite.
  - Se borra el NavMesh de rejilla.
- **AIModule:** `UPawnSensingComponent` (vista con línea de visión, oído con `MakeNoise`) y blackboard con claves FName tipadas.
- **Juego:** `AShooterAIController` con un behavior tree en C++:
  - Comprar, ir al objetivo (el T portador planta, los demás escoltan; los CT se reparten o rotan), enfrentarse, plantar, desactivar, cazar.
  - Enfrentamiento con retardo de reacción, error de puntería que decae y control de ráfagas.
  - Dificultad por config.
- **Tests:** A* y autoenlazado sobre el mapa de fixture, decisiones de bots y partida headless con seed fija de 3 rondas con invariantes.

### P21 · Endurecimiento (M)
- CI Windows: `ShooterGame -nullrhi -botmatch -rounds=10 -seed=N` con comprobación de invariantes.
- Captura de objetos, uso del name pool, bytes de reflexión y pico de heap en `Budgets.md`, como objetivo para PS2.
- Build Shipping staged con BuildCookRun.
- Documentación: ARCHITECTURE, BUILD, TOOLS, ASSET_FORMATS, LEVELS, CODING_STANDARD, LeonMapping (tablas y desviaciones), NextSteps, `Game/ShooterGame/README.md` y CHANGELOG.

## ThirdPerson (PS2) durante todo el plan

ThirdPerson mantiene sus tipos `F*` hasta el port de Engine. Cuándo le afecta cada fase:

| Fase | Qué cambia |
|---|---|
| P0 | `.lproj` |
| P2 | UE_LOG |
| P3 | Matemática en PS2 |
| P4 | Config por `host:`, con los valores compilados como respaldo |
| P8-P9 | LeonHeaderTool en Docker, CoreUObject vía TestPAL |
| P13 | Arranque UObject real |

Tras cada fase: build, PCSX2, estadísticas Draw3D idénticas y 60 FPS.

## Riesgos y mitigaciones

| Riesgo | Mitigación |
|---|---|
| Migración de ~33K líneas | Commits por módulo, puentes de interop temporales, el número de tests no cambia, G1 y G2 en cada commit |
| Cambios semánticos (FString `==` sin mayúsculas, orden de TMap, int32/INDEX_NONE) | Lista de revisión por módulo; los tests dependientes del orden usan claves ordenadas |
| Ejes, lateralidad y espejados | Golden de trayectorias, un conversor por fuente, gizmo, mapa AxisTest, una sola decisión de `glFrontFace` |
| Fragilidad del parser de LeonHeaderTool | Solo el subconjunto UE, tests golden, errores claros |
| Tamaño y RAM en PS2 | Sin iostream, gc-sections, presupuestos G3, pools fijos con errores de desbordamiento claros, metadatos de reflexión recortados |
| Registro descartado por el enlace estático | Tabla de inicialización generada y explícita |
| Punteros colgantes con GC | Regla UPROPERTY, FGCObject, GC solo en puntos seguros |
| Formatos cambiantes sin editor | Versionado, deltas etiquetadas, ResavePackages, reimport reproducible (G5) |
| PCSX2 sin HostFs | Valores compilados como respaldo; TestPAL en memoria |
| Crecimiento del repo por binarios | Texturas ≤ 256², `.gitattributes`, LFS como opción futura |
| Alcance de ShooterGame | Criterios de "hecho" por fase |
| Licencias de arte | Solo CC0, `LICENSES.md` |

**Diferido del primer hito:** penetración de paredes, flash y humo, cuchillo, escaleras, cambio de lado, radio y physics assets por hueso.

## Hitos posteriores (fuera de este plan)

- **Port de Engine a PS2:**
  - Quitar `PLATFORMS Desktop`.
  - Renderer PS2 sobre FPS2RHI e IRendererModule: VU1, clipping, texturas CLUT, 4 MB de VRAM, skinning.
  - HUD en el GS, audio audsrv/SPU2, cook PS2 (PSMT8/4, paquetes LPS2 v2, ADPCM), pak en `cdrom0:` alineado a 2048 más ISO/SYSTEM.CNF, `PS2Input.ini`.
  - Presupuesto de 32 MB y timestep fijo.
  - ThirdPerson → `AThirdPersonCharacter`.
- **Iluminación estática y lightmaps** (`<Map>_BuiltData.lasset` + baker).
- **Editor OpenGL.**
- **Replicación UObject** (UNetDriver/UActorChannel/FRepLayout).
- **Otros:** carga asíncrona, compresión de pak, material instances, partículas, cache binaria de config, Linux verificado.

## Flujo de trabajo

Igual que en el refactor anterior:
- Rama `feature/core-uobject-shootergame` creada desde `refactor/ue-structure`.
- Commits por fase o grupo, terminados en `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.
- `Docs/UNREAL_ENGINE/` queda fuera de `git add`.
- `Docs/PS2OFFICIAL` no se toca.
- Merge y push solo si el usuario lo pide.
- Los tags `archive/*` se crean localmente y no se publican sin permiso.
