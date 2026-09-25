#include "SceneRenderer.h"

#include "Frustum.h"
#include "GLClipSpace.h"
#include "Level/Light.h"
#include "Misc/Paths.h"
#include "Primitives.h"
#include "RenderMatrices.h"
#include "RendererLog.h"

#include <glad/glad.h>

#include <random>

namespace
{

	/** The camera's projection in GL clip space: every pass draws with it. */
	FMatrix GetProjectionGL(const UCameraComponent& Camera)
	{
		return ToGLClipSpace(Camera.ProjectionMatrix());
	}

	struct FDrawItem
	{
		int32 ObjectIndex = 0;
		int32 SubMeshIndex = 0;
		float SortKey = 0.0f;
	};

	// std140 layouts — must match blinn_phong.frag uniform blocks.
	struct alignas(16) FCameraBlock
	{
		FMatrix View = FMatrix::Identity; // UE view space
		FMatrix Projection = FMatrix::Identity; // view to GL clip space
		FMatrix ViewProjection = FMatrix::Identity; // world to GL clip space
		FVector4 CameraPos = FVector4(0.0f, 0.0f, 0.0f, 0.0f); // xyz
	};

	struct alignas(16) FLightsBlock
	{
		int32 DirCount = 0;
		int32 PointCount = 0;
		int32 Pad0 = 0;
		int32 Pad1 = 0;
		FVector4 DirDirections[MaxDirectionalLights];
		FVector4 DirColors[MaxDirectionalLights];
		FVector4 PointPositions[MaxPointLights];
		FVector4 PointColors[MaxPointLights];
		FVector4 PointRanges[MaxPointLights]; // .x = range
	};

	static_assert(sizeof(FCameraBlock) == 208, "FCameraBlock must match std140 Camera UBO");
	static_assert(sizeof(FLightsBlock) == 272, "FLightsBlock must match std140 Lights UBO");

	float DistanceSqToCamera(const UStaticMeshComponent& Object, const FVector& InCameraPos)
	{
		const FBox Box =
			TransformLocalBox(Object.Mesh->GetLocalMin(), Object.Mesh->GetLocalMax(), Object.EffectiveModelMatrix());
		const FVector D = Box.GetCenter() - InCameraPos;
		return D | D;
	}

	FBox WorldAabbFromObject(const UStaticMeshComponent& Object)
	{
		return TransformLocalBox(Object.Mesh->GetLocalMin(), Object.Mesh->GetLocalMax(), Object.EffectiveModelMatrix());
	}

	void ExpandWorldAabbFromObject(const UStaticMeshComponent& Object, FVector& WorldMin, FVector& WorldMax)
	{
		const FBox Box = WorldAabbFromObject(Object);
		WorldMin = WorldMin.ComponentMin(Box.Min);
		WorldMax = WorldMax.ComponentMax(Box.Max);
	}

	void SnapAabbOutward(FVector& WorldMin, FVector& WorldMax, float Step)
	{
		if (Step <= 0.0f)
		{
			return;
		}
		WorldMin = FVector(FMath::FloorToFloat(WorldMin.X / Step), FMath::FloorToFloat(WorldMin.Y / Step),
					   FMath::FloorToFloat(WorldMin.Z / Step)) *
			Step;
		WorldMax = FVector(FMath::CeilToFloat(WorldMax.X / Step), FMath::CeilToFloat(WorldMax.Y / Step),
					   FMath::CeilToFloat(WorldMax.Z / Step)) *
			Step;
	}

	bool ComputeCasterAabb(const ULevel& Level, FVector& WorldMin, FVector& WorldMax)
	{
		WorldMin = FVector(TNumericLimits<float>::Max());
		WorldMax = FVector(TNumericLimits<float>::Lowest());
		bool bAny = false;
		for (const UStaticMeshComponent& Object : Level.GetStaticMeshes())
		{
			if (!Object.IsShadowCaster())
			{
				continue;
			}
			ExpandWorldAabbFromObject(Object, WorldMin, WorldMax);
			bAny = true;
		}
		if (bAny)
		{
			// Quantize (50 cm) so spinning casters don't retune the ortho light every frame (shadow flicker).
			SnapAabbOutward(WorldMin, WorldMax, 50.0f);
		}
		return bAny;
	}

	// std::mt19937 + uniform_real_distribution keep the SSAO kernel and noise identical to earlier releases.
	void BuildAoKernel(FVector (&Kernel)[FSceneRenderer::MaxAoSamples])
	{
		std::mt19937 Rng(1337u);
		std::uniform_real_distribution<float> Unit(0.0f, 1.0f);
		for (int32 I = 0; I < FSceneRenderer::MaxAoSamples; ++I)
		{
			// Separate statements keep the draw order of the three components fixed.
			const float X = Unit(Rng) * 2.0f - 1.0f;
			const float Y = Unit(Rng) * 2.0f - 1.0f;
			const float Z = Unit(Rng);
			FVector Sample = FVector(X, Y, Z).GetUnsafeNormal();
			Sample *= Unit(Rng);
			float Scale = static_cast<float>(I) / static_cast<float>(FSceneRenderer::MaxAoSamples);
			Scale = 0.1f + 0.9f * (Scale * Scale);
			Kernel[I] = Sample * Scale;
		}
	}

	uint32 CreateAoNoiseTexture()
	{
		std::mt19937 Rng(42u);
		std::uniform_real_distribution<float> Unit(0.0f, 1.0f);
		FVector Noise[16];
		for (FVector& N : Noise)
		{
			const float X = Unit(Rng) * 2.0f - 1.0f;
			const float Y = Unit(Rng) * 2.0f - 1.0f;
			N = FVector(X, Y, 0.0f);
		}
		uint32 Tex = 0;
		glGenTextures(1, &Tex);
		glBindTexture(GL_TEXTURE_2D, Tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, Noise);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		return Tex;
	}

} // namespace

bool FSceneRenderer::BindLitUbos() const
{
	const bool bLitOk = LitShader.BindUniformBlock("Camera", CameraUboBinding) &&
		LitShader.BindUniformBlock("Lights", LightsUboBinding);
	if (!bLitOk)
	{
		return false;
	}
	if (SkinnedLitShader.Valid())
	{
		return SkinnedLitShader.BindUniformBlock("Camera", CameraUboBinding) &&
			SkinnedLitShader.BindUniformBlock("Lights", LightsUboBinding);
	}
	return true;
}

