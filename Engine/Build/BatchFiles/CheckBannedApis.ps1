# Engine\Build\BatchFiles\CheckBannedApis.ps1 - fails when engine or game code uses an API that Core replaces (gate G4).
#
# Banned (comments are ignored; names are case-sensitive):
#   glm and nlohmann                         -> Core math, the Json module
#   the std containers, strings, streams, functions and smart pointers, and their headers (D2: <string>, <functional>,
#   <memory>, <sstream>, <vector>, ...)       -> TArray, TSet, TMap, FString, TFunction, TSharedPtr, TUniquePtr
#   <iostream>, std::cout, std::cerr          -> UE_LOG
#   printf and its variants                  -> UE_LOG / FString::Printf / FCString
#   LegacyGL, FLegacyTransform, LegacyAxes   -> removed in P7 (UE view and projection, FTransform, UE axes); tests too
#   the PS2's immediate path (FPS2RHI::DrawBox / BindMaterial / DrawDebugText / DrawUnlitRectAlpha, FPS2Texture,
#   FPS2Material, FPS2ViewTarget, FPS2DirectionalLight, FPS2Draw3DStats, math3d), the engine-less launch
#   (PlatformEngineLoopHooks, COMPILE_AGAINST_ENGINE) and
#   FStatsOverlay                            -> removed in ps2-shipping N2: FGSCommandList, FGSDebugDraw, UEngine's stats
#   the hand-timed frame parts (FFrameTimeClock, FEngineFrameSplit, FViewport::FDrawTimes, FGameViewportDrawTimes, the
#   PS2's present times)                     -> removed in ps2-shipping N9: SCOPE_CYCLE_COUNTER, FCycleStatsWindow
#   USoundWave::GetPCMView                   -> removed in ps2-shipping N20 (it copied a sound's samples each time it
#                                               played): USoundWave::LockPCM / UnlockPCM read them in place
#   the float static mesh render data (FStaticMeshLODResources' Vertices / Indices / Sections) and
#   TransformStaticMesh                      -> removed in ps2-shipping N12: LPS2 v2 (FLPS2Mesh), the collision triangles
#   the polled vertical blank (graph_wait_vsync and libgraph's other CSR polls, graph_initialize, LastFlipSeconds)
#                                            -> removed in ps2-shipping N10: FPS2VerticalBlank, FGSFieldPacer
#   the synchronous frame packet (FlushFrame, dma_channel_send_normal, dma_wait_fast, libpacket's packet_t)
#                                            -> removed in ps2-shipping N11: FGSGifPacket::BuildChain, the frame's
#                                               double-buffered DMA chain
#   FGSTextureCache::GetNumUploads / ResetStats (the cache that started over when full)
#                                            -> removed in ps2-shipping N13: FGSTextureCache::BeginFrame, GetFrameCounters
#   a sound's PCM played (USoundWave::GetPCMView, removed in ps2-shipping N20; LockPCM / UnlockPCM, FSoundWavePCM,
#   the procedural UI tones and audsrv's PCM stream, removed in N19)
#                                            -> the sound's SPU2 ADPCM buffer (USoundWave::GetSoundBuffer,
#                                               FAudioDevice::AcquireSoundBuffer): the SPU2's voices play it on the PS2,
#                                               the desktop decodes and mixes it
#   FMallocAnsi and MallocAnsi.h             -> removed in ps2-shipping N17: FMallocBinned (HAL/MallocBinned.h) is GMalloc
#                                               on every platform
#   FBX and OBJ import (UFbxFactory, EFBXImportType, the Fbx* and Obj* importers, ufbx, tinyobjloader), the right-handed
#   Z-up import basis, and the model-space animation (FSkeletalVertex, FSkeletalMeshData, matrix keys,
#   GetBoneWorldMatrices / GetBoneModelMatrix, SetRawAnimationData / GetRawAnimationData, BuildFromImportData)
#                                            -> removed in ps2-shipping N21: glTF is the only mesh and animation format
#                                               (UGLTFImportFactory, LoadSkeletalMeshFromGltf, LoadAnimSequencesFromGltf);
#                                               FSkinWeightInfo, compressed local tracks (FCompressedAnimSequence),
#                                               UAnimInstance::EvaluatePose and the component's cached pose
#   the two-clip locomotion player (UBlendSpace1D::Evaluate, UAnimInstance::GetBlendAlpha)
#                                            -> removed in ps2-shipping N25: UBlendSpaceBase::GetSamplesFromBlendInput
#                                               (FBlendSampleData, 1D and 2D), UAnimInstance::GetLocomotionSamples
#   FGSCommandList::GetImageData (the lists' copied images, a texture's CLUT made at upload)
#                                            -> removed in ps2-shipping N23: GetImage / GetNumImages, the cooked texture
#                                               blob uploaded in place (UploadImageInPlace)
#   the frame on the GIF channel (DMA_CHANNEL_GIF, PATH3) and libpacket2's VIF helpers
#                                            -> removed in ps2-shipping N14: the frame's VIF1 chain (DIRECT, VU1)
#   ShooterGame's AShooterWeapon_Pistol / _Rifle / _Sniper, EShooterHitGroup::Body and GetBuyMenuItems
#                                            -> removed in ps2-shipping N30a: CS 1.6's weapons (AShooterWeapon_Glock,
#                                               _USP, _Deagle, _MP5, _AK47, _M4A1, _AWP, _Knife), CS's hit groups, the
#                                               buy menu's pages (GetBuyMenuEntries)
#   ShooterGame's AShooterWeapon_Grenade   -> removed in ps2-shipping N30b: AShooterWeapon_HEGrenade, _Flashbang,
#                                               _SmokeGrenade
#   ShooterGame's team-named bots (Bot_CT_1, NextBotNumber) and the buy menu's own number keys (OnBuyMenuItem1..7,
#   BuyMenuInputComponent)                   -> removed in ps2-shipping N30e: CS's BotProfile names
#                                               (AShooterGameMode::BotNames, GetBotName) with the stream seeded by the
#                                               bot's index (SetBotIndex); the menus' input component (MenuInputComponent,
#                                               OnMenuItem1..9) shared with the radio menus
#   ShooterGame's surface table (EShooterSurface, FShooterSurfaceMaterial, SurfaceMaterials) and its one footstep
#   (FootstepSoundName, S_Footstep)          -> removed in ps2-shipping N30f: the physical materials (UPhysicalMaterial,
#                                               EPhysicalSurface, FHitResult::PhysMaterial, SHOOTER_SURFACE_*), the
#                                               surfaces' sounds (FootstepSounds, FShooterSurfaceSounds)
#   FCanvas::GetTriangles, FScene::GatherStaticMeshes / GatherSkeletalMeshes
#                                            -> removed in ps2-shipping N15: FCanvas::GetPrimitives (the HUD's rectangles
#                                               as SPRITEs), FScene::GatherPrimitives (cells and portals)
#   the single-pad input (FPS2InputInterface::Get, GetRawButtonMask, GetRawSticks) and the IOP's modules loaded by
#   hand (SifLoadModule of rom0:SIO2MAN / PADMAN / MCMAN / MCSERV)
#                                            -> ps2-shipping N24: IInputInterface's controller ids, the pressure axes and the
#                                               force feedback; FPlatformMisc::LoadIopModule (once each)
#   the skinned batches posed on the EE (bQuantizedPose, AllocatePosedStreams, QuantizePose, SkinBatch, PosedPositions /
#   PosedNormals)                            -> removed in ps2-shipping N14b: VU1's Skinned programs with the palette
#   ShooterGame's LoadOptionalAsset copies   -> removed in ps2-shipping N24b: LoadShooterObject / LoadShooterAsset
#   FLegacyCoordinateConversion and LegacyCoordinateConversion.h
#                                            -> UE-space data; only tests convert legacy (Y up, metres) data, the
#                                               golden tables: see $TestsOnly
#
# Allowed where Core wraps the C and C++ libraries (D2): ThirdParty, Core's platform HAL sources (Core/Private/Windows,
# the PS2 Core), the printf family inside Core/Private, LeonHeaderTool (a std-only host tool), the
# PS2's leonrun (a host tool built inside Play!'s tree, Build/PlayRunner) and the test program mains.
#
# A violation prints "<file>:<line>: G4 <rule>: <code> -> <what to use>". -Root checks another tree (default: the repo).
param([string] $Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") {
	$Root = Join-Path $PSScriptRoot "..\..\.."
}
$Root = (Resolve-Path $Root).Path.TrimEnd('\', '/')

# The legacy world lives in tests only since the legacy levels went (P15): FLegacyCoordinateConversion itself
# (RenderCore's Public/Tests and Private/Tests), the golden adapters (Engine's Public/Tests/LegacyGolden.h) and the tests
# that compare against legacy-space tables. No runtime or editor source may use it.
$TestsOnly = '[\\/](Public|Private)[\\/]Tests[\\/]'

$Rules = @(
	@{ Name = "glm"; Pattern = 'glm::|<glm/'; Use = "Core math" },
	@{ Name = "nlohmann"; Pattern = 'nlohmann'; Use = "the Json module" },
	@{ Name = "std container / string / function / smart pointer"
		Pattern = 'std::(vector|string|map|unordered_map|set|unordered_set|list|deque|array|function|shared_ptr|unique_ptr|string_view|stringstream|istringstream|ostringstream)\b'
		Use = "TArray, TSet, TMap, FString, TFunction, TSharedPtr, TUniquePtr" },
	@{ Name = "std header (D2)"
		Pattern = '#\s*include\s*<(string|functional|memory|sstream|vector|map|unordered_map|set|unordered_set|list|deque|array|string_view)>'
		Use = "the Core headers (Containers/, Templates/); Core wraps the few C++ headers it needs" },
	@{ Name = "iostream"; Pattern = '<iostream>|std::(cout|cerr|clog)\b'; Use = "UE_LOG" },
	@{ Name = "printf"; Pattern = '(?<![A-Za-z_])(f|s|sn|v|vs|vsn|vf|_sn|_vsn)?printf\s*\('; AllowedIn = '[\\/]Runtime[\\/]Core[\\/]Private[\\/]'
		Use = "UE_LOG, FString::Printf or FCString" },
	@{ Name = "the PS2 immediate path and the engine-less launch (ps2-shipping N2)"
		Pattern = 'FPS2RHI::(DrawBox|BindMaterial|SetViewTarget|SetDirectionalLight|SetAmbientLightColor|DrawDebugText|DrawUnlitRectAlpha|ScreenVertex)\b|\b(FPS2Texture|FPS2Material|FPS2ViewTarget|FPS2DirectionalLight|FPS2Draw3DStats|PlatformEngineLoopHooks|FStatsOverlay|COMPILE_AGAINST_ENGINE)\b|<math3d\.h>'
		Use = "an FGSCommandList (the Renderer, FGSDebugDraw) submitted to FPS2RHI; games compile against the engine" },
	@{ Name = "the hand-timed frame parts (ps2-shipping N9)"
		Pattern = '\b(FFrameTimeClock|FEngineFrameSplit|FDrawTimes|FGameViewportDrawTimes|GetLastDrawTimes|bLogPresentTimes)\b'
		Use = "SCOPE_CYCLE_COUNTER (Stats/Stats.h) and FCycleStatsWindow" },
	@{ Name = "a sound's samples copied to play it (ps2-shipping N20)"; Pattern = '\bGetPCMView\b'
		Use = "USoundWave::LockPCM / UnlockPCM (the audio device copies what it plays)" },
	@{ Name = "the float static mesh render data (ps2-shipping N12)"
		Pattern = 'LODResources(\(\))?\s*\.\s*(Vertices|Indices|Sections)\b|\bTransformStaticMesh\b'
		Use = "LPS2 v2 (FStaticMeshLODResources::RenderData) or UStaticMesh::GetPhysicsTriMeshData" },
	@{ Name = "the polled vertical blank (ps2-shipping N10)"
		Pattern = '\b(graph_wait_vsync|graph_check_vsync|graph_start_vsync|graph_initialize|LastFlipSeconds)\b'
		Use = "FPS2VerticalBlank (the INTC_VBLANK_S handler's field count and semaphore) and FGSFieldPacer" },
	@{ Name = "the synchronous copied frame packet (ps2-shipping N11)"
		Pattern = '\b(FlushFrame|dma_channel_send_normal|dma_channel_send_normal_ucab|dma_wait_fast|packet_init|packet_free|packet_t)\b|<packet\.h>'
		Use = "FGSGifPacket::BuildChain into the frame's double-buffered DMA chain (BuildFrameChain, KickFrameChain)" },
	@{ Name = "the texture cache that started over when full (ps2-shipping N13)"
		Pattern = '\bGetNumUploads\b|TextureCache\(\)\s*\.\s*ResetStats\b|TextureCache\.ResetStats\b'
		Use = "FGSTextureCache::BeginFrame and GetFrameCounters (LRU residency by blocks)" },
	@{ Name = "a sound's PCM played (ps2-shipping N20, N19)"
		Pattern = '\b(GetPCMView|LockPCM|UnlockPCM|FSoundWavePCM|BuildUiTone)\b|\baudsrv_(play_audio|set_format|wait_audio|available|queued)\b'
		Use = "the sound's SPU2 ADPCM buffer (USoundWave::GetSoundBuffer, FAudioDevice::AcquireSoundBuffer)" },
	@{ Name = "the copied upload images (ps2-shipping N23)"; Pattern = '\bGetImageData\b'
		Use = "FGSCommandList::GetImage / GetNumImages; a cooked texture uploads in place (UploadImageInPlace)" },
	@{ Name = "the C runtime allocator as GMalloc (ps2-shipping N17)"; Pattern = '\bFMallocAnsi\b|MallocAnsi\.h'
		Use = "FMemory (GMalloc is FMallocBinned, HAL/MallocBinned.h), FMemStack for a frame's temporaries" },
	@{ Name = "FBX / OBJ import and the model-space animation (ps2-shipping N21)"
		Pattern = '\b(UFbxFactory|FbxFactory|EFBXImportType|FBXIT_\w+|FFbxImportCommon|LoadStaticMeshFromFbx|LoadSkeletalMeshFromFbx|LoadAnimSequenceFromFbx|BuildFromFbx|BuildFromObj|LoadObj|LoadObjSourceSpace|RightHandedZUp|FSkeletalVertex|FSkeletalMeshData|EmbeddedAnim|GetBoneWorldMatrices|GetBoneModelMatrix|SetRawAnimationData|GetRawAnimationData|BuildFromImportData|ConvertSkeletalMeshData|ConvertAnimSequence|ufbx_\w+|tinyobj)\b|<ufbx\.h>|<tiny_obj_loader\.h>'
		Use = "glTF (UGLTFImportFactory, LoadSkeletalMeshFromGltf, LoadAnimSequencesFromGltf), FSkinWeightInfo, FCompressedAnimSequence, UAnimInstance::EvaluatePose, USkeletalMeshComponent::GetComponentSpaceTransforms" },
	@{ Name = "the two-clip locomotion player (ps2-shipping N25)"
		Pattern = '\bGetBlendAlpha\b|(BlendSpace|Bs|Space)\s*(\.|->)\s*Evaluate\s*\('
		Use = "UBlendSpaceBase::GetSamplesFromBlendInput (FBlendSampleData), UAnimInstance::GetLocomotionSamples" },
	@{ Name = "the actor loop, tick-counted timers and the variable step (ps2-shipping N18)"
		Pattern = '->bCanEverTick\b|^\s*bCanEverTick\s*=|\b(LifeSpanRemaining|TickPooledPointLights|PooledPointLightEndTimes|TimeUntilPain|TimeUntilNextUpdate|ElapsedRemainder|NextHeadlessTick|TickHz)\b'
		Use = "PrimaryActorTick / PrimaryComponentTick (FTickFunction), the world's FTimerManager, FFixedStepClock" },
	@{ Name = "ShooterGame's box bodies and N25's placeholder names (ps2-shipping N27)"
		Pattern = '\b(CTBodySkeletalMeshName|TBodySkeletalMeshName|ArmsMeshName)\b|\bBodyMesh\s*(->|=)'
		Use = "the skinned team body (ACharacter::GetMesh, CTBodyMeshName / TBodyMeshName) and arms (CTArmsMeshName / TArmsMeshName)" },
	@{ Name = "the frame on the GIF channel (PATH3) and libpacket2's VIF helpers (ps2-shipping N14)"
		Pattern = '\bDMA_CHANNEL_GIF\b|\bpacket2_(vif|utils_vu)_\w+|<packet2(_vif|_utils)?\.h>'
		Use = "the frame's VIF1 chain (FGSGifPacket::BuildChain with FPS2VU1BatchEncoder: DIRECT, and VU1 for the batches)" },
	@{ Name = "ShooterGame's generic weapons, the capsule's two hit groups and the flat buy menu (ps2-shipping N30a)"
		Pattern = '\bAShooterWeapon_(Pistol|Rifle|Sniper)\b|ShooterWeapon_Sniper\.h|\bEShooterHitGroup::Body\b|\bGetBuyMenuItems\b'
		Use = "CS 1.6's weapons (AShooterWeapon_Glock, _USP, _Deagle, _MP5, _AK47, _M4A1, _AWP, _Knife), EShooterHitGroup's CS groups, AShooterPlayerController::GetBuyMenuEntries" },
	@{ Name = "ShooterGame's one grenade (ps2-shipping N30b)"; Pattern = '\bAShooterWeapon_Grenade\b'
		Use = "CS 1.6's three grenades: AShooterWeapon_HEGrenade, AShooterWeapon_Flashbang, AShooterWeapon_SmokeGrenade" },
	@{ Name = "ShooterGame's team-named bots and the buy menu's own number keys (ps2-shipping N30e)"
		Pattern = 'Bot_(CT|T)_\d|Bot_%s_%d|\bNextBotNumber\b|\bOnBuyMenuItem\d\b|\bBuyMenuInputComponent\b'
		Use = "CS's BotProfile names (AShooterGameMode::BotNames, GetBotName), the bot's stream seeded by its index (AShooterAIController::SetBotIndex); the menus' input component (MenuInputComponent, OnMenuItem1..9)" },
	@{ Name = "ShooterGame's surface table and its one footstep (ps2-shipping N30f)"
		Pattern = '\bEShooterSurface\b|\bFShooterSurfaceMaterial\b|\bSurfaceMaterials\b|\bFootstepSoundName\b|\bS_Footstep\b'
		Use = "the physical materials (UMaterial::PhysMaterial, FHitResult::PhysMaterial with bReturnPhysicalMaterial, UPhysicalMaterial::DetermineSurfaceType, SHOOTER_SURFACE_*) and the surfaces' sounds (AShooterCharacter::FootstepSounds, AShooterWeapon_Instant::ImpactSounds)" },
	@{ Name = "the canvas as triangles and the scene's two gathers (ps2-shipping N15)"
		Pattern = '\bFCanvas::GetTriangles\b|Canvas(\.|->)GetTriangles\b|\bGather(Static|Skeletal)Meshes\b'
		Use = "FCanvas::GetPrimitives (rectangles as the GS's sprites), FScene::GatherPrimitives (with the view's cells)" },
	@{ Name = "the single pad's raw state and the IOP modules loaded by hand (ps2-shipping N24)"
		Pattern = 'FPS2InputInterface::Get\b|\b(GetRawButtonMask|GetRawSticks)\b|SifLoadModule\("rom0:(SIO2MAN|PADMAN|MCMAN|MCSERV)'
		Use = "IInputInterface (a controller id, the pressure axes, SetForceFeedbackChannelValues); FPlatformMisc::LoadIopModule" },
	@{ Name = "the skinned batches posed on the EE (ps2-shipping N14b)"
		Pattern = '\b(bQuantizedPose|AllocatePosedStreams|QuantizePose|SkinBatch|PosedPositions|PosedNormals)\b'
		Use = "VU1's Skinned programs with the batch's palette (FGSCommandList::AllocateSkinPalette, MakeSkinPalette)" },
	@{ Name = "ShooterGame's per-class optional asset loaders (ps2-shipping N24b)"; Pattern = '\bLoadOptionalAsset\b'
		Use = "LoadShooterObject / LoadShooterAsset (ShooterGame), which resolve what is in memory and keep it" },
	@{ Name = "legacy GL / transform / axes"; Pattern = 'LegacyGL|FLegacyTransform|LegacyAxes'
		Use = "UE view and projection matrices (ToGLClipSpace last), FTransform, UE axes" },
	@{ Name = "legacy coordinate conversion outside the tests"
		Pattern = 'FLegacyCoordinateConversion|LegacyCoordinateConversion\.h'; AllowedIn = $TestsOnly
		Use = "UE-space data; only tests convert legacy data (allowlist: `$TestsOnly in CheckBannedApis.ps1)" }
)
$Excluded = '[\\/](ThirdParty|Intermediate|Binaries|Saved)[\\/]|[\\/]Runtime[\\/]Core[\\/]Private[\\/]Windows[\\/]|' +
	'[\\/]Platforms[\\/]PS2[\\/]Source[\\/]Runtime[\\/]Core[\\/]|[\\/]LeonHeaderTool[\\/]|[\\/]PlayRunner[\\/]|' +
	'LeonAutomationTestsMain\.cpp$|[\\/]TestPAL[\\/]Private[\\/]'

$Roots = @("Engine\Source", "Engine\Platforms", "Engine\Plugins", "Game") | ForEach-Object { Join-Path $Root $_ } |
	Where-Object { Test-Path $_ }
$Files = Get-ChildItem -Path $Roots -Recurse -File -Include *.h, *.cpp, *.inl | Where-Object { $_.FullName -notmatch $Excluded }

$Violations = 0
foreach ($File in $Files) {
	$Text = [System.IO.File]::ReadAllText($File.FullName)
	# Blank out comments but keep the line structure so reported line numbers stay right.
	$Text = [regex]::Replace($Text, '/\*.*?\*/', { param($M) [regex]::Replace($M.Value, '[^\n]', '') }, 'Singleline')
	$Text = [regex]::Replace($Text, '//[^\n]*', '')
	$Lines = $Text -split "`n"
	for ($Index = 0; $Index -lt $Lines.Count; $Index++) {
		foreach ($Rule in $Rules) {
			if ($Lines[$Index] -cnotmatch $Rule.Pattern) { continue }
			if ($Rule.AllowedIn -and $File.FullName -match $Rule.AllowedIn) { continue }
			$Relative = $File.FullName.Substring($Root.Length + 1)
			Write-Output ("{0}:{1}: G4 {2}: {3} -> {4}" -f $Relative, ($Index + 1), $Rule.Name, $Lines[$Index].Trim(),
				$Rule.Use)
			$Violations++
		}
	}
}

if ($Violations -gt 0) {
	Write-Output "CheckBannedApis: $Violations violation(s)"
	exit 1
}
Write-Output "CheckBannedApis: OK ($($Files.Count) files)"
exit 0
