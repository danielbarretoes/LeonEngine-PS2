#include <glad/glad.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include "Misc/Paths.h"
#include "Level/Light.h"
#include "Frustum.h"
#include "Primitives.h"
#include "SceneRenderer.h"
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

struct FDrawItem {
    std::size_t ObjectIndex = 0;
    std::size_t SubMeshIndex = 0;
    float SortKey = 0.0f;
};

// std140 layouts — must match blinn_phong.frag uniform blocks.
struct alignas(16) CameraBlock {
    glm::mat4 View{1.0f};
    glm::mat4 Projection{1.0f};
    glm::mat4 ViewProjection{1.0f};
    glm::vec4 CameraPos{0.0f}; // xyz
};

struct alignas(16) LightsBlock {
    int DirCount = 0;
    int PointCount = 0;
    int Pad0 = 0;
    int Pad1 = 0;
    glm::vec4 DirDirections[MaxDirectionalLights]{};
    glm::vec4 DirColors[MaxDirectionalLights]{};
    glm::vec4 PointPositions[MaxPointLights]{};
    glm::vec4 PointColors[MaxPointLights]{};
    glm::vec4 PointRanges[MaxPointLights]{}; // .x = range
};

static_assert(sizeof(CameraBlock) == 208, "CameraBlock must match std140 Camera UBO");
static_assert(sizeof(LightsBlock) == 272, "LightsBlock must match std140 Lights UBO");

float DistanceSqToCamera(const UStaticMeshComponent& Object, const glm::vec3& InCameraPos) {
    const FBox Box = FBox::FromLocalTransformed(Object.Mesh->GetLocalMin(), Object.Mesh->GetLocalMax(),
                                                Object.EffectiveModelMatrix());
    const glm::vec3 Center = (Box.Min + Box.Max) * 0.5f;
    const glm::vec3 D = Center - InCameraPos;
    return glm::dot(D, D);
}

FBox WorldAabbFromObject(const UStaticMeshComponent& Object) {
    return FBox::FromLocalTransformed(Object.Mesh->GetLocalMin(), Object.Mesh->GetLocalMax(),
                                      Object.EffectiveModelMatrix());
}

void ExpandWorldAabbFromObject(const UStaticMeshComponent& Object, glm::vec3& WorldMin,
                               glm::vec3& WorldMax) {
    const FBox Box = WorldAabbFromObject(Object);
    WorldMin = glm::min(WorldMin, Box.Min);
    WorldMax = glm::max(WorldMax, Box.Max);
}

void SnapAabbOutward(glm::vec3& WorldMin, glm::vec3& WorldMax, float Step) {
    if (Step <= 0.0f) {
        return;
    }
    WorldMin = glm::floor(WorldMin / Step) * Step;
    WorldMax = glm::ceil(WorldMax / Step) * Step;
}

bool ComputeCasterAabb(const ULevel& Level, glm::vec3& WorldMin, glm::vec3& WorldMax) {
    WorldMin = glm::vec3(std::numeric_limits<float>::max());
    WorldMax = glm::vec3(std::numeric_limits<float>::lowest());
    bool bAny = false;
    for (const UStaticMeshComponent& Object : Level.GetStaticMeshes()) {
        if (!Object.IsShadowCaster()) {
            continue;
        }
        ExpandWorldAabbFromObject(Object, WorldMin, WorldMax);
        bAny = true;
    }
    if (bAny) {
        // Quantize so spinning casters don't retune the ortho light every frame (shadow flicker).
        SnapAabbOutward(WorldMin, WorldMax, 0.5f);
    }
    return bAny;
}

glm::mat4 MakeReflectMatrix(float PlaneY) {
    glm::mat4 ReflectMat(1.0f);
    ReflectMat[1][1] = -1.0f;
    ReflectMat[3][1] = 2.0f * PlaneY;
    return ReflectMat;
}

void BuildAoKernel(std::array<glm::vec3, FSceneRenderer::MaxAoSamples>& Kernel) {
    std::mt19937 Rng(1337u);
    std::uniform_real_distribution<float> Unit(0.0f, 1.0f);
    for (int I = 0; I < FSceneRenderer::MaxAoSamples; ++I) {
        glm::vec3 Sample{Unit(Rng) * 2.0f - 1.0f, Unit(Rng) * 2.0f - 1.0f, Unit(Rng)};
        Sample = glm::normalize(Sample);
        Sample *= Unit(Rng);
        float Scale = static_cast<float>(I) / static_cast<float>(FSceneRenderer::MaxAoSamples);
        Scale = 0.1f + 0.9f * (Scale * Scale);
        Kernel[static_cast<std::size_t>(I)] = Sample * Scale;
    }
}

unsigned int CreateAoNoiseTexture() {
    std::mt19937 Rng(42u);
    std::uniform_real_distribution<float> Unit(0.0f, 1.0f);
    std::array<glm::vec3, 16> Noise{};
    for (glm::vec3& N : Noise) {
        N = glm::vec3{Unit(Rng) * 2.0f - 1.0f, Unit(Rng) * 2.0f - 1.0f, 0.0f};
    }
    unsigned int Tex = 0;
    glGenTextures(1, &Tex);
    glBindTexture(GL_TEXTURE_2D, Tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, Noise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return Tex;
}

} // namespace

bool FSceneRenderer::BindLitUbos() const {
    const bool bLitOk = LitShader.BindUniformBlock("Camera", CameraUboBinding) &&
                       LitShader.BindUniformBlock("Lights", LightsUboBinding);
    if (!bLitOk) {
        return false;
    }
    if (SkinnedLitShader.Valid()) {
        return SkinnedLitShader.BindUniformBlock("Camera", CameraUboBinding) &&
               SkinnedLitShader.BindUniformBlock("Lights", LightsUboBinding);
    }
    return true;
}