bool FSceneRenderer::Initialize(const FString& InShaderDirectory)
{
	ShaderDirectory = InShaderDirectory;
	auto ShaderFile = [&](const ANSICHAR* Name)
	{
		const FString UnderDir = FPaths::Combine(InShaderDirectory, Name);
		if (FPaths::FileExists(UnderDir))
		{
			return UnderDir;
		}
		// Fallback: executable-relative assets (POST_BUILD copy / packaged layout).
		return FPaths::ResolveLegacyContentPath(FPaths::Combine("assets/Shaders", Name));
	};
	if (!LitShader.LoadFromFiles(ShaderFile("blinn_phong.vert"), ShaderFile("blinn_phong.frag")))
	{
		UE_LOG(LogRenderer, Error, "Failed to load lit shaders from %s", *InShaderDirectory);
		return false;
	}
	if (!SkinnedLitShader.LoadFromFiles(ShaderFile("skinned_lit.vert"), ShaderFile("blinn_phong.frag")))
	{
		UE_LOG(LogRenderer, Error, "Failed to load skinned lit shaders from %s", *InShaderDirectory);
		return false;
	}
	if (!UnlitShader.LoadFromFiles(ShaderFile("unlit.vert"), ShaderFile("unlit.frag")))
	{
		UE_LOG(LogRenderer, Error, "Failed to load unlit shaders from %s", *InShaderDirectory);
		return false;
	}
	if (!ShadowShader.LoadFromFiles(ShaderFile("shadow_depth.vert"), ShaderFile("shadow_depth.frag")))
	{
		UE_LOG(LogRenderer, Error, "Failed to load shadow shaders from %s", *InShaderDirectory);
		return false;
	}
	if (!SkinnedShadowShader.LoadFromFiles(ShaderFile("skinned_shadow_depth.vert"), ShaderFile("shadow_depth.frag")))
	{
		UE_LOG(LogRenderer, Error, "Failed to load skinned shadow shaders from %s", *InShaderDirectory);
		return false;
	}
	if (!SsaoShader.LoadFromFiles(ShaderFile("fullscreen.vert"), ShaderFile("ssao.frag")))
	{
		UE_LOG(LogRenderer, Error, "Failed to load SSAO shaders from %s", *InShaderDirectory);
		return false;
	}
	if (!SsaoBlurShader.LoadFromFiles(ShaderFile("fullscreen.vert"), ShaderFile("ssao_blur.frag")))
	{
		UE_LOG(LogRenderer, Error, "Failed to load SSAO blur shaders from %s", *InShaderDirectory);
		return false;
	}
	if (!PostCompositeShader.LoadFromFiles(ShaderFile("fullscreen.vert"), ShaderFile("post_composite.frag")))
	{
		UE_LOG(LogRenderer, Error, "Failed to load post composite shaders from %s", *InShaderDirectory);
		return false;
	}
	if (!FxaaShader.LoadFromFiles(ShaderFile("fullscreen.vert"), ShaderFile("fxaa.frag")))
	{
		UE_LOG(LogRenderer, Error, "Failed to load FXAA shaders from %s", *InShaderDirectory);
		return false;
	}
	if (!DebugDraw.Initialize(InShaderDirectory))
	{
		return false;
	}
	if (!OverlayDebugDraw.Initialize(InShaderDirectory))
	{
		return false;
	}
	ApplyPostProcessQuality(Post, EPostProcessQuality::Low);
	if (!ShadowMap.Create(Post.ShadowMapSize))
	{
		return false;
	}
	if (!PassTimers.Create())
	{
		UE_LOG(LogRenderer, Error, "Failed to create GPU pass timers");
		return false;
	}
	if (!CameraUbo.Create(sizeof(FCameraBlock), CameraUboBinding) ||
		!LightsUbo.Create(sizeof(FLightsBlock), LightsUboBinding))
	{
		UE_LOG(LogRenderer, Error, "Failed to create camera/lights uniform buffers");
		return false;
	}
	if (!BindLitUbos())
	{
		UE_LOG(LogRenderer, Error, "Failed to bind lit shader UBO blocks");
		return false;
	}

	const uint8 White[4] = {255, 255, 255, 255};
	WhiteTexture = MakeShared<UTexture2D>(UTexture2D::Create(1, 1, White));
	FlatNormalTexture = MakeShared<UTexture2D>(UTexture2D::CreateFlatNormal(4));
	if (!WhiteTexture->Valid() || !FlatNormalTexture->Valid())
	{
		UE_LOG(LogRenderer, Error, "Failed to create default textures/meshes");
		return false;
	}

	glGenVertexArrays(1, &FullscreenVao);
	BuildAoKernel(AoKernel);
	AoNoiseTexture = CreateAoNoiseTexture();
	if (FullscreenVao == 0 || AoNoiseTexture == 0)
	{
		UE_LOG(LogRenderer, Error, "Failed to create post-process GPU resources");
		return false;
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	// Front faces wind counter-clockwise in GL window space (GL's default, made explicit).
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	return true;
}

void FSceneRenderer::Shutdown()
{
	LightsUbo.Destroy();
	CameraUbo.Destroy();
	OverlayDebugDraw.Shutdown();
	DebugDraw.Shutdown();
	FlatNormalTexture.Reset();
	WhiteTexture.Reset();
	PassTimers.Destroy();
	LdrColor.Destroy();
	SsaoTarget.Destroy();
	SceneColor.Destroy();
	PlanarReflection.Destroy();
	ShadowMap.Destroy();
	if (AoNoiseTexture != 0)
	{
		glDeleteTextures(1, &AoNoiseTexture);
		AoNoiseTexture = 0;
	}
	if (FullscreenVao != 0)
	{
		glDeleteVertexArrays(1, &FullscreenVao);
		FullscreenVao = 0;
	}
	FxaaShader.Destroy();
	PostCompositeShader.Destroy();
	SsaoBlurShader.Destroy();
	SsaoShader.Destroy();
	SkinnedShadowShader.Destroy();
	ShadowShader.Destroy();
	UnlitShader.Destroy();
	SkinnedLitShader.Destroy();
	LitShader.Destroy();
	SkeletalDraws.Empty();
	StaticDraws.Empty();
	ShaderDirectory.Empty();
}

EShaderReloadResult FSceneRenderer::ReloadShaders(bool bForce)
{
	EShaderReloadResult Result = EShaderReloadResult::Unchanged;
	const FShader::FAcceptFunction LitAccept = [this]() { return BindLitUbos(); };

	auto TryReload = [&](FShader& Shader, const FShader::FAcceptFunction& Accept = {})
	{
		const EShaderReloadResult R =
			bForce ? Shader.ForceReloadFromDisk(Accept) : Shader.ReloadFromDiskIfChanged(Accept);
		Result = MergeShaderReload(Result, R);
		return R != EShaderReloadResult::Failed || Shader.Valid();
	};

	if (!TryReload(LitShader, LitAccept) || !TryReload(SkinnedLitShader, LitAccept) || !TryReload(UnlitShader) ||
		!TryReload(ShadowShader) || !TryReload(SkinnedShadowShader) || !TryReload(SsaoShader) ||
		!TryReload(SsaoBlurShader) || !TryReload(PostCompositeShader) || !TryReload(FxaaShader))
	{
		return EShaderReloadResult::Failed;
	}
	Result = MergeShaderReload(Result, DebugDraw.ReloadShader(bForce));
	Result = MergeShaderReload(Result, OverlayDebugDraw.ReloadShader(bForce));
	return Result;
}

void FSceneRenderer::BeginFrame(int32 FramebufferWidth, int32 FramebufferHeight)
{
	FbWidth = FramebufferWidth;
	FbHeight = FramebufferHeight;
	EnsureShadowMapSize();

	const bool bPostOn = Post.bEnabled && FbWidth > 0 && FbHeight > 0;
	if (bPostOn)
	{
		(void)SceneColor.EnsureSize(FbWidth, FbHeight);
		(void)LdrColor.EnsureSize(FbWidth, FbHeight);
		// Full-res SSAO: half-res undersamples 24-bit depth into visible parallel bands.
		(void)SsaoTarget.EnsureSize(FbWidth, FbHeight);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
	glViewport(0, 0, FramebufferWidth, FramebufferHeight);
	glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void FSceneRenderer::EnsureShadowMapSize()
{
	const int32 Size = FMath::Clamp(Post.ShadowMapSize, 512, 4096);
	if (ShadowMap.Valid() && ShadowMap.GetSize() == Size)
	{
		return;
	}
	ShadowMap.Destroy();
	(void)ShadowMap.Create(Size);
}

FRHIFramebufferId FSceneRenderer::ColorRestoreFbo() const
{
	if (Post.bEnabled && SceneColor.Valid())
	{
		return SceneColor.Framebuffer();
	}
	return DrawTargetFbo;
}

void FSceneRenderer::DrawFullscreenTriangle() const
{
	glBindVertexArray(FullscreenVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
}

void FSceneRenderer::SubmitSkeletalDraw(
	const USkeletalMesh& InMesh, const FMatrix& InModel, const TArray<FMatrix>& InBoneMatrices)
{
	if (!InMesh.Valid())
	{
		return;
	}
	FSkeletalDrawItem Item;
	Item.Mesh = &InMesh;
	Item.Model = InModel;
	Item.BoneMatrices = InBoneMatrices;
	if (Item.BoneMatrices.Num() > MaxSkinBones)
	{
		Item.BoneMatrices.SetNum(MaxSkinBones);
	}
	SkeletalDraws.Add(MoveTemp(Item));
}

void FSceneRenderer::SubmitSkeletalDraw(
	const USkeletalMesh& InMesh, const FTransform& Transform, const TArray<FMatrix>& InBoneMatrices)
{
	SubmitSkeletalDraw(InMesh, Transform.ToMatrixWithScale(), InBoneMatrices);
}

void FSceneRenderer::SubmitStaticDraw(const UStaticMesh& InMesh, const FMatrix& InModel, const FMaterial& InMaterial)
{
	if (!InMesh.Valid())
	{
		return;
	}
	FStaticDrawItem Item;
	Item.Mesh = &InMesh;
	Item.Model = InModel;
	Item.Material = InMaterial;
	StaticDraws.Add(MoveTemp(Item));
}

void FSceneRenderer::ClearDebugOverlay()
{
	OverlayDebugDraw.Clear();
}

void FSceneRenderer::AddDebugLine(const FVector& A, const FVector& B, const FLinearColor& Color)
{
	OverlayDebugDraw.AddLine(A, B, Color);
}

void FSceneRenderer::AddDebugArrow(const FVector& From, const FVector& To, const FLinearColor& Color)
{
	OverlayDebugDraw.AddArrow(From, To, Color);
}

void FSceneRenderer::AddDebugAabb(const FVector& WorldMin, const FVector& WorldMax, const FLinearColor& Color)
{
	OverlayDebugDraw.AddAabb(WorldMin, WorldMax, Color);
}

void FSceneRenderer::UpdateCameraUbo(const UCameraComponent& Camera) const
{
	UpdateCameraUbo(Camera.ViewMatrix(), GetProjectionGL(Camera), Camera.GetCameraLocation());
}

void FSceneRenderer::UpdateCameraUbo(
	const FMatrix& InView, const FMatrix& InProjection, const FVector& InCameraPos) const
{
	FCameraBlock Block;
	Block.View = InView;
	Block.Projection = InProjection;
	Block.ViewProjection = InView * InProjection;
	Block.CameraPos = FVector4(InCameraPos, 1.0f);
	CameraUbo.Update(&Block, sizeof(Block));
}

void FSceneRenderer::UpdateLightsUbo(const ULevel& Level) const
{
	FLightsBlock Block;
	FMemory::Memzero(&Block, sizeof(Block));
	const auto& Dirs = Level.GetDirectionalLights();
	const auto& Points = Level.GetPointLights();

	Block.DirCount = FMath::Min(Dirs.Num(), MaxDirectionalLights);
	Block.PointCount = FMath::Min(Points.Num(), MaxPointLights);

	for (int32 I = 0; I < Block.DirCount; ++I)
	{
		const FDirectionalLight& Light = Dirs[I];
		Block.DirDirections[I] = FVector4(Light.GetDirection(), 0.0f);
		Block.DirColors[I] = FVector4(Light.LightColor * Light.Intensity, 0.0f);
	}
	for (int32 I = 0; I < Block.PointCount; ++I)
	{
		const FPointLight& Light = Points[I];
		Block.PointPositions[I] = FVector4(Light.Transform.GetLocation(), 1.0f);
		Block.PointColors[I] = FVector4(Light.LightColor * Light.Intensity, 0.0f);
		Block.PointRanges[I] = FVector4(Light.Range, 0.0f, 0.0f, 0.0f);
	}
	LightsUbo.Update(&Block, sizeof(Block));
}

void FSceneRenderer::BindShadowResources(bool bInReceiveShadows, float SourceAngleDegrees) const
{
	LitShader.SetInt("uShadowMap", 1);
	LitShader.SetInt("uReceiveShadows", (bInReceiveShadows && ShadowMap.Valid()) ? 1 : 0);
	float Texel = ShadowMap.Valid() ? 1.0f / static_cast<float>(ShadowMap.GetSize()) : 0.0f;
	// Source Angle softens PCF filter kernel (Unreal FDirectionalLight Source Angle).
	if (bInReceiveShadows && Texel > 0.0f)
	{
		const float Soft = FMath::Clamp(SourceAngleDegrees / DefaultLightSourceAngleDegrees, 0.25f, 16.0f);
		Texel *= Soft;
	}
	LitShader.SetFloat("uShadowTexelSize", Texel);
	if (bInReceiveShadows && ShadowMap.Valid())
	{
		ShadowMap.BindDepthTexture(1);
	}
}

void FSceneRenderer::BindPlanarReflection(bool bEnabled, const FMatrix& ReflectionViewProj) const
{
	LitShader.SetInt("uHasPlanarReflection", bEnabled ? 1 : 0);
	LitShader.SetInt("uPlanarReflection", 5);
	LitShader.SetMat4("uReflectionViewProj", ReflectionViewProj);
	if (bEnabled && PlanarReflection.Valid())
	{
		PlanarReflection.BindColorTexture(5);
	}
}

void FSceneRenderer::SetClipPlane(bool bEnabled, const FVector4& Plane) const
{
	const int32 Use = bEnabled ? 1 : 0;
	if (LitShader.Valid())
	{
		LitShader.Bind();
		LitShader.SetInt("uUseClipPlane", Use);
		LitShader.SetVec4("uClipPlane", Plane.X, Plane.Y, Plane.Z, Plane.W);
	}
	if (UnlitShader.Valid())
	{
		UnlitShader.Bind();
		UnlitShader.SetInt("uUseClipPlane", Use);
		UnlitShader.SetVec4("uClipPlane", Plane.X, Plane.Y, Plane.Z, Plane.W);
	}
	if (SkinnedLitShader.Valid())
	{
		SkinnedLitShader.Bind();
		SkinnedLitShader.SetInt("uUseClipPlane", Use);
		SkinnedLitShader.SetVec4("uClipPlane", Plane.X, Plane.Y, Plane.Z, Plane.W);
	}
}

void FSceneRenderer::RenderShadowPass(const ULevel& Level, const FMatrix& LightSpace)
{
	if (!ShadowMap.Valid())
	{
		return;
	}

	PassTimers.Begin(FGPUPassTimer::EPass::Shadow);
	ShadowMap.Begin();

	if (ShadowShader.Valid())
	{
		ShadowShader.Bind();
		for (const UStaticMeshComponent& Object : Level.GetStaticMeshes())
		{
			if (!Object.IsShadowCaster())
			{
				continue;
			}
			const FMatrix LightMvp = Object.EffectiveModelMatrix() * LightSpace;
			ShadowShader.SetMat4("uLightMVP", LightMvp);

			const int32 SubCount = Object.SubMeshCount();
			for (int32 S = 0; S < SubCount; ++S)
			{
				const FMaterial& Mat = Object.MaterialForSubMesh(S);
				if (!Mat.bCastsShadows || Mat.IsTransparent() || Mat.Shading == EMaterialShadingModel::Unlit)
				{
					continue;
				}
				Object.Mesh->DrawSubMesh(S);
			}
		}
	}

	// Queued skeletal draws (Character meshes submitted before DrawScene).
	if (SkinnedShadowShader.Valid())
	{
		SkinnedShadowShader.Bind();
		for (const FSkeletalDrawItem& Item : SkeletalDraws)
		{
			if (Item.Mesh == nullptr || !Item.Mesh->Valid())
			{
				continue;
			}
			const FMaterial& LocalMaterial = Item.Mesh->GetMaterial();
			if (!LocalMaterial.bCastsShadows || LocalMaterial.IsTransparent() ||
				LocalMaterial.Shading == EMaterialShadingModel::Unlit)
			{
				continue;
			}
			const FMatrix LightMvp = Item.Model * LightSpace;
			SkinnedShadowShader.SetMat4("uLightMVP", LightMvp);
			if (Item.BoneMatrices.Num() > 0)
			{
				SkinnedShadowShader.SetMat4Array("uBones", &Item.BoneMatrices[0].M[0][0], Item.BoneMatrices.Num());
			}
			Item.Mesh->Draw();
		}
	}

	ShadowMap.End(FbWidth, FbHeight, ColorRestoreFbo());
	PassTimers.End(FGPUPassTimer::EPass::Shadow);
}

FMatrix FSceneRenderer::MakeReflectMatrix(float PlaneY)
{
	FMatrix ReflectMat = FMatrix::Identity;
	ReflectMat.M[1][1] = -1.0f;
	ReflectMat.M[3][1] = 2.0f * PlaneY;
	return ReflectMat;
}

void FSceneRenderer::RenderPlanarReflectionPass(const ULevel& Level, const UCameraComponent& Camera, float PlaneY)
{
	const int32 ReflW = FMath::Max(1, FMath::RoundToInt(static_cast<float>(FbWidth) * PlanarReflectionScale));
	const int32 ReflH = FMath::Max(1, FMath::RoundToInt(static_cast<float>(FbHeight) * PlanarReflectionScale));
	if (!PlanarReflection.EnsureSize(ReflW, ReflH))
	{
		return;
	}

	PassTimers.Begin(FGPUPassTimer::EPass::Planar);

	const FMatrix ReflectMat = MakeReflectMatrix(PlaneY);
	const FMatrix LocalView = ReflectMat * Camera.ViewMatrix();
	const FMatrix LocalProjection = GetProjectionGL(Camera);
	const FMatrix LocalViewProjection = LocalView * LocalProjection;
	const FVector Eye = Camera.GetCameraLocation();
	const FVector ReflectedEye(Eye.X, (2.0f * PlaneY) - Eye.Y, Eye.Z);

	FFrustum ReflectedFrustum;
	ReflectedFrustum.ExtractFromViewProjection(LocalViewProjection);

	// Identity light space — reflection pass skips shadows (cheaper mirror).
	const FMatrix LightSpace = FMatrix::Identity;

	PlanarReflection.Begin();
	glEnable(GL_CLIP_DISTANCE0);
	SetClipPlane(true, FVector4(0.0f, 1.0f, 0.0f, -PlaneY));

	if (LitShader.Valid())
	{
		UpdateCameraUbo(LocalView, LocalProjection, ReflectedEye);
		UpdateLightsUbo(Level);
		LitShader.Bind();
		BindShadowResources(false);
		BindPlanarReflection(false, FMatrix::Identity);
	}

	FDrawOptions CheapLit{};
	CheapLit.bLitPass = true;
	CheapLit.bReceiveShadows = false;
	CheapLit.bUseNormalMaps = false;
	CheapLit.bBindSharedLitTextures = false;

	FDrawOptions UnlitOpts{};
	UnlitOpts.bLitPass = false;
	UnlitOpts.bBindSharedLitTextures = false;

	bool bLitGlobalsBound = LitShader.Valid();
	for (const UStaticMeshComponent& Object : Level.GetStaticMeshes())
	{
		if (Object.bHidden || Object.Mesh == nullptr || !Object.Mesh->Valid())
		{
			continue;
		}

		const FBox WorldBox = WorldAabbFromObject(Object);
		if (!ReflectedFrustum.IntersectsAabb(WorldBox))
		{
			++FrameStats.PlanarCulled;
			continue;
		}

		const int32 SubCount = Object.SubMeshCount();
		for (int32 S = 0; S < SubCount; ++S)
		{
			const FMaterial& Mat = Object.MaterialForSubMesh(S);
			if (Mat.bPlanarMirror || Mat.IsTransparent())
			{
				continue;
			}
			const bool bLit = Mat.Shading == EMaterialShadingModel::BlinnPhong;
			FShader& Shader = bLit ? LitShader : UnlitShader;
			if (!Shader.Valid())
			{
				continue;
			}
			Shader.Bind();
			if (bLit)
			{
				if (!bLitGlobalsBound)
				{
					BindShadowResources(false);
					BindPlanarReflection(false, FMatrix::Identity);
					bLitGlobalsBound = true;
				}
				DrawSubMesh(Shader, Object, S, Mat, LocalView, LocalProjection, LightSpace, CheapLit);
			}
			else
			{
				bLitGlobalsBound = false;
				DrawSubMesh(Shader, Object, S, Mat, LocalView, LocalProjection, LightSpace, UnlitOpts);
			}
		}
	}

	// Characters are queued before DrawScene — include them in the mirror (clip + reflected frustum).
	DrawQueuedSkeletal(LocalView, LocalProjection, LightSpace, false, 0.0f, &ReflectedFrustum, true);

	SetClipPlane(false, FVector4(0.0f, 1.0f, 0.0f, 0.0f));
	glDisable(GL_CLIP_DISTANCE0);
	PlanarReflection.End(FbWidth, FbHeight, ColorRestoreFbo());
	PassTimers.End(FGPUPassTimer::EPass::Planar);
}

void FSceneRenderer::DrawSubMesh(const FShader& Shader, const UStaticMeshComponent& Object, int32 InSubMeshIndex,
	const FMaterial& InMaterial, const FMatrix& InView, const FMatrix& InProjection, const FMatrix& LightSpace,
	const FDrawOptions& Options) const
{
	if (Object.Mesh == nullptr || !Object.Mesh->Valid())
	{
		return;
	}

	const FMatrix LocalModel = Object.EffectiveModelMatrix();
	const FMatrix Mvp = LocalModel * InView * InProjection;

	Shader.SetMat4("uMVP", Mvp);
	Shader.SetMat4("uModel", LocalModel);
	Shader.SetVec3("uAlbedo", InMaterial.Albedo.X, InMaterial.Albedo.Y, InMaterial.Albedo.Z);
	Shader.SetFloat("uAlpha", InMaterial.Alpha);
	Shader.SetVec2("uUvScale", InMaterial.UvScale.X, InMaterial.UvScale.Y);
	Shader.SetInt("uAlbedoMap", 0);

	if (Options.bLitPass)
	{
		float Normal[9];
		GetNormalMatrix3x3(LocalModel, Normal);
		Shader.SetMat3("uNormalMatrix", Normal);
		Shader.SetFloat("uShininess", InMaterial.Shininess);
		Shader.SetFloat("uRoughness", InMaterial.Roughness);
		Shader.SetVec3("uSpecular", InMaterial.Specular.X, InMaterial.Specular.Y, InMaterial.Specular.Z);
		Shader.SetFloat("uMetallic", InMaterial.Metallic);
		Shader.SetMat4("uLightSpaceMatrix", LightSpace);
		Shader.SetInt("uNormalMap", 2);
		if (Options.bBindSharedLitTextures)
		{
			Shader.SetInt("uShadowMap", 1);
			Shader.SetInt("uReceiveShadows", (Options.bReceiveShadows && ShadowMap.Valid()) ? 1 : 0);
			Shader.SetFloat(
				"uShadowTexelSize", ShadowMap.Valid() ? 1.0f / static_cast<float>(ShadowMap.GetSize()) : 0.0f);
			if (Options.bReceiveShadows && ShadowMap.Valid())
			{
				ShadowMap.BindDepthTexture(1);
			}
		}
	}

	const UTexture2D* Albedo =
		(InMaterial.AlbedoMap && InMaterial.AlbedoMap->Valid()) ? InMaterial.AlbedoMap.Get() : WhiteTexture.Get();
	Albedo->Bind(0);

	if (Options.bLitPass)
	{
		const UTexture2D* Normals = (Options.bUseNormalMaps && InMaterial.NormalMap && InMaterial.NormalMap->Valid())
			? InMaterial.NormalMap.Get()
			: FlatNormalTexture.Get();
		Normals->Bind(2);
	}

	Object.Mesh->DrawSubMesh(InSubMeshIndex);
}

void FSceneRenderer::DrawScene(const ULevel& Level, const UCameraComponent& Camera)
{
	FrameStats = {};
	PassTimers.BeginFrame();
	FrameStats.ShadowMs = PassTimers.Milliseconds(FGPUPassTimer::EPass::Shadow);
	FrameStats.PlanarMs = PassTimers.Milliseconds(FGPUPassTimer::EPass::Planar);
	FrameStats.ColorMs = PassTimers.Milliseconds(FGPUPassTimer::EPass::Color);
	FrameStats.SsaoMs = PassTimers.Milliseconds(FGPUPassTimer::EPass::Ssao);
	FrameStats.PostMs = PassTimers.Milliseconds(FGPUPassTimer::EPass::Post);

	// Editor Player Collision / similar: clear + overlay only (keep timer pairs intact).
	if (!bSceneGeometryEnabled)
	{
		PassTimers.Begin(FGPUPassTimer::EPass::Shadow);
		PassTimers.End(FGPUPassTimer::EPass::Shadow);
		PassTimers.Begin(FGPUPassTimer::EPass::Planar);
		PassTimers.End(FGPUPassTimer::EPass::Planar);
		PassTimers.Begin(FGPUPassTimer::EPass::Color);
		PassTimers.End(FGPUPassTimer::EPass::Color);
		PassTimers.Begin(FGPUPassTimer::EPass::Ssao);
		PassTimers.End(FGPUPassTimer::EPass::Ssao);
		PassTimers.Begin(FGPUPassTimer::EPass::Post);
		PassTimers.End(FGPUPassTimer::EPass::Post);

		if (OverlayDebugDraw.IsValid() && !OverlayDebugDraw.IsEmpty())
		{
			glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
			glViewport(0, 0, FbWidth, FbHeight);
			OverlayDebugDraw.Flush(Camera.ViewMatrix() * GetProjectionGL(Camera));
		}
		OverlayDebugDraw.Clear();
		SkeletalDraws.Reset();
		StaticDraws.Reset();
		return;
	}

	const bool bPostOn = Post.bEnabled && SceneColor.Valid();

	const FMatrix LocalView = Camera.ViewMatrix();
	const FMatrix LocalProjection = GetProjectionGL(Camera);
	const FMatrix LocalViewProjection = LocalView * LocalProjection;
	const FVector LocalCameraPos = Camera.GetCameraLocation();

	FFrustum CameraFrustum;
	CameraFrustum.ExtractFromViewProjection(LocalViewProjection);

	FMatrix LightSpace = FMatrix::Identity;
	// Shadow map follows directional light 0 when castShadows; extras are lighting-only.
	const bool bCastDirShadows = Level.GetDirectionalLights().Num() > 0 && Level.GetDirectionalLights()[0].bCastShadows;
	if (bCastDirShadows)
	{
		const FVector LightDir = Level.GetDirectionalLights()[0].GetDirection();
		FVector WorldMin;
		FVector WorldMax;
		/** Shadow box padding and the box used without casters (cm). */
		constexpr float ShadowPadding = 75.0f;
		if (ComputeCasterAabb(Level, WorldMin, WorldMax))
		{
			LightSpace = FShadowMap::FitLightSpaceMatrix(LightDir, WorldMin, WorldMax, ShadowPadding);
		}
		else
		{
			LightSpace = FShadowMap::FitLightSpaceMatrix(
				LightDir, FVector(-300.0f, 0.0f, -300.0f), FVector(300.0f, 200.0f, 300.0f), ShadowPadding);
		}
		RenderShadowPass(Level, LightSpace);
	}
	else
	{
		// Keep timer queries paired every frame (double-buffered HUD).
		PassTimers.Begin(FGPUPassTimer::EPass::Shadow);
		PassTimers.End(FGPUPassTimer::EPass::Shadow);
	}

	// Optional horizontal planar mirror (first material with planarMirror=true).
	bool bHasPlanarMirror = false;
	float MirrorPlaneY = 0.0f;
	FMatrix ReflectionViewProj = FMatrix::Identity;
	for (const UStaticMeshComponent& Object : Level.GetStaticMeshes())
	{
		const int32 SubCount = Object.SubMeshCount();
		for (int32 S = 0; S < SubCount; ++S)
		{
			if (Object.MaterialForSubMesh(S).bPlanarMirror)
			{
				bHasPlanarMirror = true;
				// Reflect about the visible top of the mirror mesh (not actor origin).
				MirrorPlaneY = WorldAabbFromObject(Object).Max.Y;
				break;
			}
		}
		if (bHasPlanarMirror)
		{
			break;
		}
	}
	if (bHasPlanarMirror)
	{
		RenderPlanarReflectionPass(Level, Camera, MirrorPlaneY);
		ReflectionViewProj = MakeReflectMatrix(MirrorPlaneY) * Camera.ViewMatrix() * LocalProjection;
	}
	else
	{
		PassTimers.Begin(FGPUPassTimer::EPass::Planar);
		PassTimers.End(FGPUPassTimer::EPass::Planar);
	}

	TArray<FDrawItem> Opaque;
	TArray<FDrawItem> Transparent;
	Opaque.Reserve(Level.GetStaticMeshes().Num());
	Transparent.Reserve(Level.GetStaticMeshes().Num());

	for (int32 I = 0; I < Level.GetStaticMeshes().Num(); ++I)
	{
		const UStaticMeshComponent& Object = Level.GetStaticMeshes()[I];
		if (Object.bHidden || Object.Mesh == nullptr || !Object.Mesh->Valid())
		{
			continue;
		}
		++FrameStats.ObjectsTotal;

		const FBox WorldBox = WorldAabbFromObject(Object);
		if (!CameraFrustum.IntersectsAabb(WorldBox))
		{
			++FrameStats.ObjectsCulled;
			continue;
		}
		++FrameStats.ObjectsVisible;

		const float LocalSortKey = DistanceSqToCamera(Object, LocalCameraPos);
		const int32 SubCount = Object.SubMeshCount();
		FrameStats.DrawsSubmitted += SubCount;
		FrameStats.TrianglesSubmitted += Object.Mesh->TriangleCount();

		for (int32 S = 0; S < SubCount; ++S)
		{
			const FMaterial& Mat = Object.MaterialForSubMesh(S);
			const FDrawItem Item{I, S, LocalSortKey};
			if (Mat.IsTransparent())
			{
				Transparent.Add(Item);
			}
			else
			{
				Opaque.Add(Item);
			}
		}
	}

	// Stable: submeshes of one object share a key and keep their submission order.
	Opaque.StableSort([](const FDrawItem& A, const FDrawItem& B) { return A.SortKey < B.SortKey; });
	Transparent.StableSort([](const FDrawItem& A, const FDrawItem& B) { return A.SortKey > B.SortKey; });

	PassTimers.Begin(FGPUPassTimer::EPass::Color);

	if (bPostOn)
	{
		SceneColor.Begin();
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
		glViewport(0, 0, FbWidth, FbHeight);
	}

	const float ShadowSourceAngle =
		bCastDirShadows ? Level.GetDirectionalLights()[0].SourceAngle : DefaultLightSourceAngleDegrees;

	// Optional early-Z: write opaque depth before expensive lit shading.
	if (Post.bEarlyZ && UnlitShader.Valid() && Opaque.Num() > 0)
	{
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);
		glDisable(GL_BLEND);
		UnlitShader.Bind();
		UnlitShader.SetInt("uUseClipPlane", 0);
		UnlitShader.SetVec4("uClipPlane", 0.0f, 1.0f, 0.0f, 0.0f);
		UnlitShader.SetInt("uAlbedoMap", 0);
		UnlitShader.SetVec2("uUvScale", 1.0f, 1.0f);
		UnlitShader.SetFloat("uAlpha", 1.0f);
		WhiteTexture->Bind(0);
		for (const FDrawItem& Item : Opaque)
		{
			const UStaticMeshComponent& Object = Level.GetStaticMeshes()[Item.ObjectIndex];
			const FMaterial& Mat = Object.MaterialForSubMesh(Item.SubMeshIndex);
			if (Mat.Shading == EMaterialShadingModel::Unlit)
			{
				continue;
			}
			const FMatrix LocalModel = Object.EffectiveModelMatrix();
			const FMatrix Mvp = LocalModel * LocalView * LocalProjection;
			UnlitShader.SetMat4("uMVP", Mvp);
			UnlitShader.SetMat4("uModel", LocalModel);
			UnlitShader.SetVec3("uAlbedo", 1.0f, 1.0f, 1.0f);
			Object.Mesh->DrawSubMesh(Item.SubMeshIndex);
		}
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthFunc(GL_LEQUAL);
	}

	if (LitShader.Valid())
	{
		UpdateCameraUbo(Camera);
		UpdateLightsUbo(Level);
		LitShader.Bind();
		BindShadowResources(bCastDirShadows, ShadowSourceAngle);
		SetClipPlane(false, FVector4(0.0f, 1.0f, 0.0f, 0.0f));
		BindPlanarReflection(false, ReflectionViewProj);
		if (bHasPlanarMirror && PlanarReflection.Valid())
		{
			PlanarReflection.BindColorTexture(5);
		}
	}

	FDrawOptions LitOpts{};
	LitOpts.bLitPass = true;
	LitOpts.bReceiveShadows = bCastDirShadows;
	LitOpts.bUseNormalMaps = true;
	LitOpts.bBindSharedLitTextures = false;

	FDrawOptions UnlitOpts{};
	UnlitOpts.bLitPass = false;
	UnlitOpts.bBindSharedLitTextures = false;

	auto DrawList = [&](const TArray<FDrawItem>& Items, bool bTransparentPass)
	{
		bool bLitGlobalsBound = LitShader.Valid();
		bool bMirrorEnabled = false;

		for (const FDrawItem& Item : Items)
		{
			const UStaticMeshComponent& Object = Level.GetStaticMeshes()[Item.ObjectIndex];
			const FMaterial& Mat = Object.MaterialForSubMesh(Item.SubMeshIndex);
			const bool bLit = Mat.Shading == EMaterialShadingModel::BlinnPhong;
			FShader& Shader = bLit ? LitShader : UnlitShader;
			if (!Shader.Valid())
			{
				continue;
			}

			if (bTransparentPass)
			{
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glDepthMask(GL_FALSE);
			}
			else
			{
				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
			}

			Shader.Bind();
			if (bLit)
			{
				if (!bLitGlobalsBound)
				{
					BindShadowResources(bCastDirShadows, ShadowSourceAngle);
					if (bHasPlanarMirror && PlanarReflection.Valid())
					{
						PlanarReflection.BindColorTexture(5);
					}
					bLitGlobalsBound = true;
					bMirrorEnabled = false;
				}
				const bool bUseMirror = bHasPlanarMirror && Mat.bPlanarMirror && PlanarReflection.Valid();
				if (bUseMirror != bMirrorEnabled)
				{
					BindPlanarReflection(bUseMirror, ReflectionViewProj);
					bMirrorEnabled = bUseMirror;
				}
				DrawSubMesh(Shader, Object, Item.SubMeshIndex, Mat, LocalView, LocalProjection, LightSpace, LitOpts);
			}
			else
			{
				bLitGlobalsBound = false;
				bMirrorEnabled = false;
				DrawSubMesh(Shader, Object, Item.SubMeshIndex, Mat, LocalView, LocalProjection, LightSpace, UnlitOpts);
			}
		}

		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	};

	DrawList(Opaque, false);
	DrawQueuedSkeletal(LocalView, LocalProjection, LightSpace, bCastDirShadows, ShadowSourceAngle, &CameraFrustum);
	DrawQueuedStatic(Level, LocalView, LocalProjection, LightSpace, bCastDirShadows, ShadowSourceAngle);
	if (LitShader.Valid())
	{
		LitShader.Bind();
		BindShadowResources(bCastDirShadows, ShadowSourceAngle);
		if (bHasPlanarMirror && PlanarReflection.Valid())
		{
			PlanarReflection.BindColorTexture(5);
		}
		BindPlanarReflection(false, ReflectionViewProj);
	}
	DrawList(Transparent, true);

	if (Post.bEarlyZ)
	{
		glDepthFunc(GL_LESS);
	}

	PassTimers.End(FGPUPassTimer::EPass::Color);

	// Debug into the color target (scene HDR or backbuffer) so depth occlusion stays correct.
	DrawDebug(Level, Camera, LightSpace, bCastDirShadows);

	if (bPostOn)
	{
		RenderPostStack(Camera);
	}
	else
	{
		PassTimers.Begin(FGPUPassTimer::EPass::Ssao);
		PassTimers.End(FGPUPassTimer::EPass::Ssao);
		PassTimers.Begin(FGPUPassTimer::EPass::Post);
		PassTimers.End(FGPUPassTimer::EPass::Post);
		glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
		glViewport(0, 0, FbWidth, FbHeight);
	}

	if (OverlayDebugDraw.IsValid() && !OverlayDebugDraw.IsEmpty())
	{
		glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
		glViewport(0, 0, FbWidth, FbHeight);
		OverlayDebugDraw.Flush(Camera.ViewMatrix() * GetProjectionGL(Camera));
	}
	OverlayDebugDraw.Clear();
	SkeletalDraws.Reset();
	StaticDraws.Reset();
}

void FSceneRenderer::ReadFramebufferBgr(int32 Width, int32 Height, TArray<uint8>& OutBgr) const
{
	OutBgr.SetNumUninitialized(FMath::Max(0, Width * Height * 3));
	if (OutBgr.Num() == 0)
	{
		return;
	}
	glBindFramebuffer(GL_READ_FRAMEBUFFER, DrawTargetFbo);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, Width, Height, GL_BGR, GL_UNSIGNED_BYTE, OutBgr.GetData());
}

void FSceneRenderer::RenderPostStack(const UCameraComponent& Camera)
{
	// Flow: SceneColor(+Depth) → SSAO → bilateral blur → composite+tonemap → FXAA → present
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);

	const bool bWantAo = Post.bAmbientOcclusion && SsaoShader.Valid() && SsaoBlurShader.Valid() && SsaoTarget.Valid() &&
		Post.AoSampleCount > 0;
	int32 AoReadIndex = 1;

	PassTimers.Begin(FGPUPassTimer::EPass::Ssao);
	if (bWantAo)
	{
		// GL clip space: ssao.frag reads GL depth and reconstructs UE view-space positions (+Z forward).
		const FMatrix LocalProjection = GetProjectionGL(Camera);
		const FMatrix InvProjection = LocalProjection.Inverse();
		const int32 SampleCount = FMath::Clamp(Post.AoSampleCount, 1, MaxAoSamples);

		SsaoTarget.BindWrite(0);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		SsaoShader.Bind();
		SsaoShader.SetInt("uDepth", 0);
		SsaoShader.SetInt("uNoise", 1);
		SsaoShader.SetInt("uSampleCount", SampleCount);
		SsaoShader.SetMat4("uProjection", LocalProjection);
		SsaoShader.SetMat4("uInvProjection", InvProjection);
		SsaoShader.SetFloat("uRadius", Post.AoRadius);
		SsaoShader.SetFloat("uBias", Post.AoBias);
		const float NoiseScaleX = static_cast<float>(SsaoTarget.GetWidth()) / 4.0f;
		const float NoiseScaleY = static_cast<float>(SsaoTarget.GetHeight()) / 4.0f;
		SsaoShader.SetVec2("uNoiseScale", NoiseScaleX, NoiseScaleY);
		for (int32 I = 0; I < SampleCount; ++I)
		{
			const FVector& S = AoKernel[I];
			ANSICHAR Name[32];
			FCStringAnsi::Snprintf(Name, static_cast<int32>(sizeof(Name)), "uSamples[%d]", I);
			SsaoShader.SetVec3(Name, S.X, S.Y, S.Z);
		}
		SceneColor.BindDepthTexture(0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, AoNoiseTexture);
		DrawFullscreenTriangle();

		// Horizontal then vertical spatial blur (dissolves noise without keeping depth bands).
		const float TexelX = 1.0f / static_cast<float>(SsaoTarget.GetWidth());
		const float TexelY = 1.0f / static_cast<float>(SsaoTarget.GetHeight());
		SsaoBlurShader.Bind();
		SsaoBlurShader.SetInt("uAo", 0);
		SsaoBlurShader.SetVec2("uDirection", TexelX, 0.0f);

		SsaoTarget.BindWrite(1);
		SsaoTarget.BindColorTexture(0, 0);
		DrawFullscreenTriangle();

		SsaoTarget.BindWrite(0);
		SsaoBlurShader.SetVec2("uDirection", 0.0f, TexelY);
		SsaoTarget.BindColorTexture(1, 0);
		DrawFullscreenTriangle();
		AoReadIndex = 0;
	}
	PassTimers.End(FGPUPassTimer::EPass::Ssao);

	PassTimers.Begin(FGPUPassTimer::EPass::Post);
	const float Exposure = FMath::Max(0.01f, Post.Exposure);
	const bool bWantFxaa = Post.bFxaa && FxaaShader.Valid() && LdrColor.Valid();

	if (bWantFxaa)
	{
		LdrColor.BindWrite();
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
		glViewport(0, 0, FbWidth, FbHeight);
	}

	if (PostCompositeShader.Valid())
	{
		PostCompositeShader.Bind();
		PostCompositeShader.SetInt("uSceneColor", 0);
		PostCompositeShader.SetInt("uAo", 1);
		PostCompositeShader.SetInt("uUseAo", bWantAo ? 1 : 0);
		PostCompositeShader.SetFloat("uAoIntensity", Post.AoIntensity);
		PostCompositeShader.SetFloat("uAoPower", Post.AoPower);
		PostCompositeShader.SetFloat("uExposure", Exposure);
		SceneColor.BindColorTexture(0);
		if (bWantAo)
		{
			SsaoTarget.BindColorTexture(AoReadIndex, 1);
		}
		else if (WhiteTexture != nullptr)
		{
			WhiteTexture->Bind(1);
		}
		DrawFullscreenTriangle();
	}

	if (bWantFxaa)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
		glViewport(0, 0, FbWidth, FbHeight);
		FxaaShader.Bind();
		FxaaShader.SetInt("uColor", 0);
		FxaaShader.SetVec2("uInvResolution", 1.0f / static_cast<float>(FbWidth), 1.0f / static_cast<float>(FbHeight));
		LdrColor.BindColorTexture(0);
		DrawFullscreenTriangle();
	}
	PassTimers.End(FGPUPassTimer::EPass::Post);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);
	glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
	glViewport(0, 0, FbWidth, FbHeight);
}

void FSceneRenderer::DrawQueuedSkeletal(const FMatrix& InView, const FMatrix& InProjection, const FMatrix& LightSpace,
	bool bInReceiveShadows, float ShadowSourceAngle, const FFrustum* CameraFrustum, bool bUseWorldClipPlane)
{
	if (!SkinnedLitShader.Valid() || SkeletalDraws.Num() == 0 || WhiteTexture == nullptr)
	{
		return;
	}

	SkinnedLitShader.Bind();

	float Texel = ShadowMap.Valid() ? 1.0f / static_cast<float>(ShadowMap.GetSize()) : 0.0f;
	if (bInReceiveShadows && Texel > 0.0f)
	{
		const float Soft = FMath::Clamp(ShadowSourceAngle / DefaultLightSourceAngleDegrees, 0.25f, 16.0f);
		Texel *= Soft;
	}
	SkinnedLitShader.SetInt("uShadowMap", 1);
	SkinnedLitShader.SetInt("uReceiveShadows", (bInReceiveShadows && ShadowMap.Valid()) ? 1 : 0);
	SkinnedLitShader.SetFloat("uShadowTexelSize", Texel);
	if (bInReceiveShadows && ShadowMap.Valid())
	{
		ShadowMap.BindDepthTexture(1);
	}

	SkinnedLitShader.SetInt("uHasPlanarReflection", 0);
	if (!bUseWorldClipPlane)
	{
		SkinnedLitShader.SetInt("uUseClipPlane", 0);
		SkinnedLitShader.SetVec4("uClipPlane", 0.0f, 1.0f, 0.0f, 0.0f);
	}

	for (const FSkeletalDrawItem& Item : SkeletalDraws)
	{
		if (Item.Mesh == nullptr || !Item.Mesh->Valid())
		{
			continue;
		}

		++FrameStats.ObjectsTotal;
		if (CameraFrustum != nullptr)
		{
			const FBox WorldBox = TransformLocalBox(Item.Mesh->GetLocalMin(), Item.Mesh->GetLocalMax(), Item.Model);
			if (!CameraFrustum->IntersectsAabb(WorldBox))
			{
				++FrameStats.ObjectsCulled;
				continue;
			}
		}
		++FrameStats.ObjectsVisible;

		const FMaterial& LocalMaterial = Item.Mesh->GetMaterial();
		const FMatrix& LocalModel = Item.Model;
		const FMatrix Mvp = LocalModel * InView * InProjection;
		float Normal[9];
		GetNormalMatrix3x3(LocalModel, Normal);

		SkinnedLitShader.SetMat4("uMVP", Mvp);
		SkinnedLitShader.SetMat4("uModel", LocalModel);
		SkinnedLitShader.SetMat3("uNormalMatrix", Normal);
		SkinnedLitShader.SetMat4("uLightSpaceMatrix", LightSpace);
		SkinnedLitShader.SetVec3("uAlbedo", LocalMaterial.Albedo.X, LocalMaterial.Albedo.Y, LocalMaterial.Albedo.Z);
		SkinnedLitShader.SetFloat("uAlpha", LocalMaterial.Alpha);
		SkinnedLitShader.SetVec2("uUvScale", LocalMaterial.UvScale.X, LocalMaterial.UvScale.Y);
		SkinnedLitShader.SetFloat("uShininess", LocalMaterial.Shininess);
		SkinnedLitShader.SetFloat("uRoughness", LocalMaterial.Roughness);
		SkinnedLitShader.SetVec3(
			"uSpecular", LocalMaterial.Specular.X, LocalMaterial.Specular.Y, LocalMaterial.Specular.Z);
		SkinnedLitShader.SetFloat("uMetallic", LocalMaterial.Metallic);
		SkinnedLitShader.SetInt("uAlbedoMap", 0);
		SkinnedLitShader.SetInt("uNormalMap", 2);
		// Keep pass-level shadow uniforms (valid map + soft texel); do not overwrite per draw.

		if (Item.BoneMatrices.Num() > 0)
		{
			SkinnedLitShader.SetMat4Array("uBones", &Item.BoneMatrices[0].M[0][0], Item.BoneMatrices.Num());
		}

		const UTexture2D* Albedo = LocalMaterial.AlbedoMap && LocalMaterial.AlbedoMap->Valid()
			? LocalMaterial.AlbedoMap.Get()
			: WhiteTexture.Get();
		Albedo->Bind(0);
		const UTexture2D* Normals = LocalMaterial.NormalMap && LocalMaterial.NormalMap->Valid()
			? LocalMaterial.NormalMap.Get()
			: FlatNormalTexture.Get();
		Normals->Bind(2);

		Item.Mesh->Draw();
		++FrameStats.DrawsSubmitted;
		FrameStats.TrianglesSubmitted += Item.Mesh->TriangleCount();
	}
}