bool FSceneRenderer::Initialize(const std::string& InShaderDirectory) {
    ShaderDirectory = InShaderDirectory;
    namespace fs = std::filesystem;
    auto ShaderFile = [&](const char* Name) {
        const fs::path UnderDir = fs::path(InShaderDirectory) / Name;
        if (fs::exists(UnderDir)) {
            return UnderDir.string();
        }
        // Fallback: executable-relative assets (POST_BUILD copy / packaged layout).
        return FPaths::ResolveAssetPath((fs::path("assets/Shaders") / Name).string());
    };
    if (!LitShader.LoadFromFiles(ShaderFile("blinn_phong.vert"), ShaderFile("blinn_phong.frag"))) {
        std::cerr << "Failed to load lit shaders from " << InShaderDirectory << '\n';
        return false;
    }
    if (!SkinnedLitShader.LoadFromFiles(ShaderFile("skinned_lit.vert"),
                                         ShaderFile("blinn_phong.frag"))) {
        std::cerr << "Failed to load skinned lit shaders from " << InShaderDirectory << '\n';
        return false;
    }
    if (!UnlitShader.LoadFromFiles(ShaderFile("unlit.vert"), ShaderFile("unlit.frag"))) {
        std::cerr << "Failed to load unlit shaders from " << InShaderDirectory << '\n';
        return false;
    }
    if (!ShadowShader.LoadFromFiles(ShaderFile("shadow_depth.vert"),
                                     ShaderFile("shadow_depth.frag"))) {
        std::cerr << "Failed to load shadow shaders from " << InShaderDirectory << '\n';
        return false;
    }
    if (!SkinnedShadowShader.LoadFromFiles(ShaderFile("skinned_shadow_depth.vert"),
                                            ShaderFile("shadow_depth.frag"))) {
        std::cerr << "Failed to load skinned shadow shaders from " << InShaderDirectory << '\n';
        return false;
    }
    if (!SkyboxShader.LoadFromFiles(ShaderFile("skybox.vert"), ShaderFile("skybox.frag"))) {
        std::cerr << "Failed to load skybox shaders from " << InShaderDirectory << '\n';
        return false;
    }
    if (!SsaoShader.LoadFromFiles(ShaderFile("fullscreen.vert"), ShaderFile("ssao.frag"))) {
        std::cerr << "Failed to load SSAO shaders from " << InShaderDirectory << '\n';
        return false;
    }
    if (!SsaoBlurShader.LoadFromFiles(ShaderFile("fullscreen.vert"),
                                       ShaderFile("ssao_blur.frag"))) {
        std::cerr << "Failed to load SSAO blur shaders from " << InShaderDirectory << '\n';
        return false;
    }
    if (!PostCompositeShader.LoadFromFiles(ShaderFile("fullscreen.vert"),
                                            ShaderFile("post_composite.frag"))) {
        std::cerr << "Failed to load post composite shaders from " << InShaderDirectory << '\n';
        return false;
    }
    if (!FxaaShader.LoadFromFiles(ShaderFile("fullscreen.vert"), ShaderFile("fxaa.frag"))) {
        std::cerr << "Failed to load FXAA shaders from " << InShaderDirectory << '\n';
        return false;
    }
    if (!DebugDraw.Initialize(InShaderDirectory)) {
        return false;
    }
    if (!OverlayDebugDraw.Initialize(InShaderDirectory)) {
        return false;
    }
    ApplyPostProcessQuality(Post, EPostProcessQuality::Low);
    if (!ShadowMap.Create(Post.ShadowMapSize)) {
        return false;
    }
    if (!PassTimers.Create()) {
        std::cerr << "Failed to create GPU pass timers\n";
        return false;
    }
    if (!CameraUbo.Create(sizeof(CameraBlock), CameraUboBinding) ||
        !LightsUbo.Create(sizeof(LightsBlock), LightsUboBinding)) {
        std::cerr << "Failed to create camera/lights uniform buffers\n";
        return false;
    }
    if (!BindLitUbos()) {
        std::cerr << "Failed to bind lit shader UBO blocks\n";
        return false;
    }

    const std::array<unsigned char, 4> White = {255, 255, 255, 255};
    WhiteTexture = std::make_shared<UTexture2D>(UTexture2D::Create(1, 1, White.data()));
    FlatNormalTexture = std::make_shared<UTexture2D>(UTexture2D::CreateFlatNormal(4));
    SkyboxMesh = std::make_shared<UStaticMesh>(UStaticMesh::Upload(MakeCube()));
    if (!WhiteTexture->Valid() || !FlatNormalTexture->Valid() || !SkyboxMesh->Valid()) {
        std::cerr << "Failed to create default textures/meshes\n";
        return false;
    }

    glGenVertexArrays(1, &FullscreenVao);
    BuildAoKernel(AoKernel);
    AoNoiseTexture = CreateAoNoiseTexture();
    if (FullscreenVao == 0 || AoNoiseTexture == 0) {
        std::cerr << "Failed to create post-process GPU resources\n";
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    return true;
}

void FSceneRenderer::Shutdown() {
    LightsUbo.Destroy();
    CameraUbo.Destroy();
    OverlayDebugDraw.Shutdown();
    DebugDraw.Shutdown();
    SkyboxMesh.reset();
    FlatNormalTexture.reset();
    WhiteTexture.reset();
    PassTimers.Destroy();
    LdrColor.Destroy();
    SsaoTarget.Destroy();
    SceneColor.Destroy();
    PlanarReflection.Destroy();
    ShadowMap.Destroy();
    if (AoNoiseTexture != 0) {
        glDeleteTextures(1, &AoNoiseTexture);
        AoNoiseTexture = 0;
    }
    if (FullscreenVao != 0) {
        glDeleteVertexArrays(1, &FullscreenVao);
        FullscreenVao = 0;
    }
    FxaaShader.Destroy();
    PostCompositeShader.Destroy();
    SsaoBlurShader.Destroy();
    SsaoShader.Destroy();
    SkyboxShader.Destroy();
    SkinnedShadowShader.Destroy();
    ShadowShader.Destroy();
    UnlitShader.Destroy();
    SkinnedLitShader.Destroy();
    LitShader.Destroy();
    SkeletalDraws.clear();
    StaticDraws.clear();
    ShaderDirectory.clear();
}

EShaderReloadResult FSceneRenderer::ReloadShaders(bool bForce) {
    EShaderReloadResult Result = EShaderReloadResult::Unchanged;
    const FShader::FAcceptFunction LitAccept = [this]() { return BindLitUbos(); };

    auto TryReload = [&](FShader& Shader, const FShader::FAcceptFunction& Accept = {}) {
        const EShaderReloadResult R =
            bForce ? Shader.ForceReloadFromDisk(Accept) : Shader.ReloadFromDiskIfChanged(Accept);
        Result = MergeShaderReload(Result, R);
        return R != EShaderReloadResult::Failed || Shader.Valid();
    };

    if (!TryReload(LitShader, LitAccept) || !TryReload(SkinnedLitShader, LitAccept) ||
        !TryReload(UnlitShader) || !TryReload(ShadowShader) || !TryReload(SkinnedShadowShader) ||
        !TryReload(SkyboxShader) || !TryReload(SsaoShader) || !TryReload(SsaoBlurShader) ||
        !TryReload(PostCompositeShader) || !TryReload(FxaaShader)) {
        return EShaderReloadResult::Failed;
    }
    Result = MergeShaderReload(Result, DebugDraw.ReloadShader(bForce));
    Result = MergeShaderReload(Result, OverlayDebugDraw.ReloadShader(bForce));
    return Result;
}

void FSceneRenderer::BeginFrame(int FramebufferWidth, int FramebufferHeight) {
    FbWidth = FramebufferWidth;
    FbHeight = FramebufferHeight;
    EnsureShadowMapSize();

    const bool bPostOn = Post.bEnabled && FbWidth > 0 && FbHeight > 0;
    if (bPostOn) {
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

void FSceneRenderer::EnsureShadowMapSize() {
    const int Size = std::clamp(Post.ShadowMapSize, 512, 4096);
    if (ShadowMap.Valid() && ShadowMap.GetSize() == Size) {
        return;
    }
    ShadowMap.Destroy();
    (void)ShadowMap.Create(Size);
}

unsigned int FSceneRenderer::ColorRestoreFbo() const {
    if (Post.bEnabled && SceneColor.Valid()) {
        return SceneColor.Framebuffer();
    }
    return DrawTargetFbo;
}

void FSceneRenderer::DrawFullscreenTriangle() const {
    glBindVertexArray(FullscreenVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void FSceneRenderer::SubmitSkeletalDraw(const USkeletalMesh& InMesh, const glm::mat4& InModel,
                                  const std::vector<glm::mat4>& InBoneMatrices) {
    if (!InMesh.Valid()) {
        return;
    }
    FSkeletalDrawItem Item;
    Item.Mesh = &InMesh;
    Item.Model = InModel;
    Item.BoneMatrices = InBoneMatrices;
    if (Item.BoneMatrices.size() > static_cast<std::size_t>(MaxSkinBones)) {
        Item.BoneMatrices.resize(static_cast<std::size_t>(MaxSkinBones));
    }
    SkeletalDraws.push_back(std::move(Item));
}

void FSceneRenderer::SubmitSkeletalDraw(const USkeletalMesh& InMesh, const FTransform& Transform,
                                  const std::vector<glm::mat4>& InBoneMatrices) {
    SubmitSkeletalDraw(InMesh, Transform.ModelMatrix(), InBoneMatrices);
}

void FSceneRenderer::SubmitStaticDraw(const UStaticMesh& InMesh, const glm::mat4& InModel,
                                const FMaterial& InMaterial) {
    if (!InMesh.Valid()) {
        return;
    }
    FStaticDrawItem Item;
    Item.Mesh = &InMesh;
    Item.Model = InModel;
    Item.Material = InMaterial;
    StaticDraws.push_back(std::move(Item));
}

void FSceneRenderer::ClearDebugOverlay() {
    OverlayDebugDraw.Clear();
}

void FSceneRenderer::AddDebugLine(const glm::vec3& A, const glm::vec3& B, const glm::vec3& Color) {
    OverlayDebugDraw.AddLine(A, B, Color);
}

void FSceneRenderer::AddDebugArrow(const glm::vec3& From, const glm::vec3& To, const glm::vec3& Color) {
    OverlayDebugDraw.AddArrow(From, To, Color);
}

void FSceneRenderer::AddDebugAabb(const glm::vec3& WorldMin, const glm::vec3& WorldMax,
                            const glm::vec3& Color) {
    OverlayDebugDraw.AddAabb(WorldMin, WorldMax, Color);
}

void FSceneRenderer::UpdateCameraUbo(const UCameraComponent& Camera) const {
    UpdateCameraUbo(Camera.ViewMatrix(), Camera.ProjectionMatrix(), Camera.GetCameraLocation());
}

void FSceneRenderer::UpdateCameraUbo(const glm::mat4& InView, const glm::mat4& InProjection,
                               const glm::vec3& InCameraPos) const {
    CameraBlock Block{};
    Block.View = InView;
    Block.Projection = InProjection;
    Block.ViewProjection = InProjection * InView;
    Block.CameraPos = glm::vec4(InCameraPos, 1.0f);
    CameraUbo.Update(&Block, sizeof(Block));
}

void FSceneRenderer::UpdateLightsUbo(const ULevel& Level) const {
    LightsBlock Block{};
    const auto& Dirs = Level.GetDirectionalLights();
    const auto& Points = Level.GetPointLights();

    Block.DirCount = std::min(static_cast<int>(Dirs.size()), MaxDirectionalLights);
    Block.PointCount = std::min(static_cast<int>(Points.size()), MaxPointLights);

    for (int I = 0; I < Block.DirCount; ++I) {
        const auto& Light = Dirs[static_cast<std::size_t>(I)];
        Block.DirDirections[I] = glm::vec4(Light.GetDirection(), 0.0f);
        Block.DirColors[I] = glm::vec4(Light.LightColor * Light.Intensity, 0.0f);
    }
    for (int I = 0; I < Block.PointCount; ++I) {
        const auto& Light = Points[static_cast<std::size_t>(I)];
        Block.PointPositions[I] = glm::vec4(Light.Transform.Position, 1.0f);
        Block.PointColors[I] = glm::vec4(Light.LightColor * Light.Intensity, 0.0f);
        Block.PointRanges[I] = glm::vec4(Light.Range, 0.0f, 0.0f, 0.0f);
    }
    LightsUbo.Update(&Block, sizeof(Block));
}

void FSceneRenderer::BindEnvironment(const ULevel& Level) const {
    const bool bHasEnv = Level.GetEnvironment() != nullptr && Level.GetEnvironment()->Valid();
    const bool bHasIrr = bHasEnv && Level.GetEnvironment()->HasIrradiance();
    LitShader.SetInt("uHasEnvMap", bHasEnv ? 1 : 0);
    LitShader.SetInt("uHasIrradiance", bHasIrr ? 1 : 0);
    LitShader.SetFloat("uEnvExposure", Level.GetEnvironmentExposure());
    LitShader.SetFloat("uEnvMaxLod", bHasEnv ? Level.GetEnvironment()->MaxLod() : 0.0f);
    LitShader.SetInt("uEnvMap", 3);
    LitShader.SetInt("uIrradianceMap", 4);
    if (bHasEnv) {
        Level.GetEnvironment()->Bind(3);
    }
    if (bHasIrr) {
        Level.GetEnvironment()->BindIrradiance(4);
    }
}

void FSceneRenderer::BindShadowResources(bool bInReceiveShadows, float SourceAngleDegrees) const {
    LitShader.SetInt("uShadowMap", 1);
    LitShader.SetInt("uReceiveShadows", (bInReceiveShadows && ShadowMap.Valid()) ? 1 : 0);
    float Texel = ShadowMap.Valid() ? 1.0f / static_cast<float>(ShadowMap.GetSize()) : 0.0f;
    // Source Angle softens PCF filter kernel (Unreal FDirectionalLight Source Angle).
    if (bInReceiveShadows && Texel > 0.0f) {
        const float Soft =
            std::clamp(SourceAngleDegrees / DefaultLightSourceAngleDegrees, 0.25f, 16.0f);
        Texel *= Soft;
    }
    LitShader.SetFloat("uShadowTexelSize", Texel);
    if (bInReceiveShadows && ShadowMap.Valid()) {
        ShadowMap.BindDepthTexture(1);
    }
}

void FSceneRenderer::BindPlanarReflection(bool bEnabled, const glm::mat4& ReflectionViewProj) const {
    LitShader.SetInt("uHasPlanarReflection", bEnabled ? 1 : 0);
    LitShader.SetInt("uPlanarReflection", 5);
    LitShader.SetMat4("uReflectionViewProj", glm::value_ptr(ReflectionViewProj));
    if (bEnabled && PlanarReflection.Valid()) {
        PlanarReflection.BindColorTexture(5);
    }
}

void FSceneRenderer::SetClipPlane(bool bEnabled, const glm::vec4& Plane) const {
    const int Use = bEnabled ? 1 : 0;
    if (LitShader.Valid()) {
        LitShader.Bind();
        LitShader.SetInt("uUseClipPlane", Use);
        LitShader.SetVec4("uClipPlane", Plane.x, Plane.y, Plane.z, Plane.w);
    }
    if (UnlitShader.Valid()) {
        UnlitShader.Bind();
        UnlitShader.SetInt("uUseClipPlane", Use);
        UnlitShader.SetVec4("uClipPlane", Plane.x, Plane.y, Plane.z, Plane.w);
    }
    if (SkinnedLitShader.Valid()) {
        SkinnedLitShader.Bind();
        SkinnedLitShader.SetInt("uUseClipPlane", Use);
        SkinnedLitShader.SetVec4("uClipPlane", Plane.x, Plane.y, Plane.z, Plane.w);
    }
}

void FSceneRenderer::RenderShadowPass(const ULevel& Level, const glm::mat4& LightSpace) {
    if (!ShadowMap.Valid()) {
        return;
    }

    PassTimers.Begin(FGPUPassTimer::EPass::Shadow);
    ShadowMap.Begin();

    if (ShadowShader.Valid()) {
        ShadowShader.Bind();
        for (const UStaticMeshComponent& Object : Level.GetStaticMeshes()) {
            if (!Object.IsShadowCaster()) {
                continue;
            }
            const glm::mat4 LightMvp = LightSpace * Object.EffectiveModelMatrix();
            ShadowShader.SetMat4("uLightMVP", glm::value_ptr(LightMvp));

            const std::size_t SubCount = Object.SubMeshCount();
            for (std::size_t S = 0; S < SubCount; ++S) {
                const FMaterial& Mat = Object.MaterialForSubMesh(S);
                if (!Mat.bCastsShadows || Mat.IsTransparent() || Mat.Shading == EMaterialShadingModel::Unlit) {
                    continue;
                }
                Object.Mesh->DrawSubMesh(S);
            }
        }
    }

    // Queued skeletal draws (Character meshes submitted before DrawScene).
    if (SkinnedShadowShader.Valid()) {
        SkinnedShadowShader.Bind();
        for (const FSkeletalDrawItem& Item : SkeletalDraws) {
            if (Item.Mesh == nullptr || !Item.Mesh->Valid()) {
                continue;
            }
            const FMaterial& LocalMaterial = Item.Mesh->GetMaterial();
            if (!LocalMaterial.bCastsShadows || LocalMaterial.IsTransparent() ||
                LocalMaterial.Shading == EMaterialShadingModel::Unlit) {
                continue;
            }
            const glm::mat4 LightMvp = LightSpace * Item.Model;
            SkinnedShadowShader.SetMat4("uLightMVP", glm::value_ptr(LightMvp));
            if (!Item.BoneMatrices.empty()) {
                SkinnedShadowShader.SetMat4Array("uBones", glm::value_ptr(Item.BoneMatrices[0]),
                                                  static_cast<int>(Item.BoneMatrices.size()));
            }
            Item.Mesh->Draw();
        }
    }

    ShadowMap.End(FbWidth, FbHeight, ColorRestoreFbo());
    PassTimers.End(FGPUPassTimer::EPass::Shadow);
}

void FSceneRenderer::RenderPlanarReflectionPass(const ULevel& Level, const UCameraComponent& Camera, float PlaneY) {
    const int ReflW = std::max(
        1, static_cast<int>(std::lround(static_cast<float>(FbWidth) * PlanarReflectionScale)));
    const int ReflH = std::max(
        1, static_cast<int>(std::lround(static_cast<float>(FbHeight) * PlanarReflectionScale)));
    if (!PlanarReflection.EnsureSize(ReflW, ReflH)) {
        return;
    }

    PassTimers.Begin(FGPUPassTimer::EPass::Planar);

    const glm::mat4 ReflectMat = MakeReflectMatrix(PlaneY);
    const glm::mat4 LocalView = Camera.ViewMatrix() * ReflectMat;
    const glm::mat4 LocalProjection = Camera.ProjectionMatrix();
    const glm::mat4 LocalViewProjection = LocalProjection * LocalView;
    const glm::vec3 Eye = Camera.GetCameraLocation();
    const glm::vec3 ReflectedEye{Eye.x, (2.0f * PlaneY) - Eye.y, Eye.z};

    FFrustum ReflectedFrustum;
    ReflectedFrustum.ExtractFromViewProjection(LocalViewProjection);

    // Identity light space — reflection pass skips shadows (cheaper mirror).
    const glm::mat4 LightSpace(1.0f);

    PlanarReflection.Begin();
    glEnable(GL_CLIP_DISTANCE0);
    SetClipPlane(true, glm::vec4{0.0f, 1.0f, 0.0f, -PlaneY});

    if (LitShader.Valid()) {
        UpdateCameraUbo(LocalView, LocalProjection, ReflectedEye);
        UpdateLightsUbo(Level);
        LitShader.Bind();
        BindEnvironment(Level);
        BindShadowResources(false);
        BindPlanarReflection(false, glm::mat4{1.0f});
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
    for (const UStaticMeshComponent& Object : Level.GetStaticMeshes()) {
        if (Object.bHidden || Object.Mesh == nullptr || !Object.Mesh->Valid()) {
            continue;
        }

        const FBox WorldBox = WorldAabbFromObject(Object);
        if (!ReflectedFrustum.IntersectsAabb(WorldBox)) {
            ++FrameStats.PlanarCulled;
            continue;
        }

        const std::size_t SubCount = Object.SubMeshCount();
        for (std::size_t S = 0; S < SubCount; ++S) {
            const FMaterial& Mat = Object.MaterialForSubMesh(S);
            if (Mat.bPlanarMirror || Mat.IsTransparent()) {
                continue;
            }
            const bool bLit = Mat.Shading == EMaterialShadingModel::BlinnPhong;
            FShader& Shader = bLit ? LitShader : UnlitShader;
            if (!Shader.Valid()) {
                continue;
            }
            Shader.Bind();
            if (bLit) {
                if (!bLitGlobalsBound) {
                    BindEnvironment(Level);
                    BindShadowResources(false);
                    BindPlanarReflection(false, glm::mat4{1.0f});
                    bLitGlobalsBound = true;
                }
                DrawSubMesh(Shader, Object, S, Mat, LocalView, LocalProjection, LightSpace, CheapLit);
            } else {
                bLitGlobalsBound = false;
                DrawSubMesh(Shader, Object, S, Mat, LocalView, LocalProjection, LightSpace, UnlitOpts);
            }
        }
    }

    DrawSkybox(Level, LocalView, LocalProjection);

    // Characters are queued before DrawScene — include them in the mirror (clip + reflected frustum).
    DrawQueuedSkeletal(Level, LocalView, LocalProjection, LightSpace, false, 0.0f, &ReflectedFrustum, true);

    SetClipPlane(false, glm::vec4{0.0f, 1.0f, 0.0f, 0.0f});
    glDisable(GL_CLIP_DISTANCE0);
    PlanarReflection.End(FbWidth, FbHeight, ColorRestoreFbo());
    PassTimers.End(FGPUPassTimer::EPass::Planar);
}

void FSceneRenderer::DrawSkybox(const ULevel& Level, const glm::mat4& InView,
                          const glm::mat4& InProjection) const {
    if (Level.GetEnvironment() == nullptr || !Level.GetEnvironment()->Valid() || !SkyboxShader.Valid() ||
        SkyboxMesh == nullptr || !SkyboxMesh->Valid()) {
        return;
    }

    glDepthFunc(GL_LEQUAL);
    glCullFace(GL_FRONT);
    SkyboxShader.Bind();
    SkyboxShader.SetMat4("uView", glm::value_ptr(InView));
    SkyboxShader.SetMat4("uProjection", glm::value_ptr(InProjection));
    SkyboxShader.SetInt("uEnvMap", 0);
    SkyboxShader.SetFloat("uEnvExposure", Level.GetEnvironmentExposure());
    Level.GetEnvironment()->Bind(0);
    SkyboxMesh->Draw();
    glCullFace(GL_BACK);
    glDepthFunc(GL_LESS);
}

void FSceneRenderer::DrawSubMesh(const FShader& Shader, const UStaticMeshComponent& Object,
                           std::size_t InSubMeshIndex, const FMaterial& InMaterial,
                           const glm::mat4& InView, const glm::mat4& InProjection,
                           const glm::mat4& LightSpace, const FDrawOptions& Options) const {
    if (Object.Mesh == nullptr || !Object.Mesh->Valid()) {
        return;
    }

    const glm::mat4 LocalModel = Object.EffectiveModelMatrix();
    const glm::mat4 Mvp = InProjection * InView * LocalModel;

    Shader.SetMat4("uMVP", glm::value_ptr(Mvp));
    Shader.SetMat4("uModel", glm::value_ptr(LocalModel));
    Shader.SetVec3("uAlbedo", InMaterial.Albedo.x, InMaterial.Albedo.y, InMaterial.Albedo.z);
    Shader.SetFloat("uAlpha", InMaterial.Alpha);
    Shader.SetVec2("uUvScale", InMaterial.UvScale.x, InMaterial.UvScale.y);
    Shader.SetInt("uAlbedoMap", 0);

    if (Options.bLitPass) {
        const glm::mat3 Normal = glm::transpose(glm::inverse(glm::mat3(LocalModel)));
        Shader.SetMat3("uNormalMatrix", glm::value_ptr(Normal));
        Shader.SetFloat("uShininess", InMaterial.Shininess);
        Shader.SetFloat("uRoughness", InMaterial.Roughness);
        Shader.SetVec3("uSpecular", InMaterial.Specular.x, InMaterial.Specular.y, InMaterial.Specular.z);
        Shader.SetFloat("uMetallic", InMaterial.Metallic);
        Shader.SetMat4("uLightSpaceMatrix", glm::value_ptr(LightSpace));
        Shader.SetInt("uNormalMap", 2);
        Shader.SetInt("uLightmap", 6);
        const bool bUseLm = Object.UsesLightmap();
        Shader.SetInt("uUseLightmap", bUseLm ? 1 : 0);
        if (Options.bBindSharedLitTextures) {
            Shader.SetInt("uShadowMap", 1);
            Shader.SetInt("uReceiveShadows",
                          (Options.bReceiveShadows && ShadowMap.Valid() && !bUseLm) ? 1 : 0);
            Shader.SetFloat("uShadowTexelSize", ShadowMap.Valid()
                                                    ? 1.0f / static_cast<float>(ShadowMap.GetSize())
                                                    : 0.0f);
            if (Options.bReceiveShadows && ShadowMap.Valid()) {
                ShadowMap.BindDepthTexture(1);
            }
        }
    }

    const UTexture2D* Albedo = (InMaterial.AlbedoMap && InMaterial.AlbedoMap->Valid())
                                ? InMaterial.AlbedoMap.get()
                                : WhiteTexture.get();
    Albedo->Bind(0);

    if (Options.bLitPass) {
        const UTexture2D* Normals =
            (Options.bUseNormalMaps && InMaterial.NormalMap && InMaterial.NormalMap->Valid())
                ? InMaterial.NormalMap.get()
                : FlatNormalTexture.get();
        Normals->Bind(2);
        if (Object.UsesLightmap()) {
            Object.Lightmap->Bind(6);
        } else if (WhiteTexture != nullptr) {
            WhiteTexture->Bind(6);
        }
    }

    Object.Mesh->DrawSubMesh(InSubMeshIndex);
}

void FSceneRenderer::DrawScene(const ULevel& Level, const UCameraComponent& Camera) {
    FrameStats = {};
    PassTimers.BeginFrame();
    FrameStats.ShadowMs = PassTimers.Milliseconds(FGPUPassTimer::EPass::Shadow);
    FrameStats.PlanarMs = PassTimers.Milliseconds(FGPUPassTimer::EPass::Planar);
    FrameStats.ColorMs = PassTimers.Milliseconds(FGPUPassTimer::EPass::Color);
    FrameStats.SsaoMs = PassTimers.Milliseconds(FGPUPassTimer::EPass::Ssao);
    FrameStats.PostMs = PassTimers.Milliseconds(FGPUPassTimer::EPass::Post);

    // Editor Player Collision / similar: clear + overlay only (keep timer pairs intact).
    if (!bSceneGeometryEnabled) {
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

        if (OverlayDebugDraw.IsValid() && !OverlayDebugDraw.IsEmpty()) {
            glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
            glViewport(0, 0, FbWidth, FbHeight);
            const glm::mat4 Vp = Camera.ProjectionMatrix() * Camera.ViewMatrix();
            OverlayDebugDraw.Flush(Vp);
        }
        OverlayDebugDraw.Clear();
        SkeletalDraws.clear();
        StaticDraws.clear();
        return;
    }

    const bool bPostOn = Post.bEnabled && SceneColor.Valid();

    const glm::mat4 LocalView = Camera.ViewMatrix();
    const glm::mat4 LocalProjection = Camera.ProjectionMatrix();
    const glm::mat4 LocalViewProjection = LocalProjection * LocalView;
    const glm::vec3 LocalCameraPos = Camera.GetCameraLocation();

    FFrustum CameraFrustum;
    CameraFrustum.ExtractFromViewProjection(LocalViewProjection);

    glm::mat4 LightSpace(1.0f);
    // Shadow map follows directional light 0 when castShadows; extras are lighting-only.
    const bool bCastDirShadows =
        !Level.GetDirectionalLights().empty() && Level.GetDirectionalLights().front().bCastShadows;
    if (bCastDirShadows) {
        const glm::vec3 LightDir = Level.GetDirectionalLights().front().GetDirection();
        glm::vec3 WorldMin;
        glm::vec3 WorldMax;
        if (ComputeCasterAabb(Level, WorldMin, WorldMax)) {
            LightSpace = FShadowMap::FitLightSpaceMatrix(LightDir, WorldMin, WorldMax, 0.75f);
        } else {
            LightSpace = FShadowMap::FitLightSpaceMatrix(LightDir, {-3, 0, -3}, {3, 2, 3}, 0.75f);
        }
        RenderShadowPass(Level, LightSpace);
    } else {
        // Keep timer queries paired every frame (double-buffered HUD).
        PassTimers.Begin(FGPUPassTimer::EPass::Shadow);
        PassTimers.End(FGPUPassTimer::EPass::Shadow);
    }

    // Optional horizontal planar mirror (first material with planarMirror=true).
    bool bHasPlanarMirror = false;
    float MirrorPlaneY = 0.0f;
    glm::mat4 ReflectionViewProj(1.0f);
    for (const UStaticMeshComponent& Object : Level.GetStaticMeshes()) {
        const std::size_t SubCount = Object.SubMeshCount();
        for (std::size_t S = 0; S < SubCount; ++S) {
            if (Object.MaterialForSubMesh(S).bPlanarMirror) {
                bHasPlanarMirror = true;
                // Reflect about the visible top of the mirror mesh (not actor origin).
                MirrorPlaneY = WorldAabbFromObject(Object).Max.y;
                break;
            }
        }
        if (bHasPlanarMirror) {
            break;
        }
    }
    if (bHasPlanarMirror) {
        RenderPlanarReflectionPass(Level, Camera, MirrorPlaneY);
        ReflectionViewProj = LocalProjection * Camera.ViewMatrix() * MakeReflectMatrix(MirrorPlaneY);
    } else {
        PassTimers.Begin(FGPUPassTimer::EPass::Planar);
        PassTimers.End(FGPUPassTimer::EPass::Planar);
    }

    std::vector<FDrawItem> Opaque;
    std::vector<FDrawItem> Transparent;
    Opaque.reserve(Level.GetStaticMeshes().size());
    Transparent.reserve(Level.GetStaticMeshes().size());

    for (std::size_t I = 0; I < Level.GetStaticMeshes().size(); ++I) {
        const UStaticMeshComponent& Object = Level.GetStaticMeshes()[I];
        if (Object.bHidden || Object.Mesh == nullptr || !Object.Mesh->Valid()) {
            continue;
        }
        ++FrameStats.ObjectsTotal;

        const FBox WorldBox = WorldAabbFromObject(Object);
        if (!CameraFrustum.IntersectsAabb(WorldBox)) {
            ++FrameStats.ObjectsCulled;
            continue;
        }
        ++FrameStats.ObjectsVisible;

        const float LocalSortKey = DistanceSqToCamera(Object, LocalCameraPos);
        const std::size_t SubCount = Object.SubMeshCount();
        FrameStats.DrawsSubmitted += static_cast<int>(SubCount);
        FrameStats.TrianglesSubmitted += Object.Mesh->TriangleCount();

        for (std::size_t S = 0; S < SubCount; ++S) {
            const FMaterial& Mat = Object.MaterialForSubMesh(S);
            const FDrawItem Item{I, S, LocalSortKey};
            if (Mat.IsTransparent()) {
                Transparent.push_back(Item);
            } else {
                Opaque.push_back(Item);
            }
        }
    }

    std::sort(Opaque.begin(), Opaque.end(),
              [](const FDrawItem& A, const FDrawItem& B) { return A.SortKey < B.SortKey; });
    std::sort(Transparent.begin(), Transparent.end(),
              [](const FDrawItem& A, const FDrawItem& B) { return A.SortKey > B.SortKey; });

    PassTimers.Begin(FGPUPassTimer::EPass::Color);

    if (bPostOn) {
        SceneColor.Begin();
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
        glViewport(0, 0, FbWidth, FbHeight);
    }

    const float ShadowSourceAngle = bCastDirShadows ? Level.GetDirectionalLights().front().SourceAngle
                                                   : DefaultLightSourceAngleDegrees;

    // Optional early-Z: write opaque depth before expensive lit shading.
    if (Post.bEarlyZ && UnlitShader.Valid() && !Opaque.empty()) {
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
        for (const FDrawItem& Item : Opaque) {
            const UStaticMeshComponent& Object = Level.GetStaticMeshes()[Item.ObjectIndex];
            const FMaterial& Mat = Object.MaterialForSubMesh(Item.SubMeshIndex);
            if (Mat.Shading == EMaterialShadingModel::Unlit) {
                continue;
            }
            const glm::mat4 LocalModel = Object.EffectiveModelMatrix();
            const glm::mat4 Mvp = LocalProjection * LocalView * LocalModel;
            UnlitShader.SetMat4("uMVP", glm::value_ptr(Mvp));
            UnlitShader.SetMat4("uModel", glm::value_ptr(LocalModel));
            UnlitShader.SetVec3("uAlbedo", 1.0f, 1.0f, 1.0f);
            Object.Mesh->DrawSubMesh(Item.SubMeshIndex);
        }
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthFunc(GL_LEQUAL);
    }

    if (LitShader.Valid()) {
        UpdateCameraUbo(Camera);
        UpdateLightsUbo(Level);
        LitShader.Bind();
        BindEnvironment(Level);
        BindShadowResources(bCastDirShadows, ShadowSourceAngle);
        SetClipPlane(false, glm::vec4{0.0f, 1.0f, 0.0f, 0.0f});
        BindPlanarReflection(false, ReflectionViewProj);
        if (bHasPlanarMirror && PlanarReflection.Valid()) {
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

    auto DrawList = [&](const std::vector<FDrawItem>& Items, bool bTransparentPass) {
        bool bLitGlobalsBound = LitShader.Valid();
        bool bMirrorEnabled = false;

        for (const FDrawItem& Item : Items) {
            const UStaticMeshComponent& Object = Level.GetStaticMeshes()[Item.ObjectIndex];
            const FMaterial& Mat = Object.MaterialForSubMesh(Item.SubMeshIndex);
            const bool bLit = Mat.Shading == EMaterialShadingModel::BlinnPhong;
            FShader& Shader = bLit ? LitShader : UnlitShader;
            if (!Shader.Valid()) {
                continue;
            }

            if (bTransparentPass) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            } else {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
            }

            Shader.Bind();
            if (bLit) {
                if (!bLitGlobalsBound) {
                    BindEnvironment(Level);
                    BindShadowResources(bCastDirShadows, ShadowSourceAngle);
                    if (bHasPlanarMirror && PlanarReflection.Valid()) {
                        PlanarReflection.BindColorTexture(5);
                    }
                    bLitGlobalsBound = true;
                    bMirrorEnabled = false;
                }
                const bool bUseMirror =
                    bHasPlanarMirror && Mat.bPlanarMirror && PlanarReflection.Valid();
                if (bUseMirror != bMirrorEnabled) {
                    BindPlanarReflection(bUseMirror, ReflectionViewProj);
                    bMirrorEnabled = bUseMirror;
                }
                DrawSubMesh(Shader, Object, Item.SubMeshIndex, Mat, LocalView, LocalProjection, LightSpace,
                            LitOpts);
            } else {
                bLitGlobalsBound = false;
                bMirrorEnabled = false;
                DrawSubMesh(Shader, Object, Item.SubMeshIndex, Mat, LocalView, LocalProjection, LightSpace,
                            UnlitOpts);
            }
        }

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    };

    DrawList(Opaque, false);
    DrawQueuedSkeletal(Level, LocalView, LocalProjection, LightSpace, bCastDirShadows, ShadowSourceAngle,
                       &CameraFrustum);
    DrawQueuedStatic(Level, LocalView, LocalProjection, LightSpace, bCastDirShadows, ShadowSourceAngle);
    // Skybox before transparent so glass can blend over the environment.
    DrawSkybox(Level, LocalView, LocalProjection);
    // Skybox rebinds cubemap on unit 0; restore lit env units before transparent lit draws.
    if (LitShader.Valid()) {
        LitShader.Bind();
        BindEnvironment(Level);
        BindShadowResources(bCastDirShadows, ShadowSourceAngle);
        if (bHasPlanarMirror && PlanarReflection.Valid()) {
            PlanarReflection.BindColorTexture(5);
        }
        BindPlanarReflection(false, ReflectionViewProj);
    }
    DrawList(Transparent, true);

    if (Post.bEarlyZ) {
        glDepthFunc(GL_LESS);
    }

    PassTimers.End(FGPUPassTimer::EPass::Color);

    // Debug into the color target (scene HDR or backbuffer) so depth occlusion stays correct.
    DrawDebug(Level, Camera, LightSpace, bCastDirShadows);

    if (bPostOn) {
        RenderPostStack(Level, Camera);
    } else {
        PassTimers.Begin(FGPUPassTimer::EPass::Ssao);
        PassTimers.End(FGPUPassTimer::EPass::Ssao);
        PassTimers.Begin(FGPUPassTimer::EPass::Post);
        PassTimers.End(FGPUPassTimer::EPass::Post);
        glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
        glViewport(0, 0, FbWidth, FbHeight);
    }

    if (OverlayDebugDraw.IsValid() && !OverlayDebugDraw.IsEmpty()) {
        glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
        glViewport(0, 0, FbWidth, FbHeight);
        const glm::mat4 Vp = Camera.ProjectionMatrix() * Camera.ViewMatrix();
        OverlayDebugDraw.Flush(Vp);
    }
    OverlayDebugDraw.Clear();
    SkeletalDraws.clear();
    StaticDraws.clear();
}

void FSceneRenderer::RenderPostStack(const ULevel& Level, const UCameraComponent& Camera) {
    // Flow: SceneColor(+Depth) → SSAO → bilateral blur → composite+tonemap → FXAA → present
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    const bool bWantAo = Post.bAmbientOcclusion && SsaoShader.Valid() && SsaoBlurShader.Valid() &&
                        SsaoTarget.Valid() && Post.AoSampleCount > 0;
    int AoReadIndex = 1;

    PassTimers.Begin(FGPUPassTimer::EPass::Ssao);
    if (bWantAo) {
        const glm::mat4 LocalProjection = Camera.ProjectionMatrix();
        const glm::mat4 InvProjection = glm::inverse(LocalProjection);
        const int SampleCount = std::clamp(Post.AoSampleCount, 1, MaxAoSamples);

        SsaoTarget.BindWrite(0);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        SsaoShader.Bind();
        SsaoShader.SetInt("uDepth", 0);
        SsaoShader.SetInt("uNoise", 1);
        SsaoShader.SetInt("uSampleCount", SampleCount);
        SsaoShader.SetMat4("uProjection", glm::value_ptr(LocalProjection));
        SsaoShader.SetMat4("uInvProjection", glm::value_ptr(InvProjection));
        SsaoShader.SetFloat("uRadius", Post.AoRadius);
        SsaoShader.SetFloat("uBias", Post.AoBias);
        const float NoiseScaleX =
            static_cast<float>(SsaoTarget.GetWidth()) / 4.0f;
        const float NoiseScaleY =
            static_cast<float>(SsaoTarget.GetHeight()) / 4.0f;
        SsaoShader.SetVec2("uNoiseScale", NoiseScaleX, NoiseScaleY);
        for (int I = 0; I < SampleCount; ++I) {
            const glm::vec3& S = AoKernel[static_cast<std::size_t>(I)];
            const std::string Name = "uSamples[" + std::to_string(I) + "]";
            SsaoShader.SetVec3(Name.c_str(), S.x, S.y, S.z);
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
    const float Exposure = std::max(0.01f, Level.GetEnvironmentExposure() * Post.Exposure);
    const bool bWantFxaa = Post.bFxaa && FxaaShader.Valid() && LdrColor.Valid();

    if (bWantFxaa) {
        LdrColor.BindWrite();
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
        glViewport(0, 0, FbWidth, FbHeight);
    }

    if (PostCompositeShader.Valid()) {
        PostCompositeShader.Bind();
        PostCompositeShader.SetInt("uSceneColor", 0);
        PostCompositeShader.SetInt("uAo", 1);
        PostCompositeShader.SetInt("uUseAo", bWantAo ? 1 : 0);
        PostCompositeShader.SetFloat("uAoIntensity", Post.AoIntensity);
        PostCompositeShader.SetFloat("uAoPower", Post.AoPower);
        PostCompositeShader.SetFloat("uExposure", Exposure);
        SceneColor.BindColorTexture(0);
        if (bWantAo) {
            SsaoTarget.BindColorTexture(AoReadIndex, 1);
        } else if (WhiteTexture != nullptr) {
            WhiteTexture->Bind(1);
        }
        DrawFullscreenTriangle();
    }

    if (bWantFxaa) {
        glBindFramebuffer(GL_FRAMEBUFFER, DrawTargetFbo);
        glViewport(0, 0, FbWidth, FbHeight);
        FxaaShader.Bind();
        FxaaShader.SetInt("uColor", 0);
        FxaaShader.SetVec2("uInvResolution", 1.0f / static_cast<float>(FbWidth),
                            1.0f / static_cast<float>(FbHeight));
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

void FSceneRenderer::DrawQueuedSkeletal(const ULevel& Level, const glm::mat4& InView,
                                  const glm::mat4& InProjection, const glm::mat4& LightSpace,
                                  bool bInReceiveShadows, float ShadowSourceAngle,
                                  const FFrustum* CameraFrustum, bool bUseWorldClipPlane) {
    if (!SkinnedLitShader.Valid() || SkeletalDraws.empty() || WhiteTexture == nullptr) {
        return;
    }

    SkinnedLitShader.Bind();

    const bool bHasEnv = Level.GetEnvironment() != nullptr && Level.GetEnvironment()->Valid();
    const bool bHasIrr = bHasEnv && Level.GetEnvironment()->HasIrradiance();
    SkinnedLitShader.SetInt("uHasEnvMap", bHasEnv ? 1 : 0);
    SkinnedLitShader.SetInt("uHasIrradiance", bHasIrr ? 1 : 0);
    SkinnedLitShader.SetFloat("uEnvExposure", Level.GetEnvironmentExposure());
    SkinnedLitShader.SetFloat("uEnvMaxLod", bHasEnv ? Level.GetEnvironment()->MaxLod() : 0.0f);
    SkinnedLitShader.SetInt("uEnvMap", 3);
    SkinnedLitShader.SetInt("uIrradianceMap", 4);
    if (bHasEnv) {
        Level.GetEnvironment()->Bind(3);
    }
    if (bHasIrr) {
        Level.GetEnvironment()->BindIrradiance(4);
    }

    float Texel = ShadowMap.Valid() ? 1.0f / static_cast<float>(ShadowMap.GetSize()) : 0.0f;
    if (bInReceiveShadows && Texel > 0.0f) {
        const float Soft =
            std::clamp(ShadowSourceAngle / DefaultLightSourceAngleDegrees, 0.25f, 16.0f);
        Texel *= Soft;
    }
    SkinnedLitShader.SetInt("uShadowMap", 1);
    SkinnedLitShader.SetInt("uReceiveShadows", (bInReceiveShadows && ShadowMap.Valid()) ? 1 : 0);
    SkinnedLitShader.SetFloat("uShadowTexelSize", Texel);
    if (bInReceiveShadows && ShadowMap.Valid()) {
        ShadowMap.BindDepthTexture(1);
    }

    SkinnedLitShader.SetInt("uHasPlanarReflection", 0);
    if (!bUseWorldClipPlane) {
        SkinnedLitShader.SetInt("uUseClipPlane", 0);
        SkinnedLitShader.SetVec4("uClipPlane", 0.0f, 1.0f, 0.0f, 0.0f);
    }

    for (const FSkeletalDrawItem& Item : SkeletalDraws) {
        if (Item.Mesh == nullptr || !Item.Mesh->Valid()) {
            continue;
        }

        ++FrameStats.ObjectsTotal;
        if (CameraFrustum != nullptr) {
            const FBox WorldBox =
                FBox::FromLocalTransformed(Item.Mesh->GetLocalMin(), Item.Mesh->GetLocalMax(), Item.Model);
            if (!CameraFrustum->IntersectsAabb(WorldBox)) {
                ++FrameStats.ObjectsCulled;
                continue;
            }
        }
        ++FrameStats.ObjectsVisible;

        const FMaterial& LocalMaterial = Item.Mesh->GetMaterial();
        const glm::mat4& LocalModel = Item.Model;
        const glm::mat4 Mvp = InProjection * InView * LocalModel;
        const glm::mat3 Model3(LocalModel);
        const float Det = glm::determinant(Model3);
        const glm::mat3 Normal =
            (std::abs(Det) < 1.0e-12f) ? glm::mat3(1.0f) : glm::transpose(glm::inverse(Model3));

        SkinnedLitShader.SetMat4("uMVP", glm::value_ptr(Mvp));
        SkinnedLitShader.SetMat4("uModel", glm::value_ptr(LocalModel));
        SkinnedLitShader.SetMat3("uNormalMatrix", glm::value_ptr(Normal));
        SkinnedLitShader.SetMat4("uLightSpaceMatrix", glm::value_ptr(LightSpace));
        SkinnedLitShader.SetVec3("uAlbedo", LocalMaterial.Albedo.x, LocalMaterial.Albedo.y,
                                  LocalMaterial.Albedo.z);
        SkinnedLitShader.SetFloat("uAlpha", LocalMaterial.Alpha);
        SkinnedLitShader.SetVec2("uUvScale", LocalMaterial.UvScale.x, LocalMaterial.UvScale.y);
        SkinnedLitShader.SetFloat("uShininess", LocalMaterial.Shininess);
        SkinnedLitShader.SetFloat("uRoughness", LocalMaterial.Roughness);
        SkinnedLitShader.SetVec3("uSpecular", LocalMaterial.Specular.x, LocalMaterial.Specular.y,
                                  LocalMaterial.Specular.z);
        SkinnedLitShader.SetFloat("uMetallic", LocalMaterial.Metallic);
        SkinnedLitShader.SetInt("uAlbedoMap", 0);
        SkinnedLitShader.SetInt("uNormalMap", 2);
        // Keep pass-level shadow uniforms (valid map + soft texel); do not overwrite per draw.

        if (!Item.BoneMatrices.empty()) {
            SkinnedLitShader.SetMat4Array("uBones", glm::value_ptr(Item.BoneMatrices[0]),
                                           static_cast<int>(Item.BoneMatrices.size()));
        }

        const UTexture2D* Albedo = LocalMaterial.AlbedoMap && LocalMaterial.AlbedoMap->Valid()
                                    ? LocalMaterial.AlbedoMap.get()
                                    : WhiteTexture.get();
        Albedo->Bind(0);
        const UTexture2D* Normals = LocalMaterial.NormalMap && LocalMaterial.NormalMap->Valid()
                                     ? LocalMaterial.NormalMap.get()
                                     : FlatNormalTexture.get();
        Normals->Bind(2);

        Item.Mesh->Draw();
        ++FrameStats.DrawsSubmitted;
        FrameStats.TrianglesSubmitted += Item.Mesh->TriangleCount();
    }
}

void FSceneRenderer::DrawQueuedStatic(const ULevel& Level, const glm::mat4& InView,
                                const glm::mat4& InProjection, const glm::mat4& LightSpace,
                                bool bInReceiveShadows, float ShadowSourceAngle) {
    if (StaticDraws.empty() || WhiteTexture == nullptr || !UnlitShader.Valid()) {
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

    for (const FStaticDrawItem& Item : StaticDraws) {
        if (Item.Mesh == nullptr || !Item.Mesh->Valid()) {
            continue;
        }

        const FMaterial& LocalMaterial = Item.Material;
        const glm::mat4& LocalModel = Item.Model;
        const glm::mat4 Mvp = InProjection * InView * LocalModel;

        UnlitShader.SetMat4("uMVP", glm::value_ptr(Mvp));
        UnlitShader.SetMat4("uModel", glm::value_ptr(LocalModel));
        UnlitShader.SetVec3("uAlbedo", LocalMaterial.Albedo.x, LocalMaterial.Albedo.y, LocalMaterial.Albedo.z);

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

void FSceneRenderer::DrawDebug(const ULevel& Level, const UCameraComponent& Camera, const glm::mat4& LightSpace,
                         bool bHasLightSpace) {
    if (!bDebugDrawEnabled || !DebugDraw.IsValid()) {
        return;
    }

    DebugDraw.Clear();

    constexpr glm::vec3 AabbColor{0.2f, 0.95f, 0.35f};
    constexpr glm::vec3 HiddenAabbColor{0.95f, 0.35f, 0.85f}; // BlockingVolume / hidden
    constexpr glm::vec3 FrustumColor{1.0f, 0.85f, 0.15f};

    for (const UStaticMeshComponent& Object : Level.GetStaticMeshes()) {
        if (Object.Mesh == nullptr || !Object.Mesh->Valid()) {
            continue;
        }
        const FBox Box = WorldAabbFromObject(Object);
        DebugDraw.AddAabb(Box.Min, Box.Max, Object.bHidden ? HiddenAabbColor : AabbColor);
    }

    if (bHasLightSpace) {
        DebugDraw.AddLightFrustum(LightSpace, FrustumColor);
    }

    const glm::mat4 LocalViewProjection = Camera.ProjectionMatrix() * Camera.ViewMatrix();
    DebugDraw.Flush(LocalViewProjection);
}