void FSceneRenderer::DrawQueuedStatic(const ULevel& Level, const FMatrix& InView, const FMatrix& InProjection,
	const FMatrix& LightSpace, bool bInReceiveShadows, float ShadowSourceAngle)
{
	if (StaticDraws.Num() == 0 || WhiteTexture == nullptr || !UnlitShader.Valid())
	{
		return;
	}

	// Attachments: unlit + two-pass depth (avoids grey ghost from two-sided z-fight).
	glDisable(GL_CLIP_DISTANCE0);
	glDisable(GL_BLEND);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	UnlitShader.Bind();
	UnlitShader.SetInt("uUseClipPlane", 0);
	UnlitShader.SetVec4("uClipPlane", 0.0f, 1.0f, 0.0f, 0.0f);
	UnlitShader.SetInt("uAlbedoMap", 0);
	UnlitShader.SetVec2("uUvScale", 1.0f, 1.0f);
	UnlitShader.SetFloat("uAlpha", 1.0f);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	WhiteTexture->Bind(0);

	for (const FStaticDrawItem& Item : StaticDraws)
	{
		if (Item.Mesh == nullptr || !Item.Mesh->Valid())
		{
			continue;
		}

		const FMaterial& LocalMaterial = Item.Material;
		const FMatrix& LocalModel = Item.Model;
		const FMatrix Mvp = LocalModel * InView * InProjection;

		UnlitShader.SetMat4("uMVP", Mvp);
		UnlitShader.SetMat4("uModel", LocalModel);
		UnlitShader.SetVec3("uAlbedo", LocalMaterial.Albedo.X, LocalMaterial.Albedo.Y, LocalMaterial.Albedo.Z);

		// Pass 1: establish closest depth (both windings compete).
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthFunc(GL_LESS);
		Item.Mesh->Draw();
		// Pass 2: solid color only on the winning depth samples.
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthFunc(GL_EQUAL);
		Item.Mesh->Draw();
		glDepthFunc(GL_LESS);

		++FrameStats.ObjectsTotal;
		++FrameStats.ObjectsVisible;
		++FrameStats.DrawsSubmitted;
		FrameStats.TrianglesSubmitted += Item.Mesh->TriangleCount();
		(void)Level;
		(void)LightSpace;
		(void)bInReceiveShadows;
		(void)ShadowSourceAngle;
	}

	glEnable(GL_CULL_FACE);
}

void FSceneRenderer::DrawDebug(
	const ULevel& Level, const UCameraComponent& Camera, const FMatrix& LightSpace, bool bHasLightSpace)
{
	if (!bDebugDrawEnabled || !DebugDraw.IsValid())
	{
		return;
	}

	DebugDraw.Clear();

	constexpr FLinearColor AabbColor(0.2f, 0.95f, 0.35f);
	constexpr FLinearColor HiddenAabbColor(0.95f, 0.35f, 0.85f); // BlockingVolume / hidden
	constexpr FLinearColor FrustumColor(1.0f, 0.85f, 0.15f);

	for (const UStaticMeshComponent& Object : Level.GetStaticMeshes())
	{
		if (Object.Mesh == nullptr || !Object.Mesh->Valid())
		{
			continue;
		}
		const FBox Box = WorldAabbFromObject(Object);
		DebugDraw.AddAabb(Box.Min, Box.Max, Object.bHidden ? HiddenAabbColor : AabbColor);
	}

	if (bHasLightSpace)
	{
		DebugDraw.AddLightFrustum(LightSpace, FrustumColor);
	}

	DebugDraw.Flush(Camera.ViewMatrix() * GetProjectionGL(Camera));
}
