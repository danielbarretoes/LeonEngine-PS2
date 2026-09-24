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
#include "Renderer.h"
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace leon {
namespace {

struct DrawItem {
    std::size_t objectIndex = 0;
    std::size_t subMeshIndex = 0;
    float sortKey = 0.0f;
};

// std140 layouts — must match blinn_phong.frag uniform blocks.
struct alignas(16) CameraBlock {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 viewProjection{1.0f};
    glm::vec4 cameraPos{0.0f}; // xyz
};

struct alignas(16) LightsBlock {
    int dirCount = 0;
    int pointCount = 0;
    int pad0 = 0;
    int pad1 = 0;
    glm::vec4 dirDirections[kMaxDirectionalLights]{};
    glm::vec4 dirColors[kMaxDirectionalLights]{};
    glm::vec4 pointPositions[kMaxPointLights]{};
    glm::vec4 pointColors[kMaxPointLights]{};
    glm::vec4 pointRanges[kMaxPointLights]{}; // .x = range
};

static_assert(sizeof(CameraBlock) == 208, "CameraBlock must match std140 Camera UBO");
static_assert(sizeof(LightsBlock) == 272, "LightsBlock must match std140 Lights UBO");

float distanceSqToCamera(const StaticMeshComponent& object, const glm::vec3& cameraPos) {
    const Aabb box = Aabb::fromLocalTransformed(object.mesh->LocalMin(), object.mesh->LocalMax(),
                                                object.EffectiveModelMatrix());
    const glm::vec3 center = (box.min + box.max) * 0.5f;
    const glm::vec3 d = center - cameraPos;
    return glm::dot(d, d);
}

Aabb worldAabbFromObject(const StaticMeshComponent& object) {
    return Aabb::fromLocalTransformed(object.mesh->LocalMin(), object.mesh->LocalMax(),
                                      object.EffectiveModelMatrix());
}

void expandWorldAabbFromObject(const StaticMeshComponent& object, glm::vec3& worldMin,
                               glm::vec3& worldMax) {
    const Aabb box = worldAabbFromObject(object);
    worldMin = glm::min(worldMin, box.min);
    worldMax = glm::max(worldMax, box.max);
}

void snapAabbOutward(glm::vec3& worldMin, glm::vec3& worldMax, float step) {
    if (step <= 0.0f) {
        return;
    }
    worldMin = glm::floor(worldMin / step) * step;
    worldMax = glm::ceil(worldMax / step) * step;
}

bool computeCasterAabb(const Level& level, glm::vec3& worldMin, glm::vec3& worldMax) {
    worldMin = glm::vec3(std::numeric_limits<float>::max());
    worldMax = glm::vec3(std::numeric_limits<float>::lowest());
    bool any = false;
    for (const StaticMeshComponent& object : level.StaticMeshes()) {
        if (!object.isShadowCaster()) {
            continue;
        }
        expandWorldAabbFromObject(object, worldMin, worldMax);
        any = true;
    }
    if (any) {
        // Quantize so spinning casters don't retune the ortho light every frame (shadow flicker).
        snapAabbOutward(worldMin, worldMax, 0.5f);
    }
    return any;
}

glm::mat4 makeReflectMatrix(float planeY) {
    glm::mat4 reflectMat(1.0f);
    reflectMat[1][1] = -1.0f;
    reflectMat[3][1] = 2.0f * planeY;
    return reflectMat;
}

void buildAoKernel(std::array<glm::vec3, Renderer::kMaxAoSamples>& kernel) {
    std::mt19937 rng(1337u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    for (int i = 0; i < Renderer::kMaxAoSamples; ++i) {
        glm::vec3 sample{unit(rng) * 2.0f - 1.0f, unit(rng) * 2.0f - 1.0f, unit(rng)};
        sample = glm::normalize(sample);
        sample *= unit(rng);
        float scale = static_cast<float>(i) / static_cast<float>(Renderer::kMaxAoSamples);
        scale = 0.1f + 0.9f * (scale * scale);
        kernel[static_cast<std::size_t>(i)] = sample * scale;
    }
}

unsigned int createAoNoiseTexture() {
    std::mt19937 rng(42u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::array<glm::vec3, 16> noise{};
    for (glm::vec3& n : noise) {
        n = glm::vec3{unit(rng) * 2.0f - 1.0f, unit(rng) * 2.0f - 1.0f, 0.0f};
    }
    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return tex;
}

} // namespace

bool Renderer::bindLitUbos() const {
    const bool litOk = litShader_.BindUniformBlock("Camera", kCameraUboBinding) &&
                       litShader_.BindUniformBlock("Lights", kLightsUboBinding);
    if (!litOk) {
        return false;
    }
    if (skinnedLitShader_.Valid()) {
        return skinnedLitShader_.BindUniformBlock("Camera", kCameraUboBinding) &&
               skinnedLitShader_.BindUniformBlock("Lights", kLightsUboBinding);
    }
    return true;
}

bool Renderer::Initialize(const std::string& shaderDirectory) {
    shaderDirectory_ = shaderDirectory;
    namespace fs = std::filesystem;
    auto shaderFile = [&](const char* name) {
        const fs::path underDir = fs::path(shaderDirectory) / name;
        if (fs::exists(underDir)) {
            return underDir.string();
        }
        // Fallback: executable-relative assets (POST_BUILD copy / packaged layout).
        return ResolveAssetPath((fs::path("assets/Shaders") / name).string());
    };
    if (!litShader_.LoadFromFiles(shaderFile("blinn_phong.vert"), shaderFile("blinn_phong.frag"))) {
        std::cerr << "Failed to load lit shaders from " << shaderDirectory << '\n';
        return false;
    }
    if (!skinnedLitShader_.LoadFromFiles(shaderFile("skinned_lit.vert"),
                                         shaderFile("blinn_phong.frag"))) {
        std::cerr << "Failed to load skinned lit shaders from " << shaderDirectory << '\n';
        return false;
    }
    if (!unlitShader_.LoadFromFiles(shaderFile("unlit.vert"), shaderFile("unlit.frag"))) {
        std::cerr << "Failed to load unlit shaders from " << shaderDirectory << '\n';
        return false;
    }
    if (!shadowShader_.LoadFromFiles(shaderFile("shadow_depth.vert"),
                                     shaderFile("shadow_depth.frag"))) {
        std::cerr << "Failed to load shadow shaders from " << shaderDirectory << '\n';
        return false;
    }
    if (!skinnedShadowShader_.LoadFromFiles(shaderFile("skinned_shadow_depth.vert"),
                                            shaderFile("shadow_depth.frag"))) {
        std::cerr << "Failed to load skinned shadow shaders from " << shaderDirectory << '\n';
        return false;
    }
    if (!skyboxShader_.LoadFromFiles(shaderFile("skybox.vert"), shaderFile("skybox.frag"))) {
        std::cerr << "Failed to load skybox shaders from " << shaderDirectory << '\n';
        return false;
    }
    if (!ssaoShader_.LoadFromFiles(shaderFile("fullscreen.vert"), shaderFile("ssao.frag"))) {
        std::cerr << "Failed to load SSAO shaders from " << shaderDirectory << '\n';
        return false;
    }
    if (!ssaoBlurShader_.LoadFromFiles(shaderFile("fullscreen.vert"),
                                       shaderFile("ssao_blur.frag"))) {
        std::cerr << "Failed to load SSAO blur shaders from " << shaderDirectory << '\n';
        return false;
    }
    if (!postCompositeShader_.LoadFromFiles(shaderFile("fullscreen.vert"),
                                            shaderFile("post_composite.frag"))) {
        std::cerr << "Failed to load post composite shaders from " << shaderDirectory << '\n';
        return false;
    }
    if (!fxaaShader_.LoadFromFiles(shaderFile("fullscreen.vert"), shaderFile("fxaa.frag"))) {
        std::cerr << "Failed to load FXAA shaders from " << shaderDirectory << '\n';
        return false;
    }
    if (!debugDraw_.Initialize(shaderDirectory)) {
        return false;
    }
    if (!overlayDebugDraw_.Initialize(shaderDirectory)) {
        return false;
    }
    ApplyPostProcessQuality(post_, EPostProcessQuality::Low);
    if (!shadowMap_.Create(post_.shadowMapSize)) {
        return false;
    }
    if (!passTimers_.Create()) {
        std::cerr << "Failed to create GPU pass timers\n";
        return false;
    }
    if (!cameraUbo_.Create(sizeof(CameraBlock), kCameraUboBinding) ||
        !lightsUbo_.Create(sizeof(LightsBlock), kLightsUboBinding)) {
        std::cerr << "Failed to create camera/lights uniform buffers\n";
        return false;
    }
    if (!bindLitUbos()) {
        std::cerr << "Failed to bind lit shader UBO blocks\n";
        return false;
    }

    const std::array<unsigned char, 4> white = {255, 255, 255, 255};
    whiteTexture_ = std::make_shared<Texture>(Texture::Create(1, 1, white.data()));
    flatNormalTexture_ = std::make_shared<Texture>(Texture::CreateFlatNormal(4));
    skyboxMesh_ = std::make_shared<StaticMesh>(StaticMesh::Upload(MakeCube()));
    if (!whiteTexture_->Valid() || !flatNormalTexture_->Valid() || !skyboxMesh_->Valid()) {
        std::cerr << "Failed to create default textures/meshes\n";
        return false;
    }

    glGenVertexArrays(1, &fullscreenVao_);
    buildAoKernel(aoKernel_);
    aoNoiseTexture_ = createAoNoiseTexture();
    if (fullscreenVao_ == 0 || aoNoiseTexture_ == 0) {
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

void Renderer::Shutdown() {
    lightsUbo_.Destroy();
    cameraUbo_.Destroy();
    overlayDebugDraw_.Shutdown();
    debugDraw_.Shutdown();
    skyboxMesh_.reset();
    flatNormalTexture_.reset();
    whiteTexture_.reset();
    passTimers_.Destroy();
    ldrColor_.Destroy();
    ssaoTarget_.Destroy();
    sceneColor_.Destroy();
    planarReflection_.Destroy();
    shadowMap_.Destroy();
    if (aoNoiseTexture_ != 0) {
        glDeleteTextures(1, &aoNoiseTexture_);
        aoNoiseTexture_ = 0;
    }
    if (fullscreenVao_ != 0) {
        glDeleteVertexArrays(1, &fullscreenVao_);
        fullscreenVao_ = 0;
    }
    fxaaShader_.Destroy();
    postCompositeShader_.Destroy();
    ssaoBlurShader_.Destroy();
    ssaoShader_.Destroy();
    skyboxShader_.Destroy();
    skinnedShadowShader_.Destroy();
    shadowShader_.Destroy();
    unlitShader_.Destroy();
    skinnedLitShader_.Destroy();
    litShader_.Destroy();
    skeletalDraws_.clear();
    staticDraws_.clear();
    shaderDirectory_.clear();
}

EShaderReloadResult Renderer::ReloadShaders(bool force) {
    EShaderReloadResult result = EShaderReloadResult::Unchanged;
    const Shader::AcceptFn litAccept = [this]() { return bindLitUbos(); };

    auto tryReload = [&](Shader& shader, const Shader::AcceptFn& accept = {}) {
        const EShaderReloadResult r =
            force ? shader.ForceReloadFromDisk(accept) : shader.ReloadFromDiskIfChanged(accept);
        result = MergeShaderReload(result, r);
        return r != EShaderReloadResult::Failed || shader.Valid();
    };

    if (!tryReload(litShader_, litAccept) || !tryReload(skinnedLitShader_, litAccept) ||
        !tryReload(unlitShader_) || !tryReload(shadowShader_) || !tryReload(skinnedShadowShader_) ||
        !tryReload(skyboxShader_) || !tryReload(ssaoShader_) || !tryReload(ssaoBlurShader_) ||
        !tryReload(postCompositeShader_) || !tryReload(fxaaShader_)) {
        return EShaderReloadResult::Failed;
    }
    result = MergeShaderReload(result, debugDraw_.ReloadShader(force));
    result = MergeShaderReload(result, overlayDebugDraw_.ReloadShader(force));
    return result;
}

void Renderer::BeginFrame(int framebufferWidth, int framebufferHeight) {
    fbWidth_ = framebufferWidth;
    fbHeight_ = framebufferHeight;
    ensureShadowMapSize();

    const bool postOn = post_.enabled && fbWidth_ > 0 && fbHeight_ > 0;
    if (postOn) {
        (void)sceneColor_.EnsureSize(fbWidth_, fbHeight_);
        (void)ldrColor_.EnsureSize(fbWidth_, fbHeight_);
        // Full-res SSAO: half-res undersamples 24-bit depth into visible parallel bands.
        (void)ssaoTarget_.EnsureSize(fbWidth_, fbHeight_);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, drawTargetFbo_);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::ensureShadowMapSize() {
    const int size = std::clamp(post_.shadowMapSize, 512, 4096);
    if (shadowMap_.Valid() && shadowMap_.Size() == size) {
        return;
    }
    shadowMap_.Destroy();
    (void)shadowMap_.Create(size);
}

unsigned int Renderer::colorRestoreFbo() const {
    if (post_.enabled && sceneColor_.Valid()) {
        return sceneColor_.Framebuffer();
    }
    return drawTargetFbo_;
}

void Renderer::drawFullscreenTriangle() const {
    glBindVertexArray(fullscreenVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void Renderer::SubmitSkeletalDraw(const SkeletalMesh& mesh, const glm::mat4& model,
                                  const std::vector<glm::mat4>& boneMatrices) {
    if (!mesh.Valid()) {
        return;
    }
    SkeletalDrawItem item;
    item.mesh = &mesh;
    item.model = model;
    item.boneMatrices = boneMatrices;
    if (item.boneMatrices.size() > static_cast<std::size_t>(kMaxSkinBones)) {
        item.boneMatrices.resize(static_cast<std::size_t>(kMaxSkinBones));
    }
    skeletalDraws_.push_back(std::move(item));
}

void Renderer::SubmitSkeletalDraw(const SkeletalMesh& mesh, const Transform& transform,
                                  const std::vector<glm::mat4>& boneMatrices) {
    SubmitSkeletalDraw(mesh, transform.modelMatrix(), boneMatrices);
}

void Renderer::SubmitStaticDraw(const StaticMesh& mesh, const glm::mat4& model,
                                const Material& material) {
    if (!mesh.Valid()) {
        return;
    }
    StaticDrawItem item;
    item.mesh = &mesh;
    item.model = model;
    item.material = material;
    staticDraws_.push_back(std::move(item));
}

void Renderer::ClearDebugOverlay() {
    overlayDebugDraw_.Clear();
}

void Renderer::AddDebugLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color) {
    overlayDebugDraw_.AddLine(a, b, color);
}

void Renderer::AddDebugArrow(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color) {
    overlayDebugDraw_.AddArrow(from, to, color);
}

void Renderer::AddDebugAabb(const glm::vec3& worldMin, const glm::vec3& worldMax,
                            const glm::vec3& color) {
    overlayDebugDraw_.AddAabb(worldMin, worldMax, color);
}

void Renderer::updateCameraUbo(const Camera& camera) const {
    updateCameraUbo(camera.ViewMatrix(), camera.ProjectionMatrix(), camera.GetCameraLocation());
}

void Renderer::updateCameraUbo(const glm::mat4& view, const glm::mat4& projection,
                               const glm::vec3& cameraPos) const {
    CameraBlock block{};
    block.view = view;
    block.projection = projection;
    block.viewProjection = projection * view;
    block.cameraPos = glm::vec4(cameraPos, 1.0f);
    cameraUbo_.Update(&block, sizeof(block));
}

void Renderer::updateLightsUbo(const Level& level) const {
    LightsBlock block{};
    const auto& dirs = level.DirectionalLights();
    const auto& points = level.PointLights();

    block.dirCount = std::min(static_cast<int>(dirs.size()), kMaxDirectionalLights);
    block.pointCount = std::min(static_cast<int>(points.size()), kMaxPointLights);

    for (int i = 0; i < block.dirCount; ++i) {
        const auto& light = dirs[static_cast<std::size_t>(i)];
        block.dirDirections[i] = glm::vec4(light.GetDirection(), 0.0f);
        block.dirColors[i] = glm::vec4(light.lightColor * light.intensity, 0.0f);
    }
    for (int i = 0; i < block.pointCount; ++i) {
        const auto& light = points[static_cast<std::size_t>(i)];
        block.pointPositions[i] = glm::vec4(light.transform.position, 1.0f);
        block.pointColors[i] = glm::vec4(light.lightColor * light.intensity, 0.0f);
        block.pointRanges[i] = glm::vec4(light.range, 0.0f, 0.0f, 0.0f);
    }
    lightsUbo_.Update(&block, sizeof(block));
}

void Renderer::bindEnvironment(const Level& level) const {
    const bool hasEnv = level.Environment() != nullptr && level.Environment()->Valid();
    const bool hasIrr = hasEnv && level.Environment()->HasIrradiance();
    litShader_.SetInt("uHasEnvMap", hasEnv ? 1 : 0);
    litShader_.SetInt("uHasIrradiance", hasIrr ? 1 : 0);
    litShader_.SetFloat("uEnvExposure", level.EnvironmentExposure());
    litShader_.SetFloat("uEnvMaxLod", hasEnv ? level.Environment()->MaxLod() : 0.0f);
    litShader_.SetInt("uEnvMap", 3);
    litShader_.SetInt("uIrradianceMap", 4);
    if (hasEnv) {
        level.Environment()->Bind(3);
    }
    if (hasIrr) {
        level.Environment()->BindIrradiance(4);
    }
}

void Renderer::bindShadowResources(bool receiveShadows, float sourceAngleDegrees) const {
    litShader_.SetInt("uShadowMap", 1);
    litShader_.SetInt("uReceiveShadows", (receiveShadows && shadowMap_.Valid()) ? 1 : 0);
    float texel = shadowMap_.Valid() ? 1.0f / static_cast<float>(shadowMap_.Size()) : 0.0f;
    // Source Angle softens PCF filter kernel (Unreal DirectionalLight Source Angle).
    if (receiveShadows && texel > 0.0f) {
        const float soft =
            std::clamp(sourceAngleDegrees / kDefaultLightSourceAngleDegrees, 0.25f, 16.0f);
        texel *= soft;
    }
    litShader_.SetFloat("uShadowTexelSize", texel);
    if (receiveShadows && shadowMap_.Valid()) {
        shadowMap_.BindDepthTexture(1);
    }
}

void Renderer::bindPlanarReflection(bool enabled, const glm::mat4& reflectionViewProj) const {
    litShader_.SetInt("uHasPlanarReflection", enabled ? 1 : 0);
    litShader_.SetInt("uPlanarReflection", 5);
    litShader_.SetMat4("uReflectionViewProj", glm::value_ptr(reflectionViewProj));
    if (enabled && planarReflection_.Valid()) {
        planarReflection_.BindColorTexture(5);
    }
}

void Renderer::setClipPlane(bool enabled, const glm::vec4& plane) const {
    const int use = enabled ? 1 : 0;
    if (litShader_.Valid()) {
        litShader_.Bind();
        litShader_.SetInt("uUseClipPlane", use);
        litShader_.SetVec4("uClipPlane", plane.x, plane.y, plane.z, plane.w);
    }
    if (unlitShader_.Valid()) {
        unlitShader_.Bind();
        unlitShader_.SetInt("uUseClipPlane", use);
        unlitShader_.SetVec4("uClipPlane", plane.x, plane.y, plane.z, plane.w);
    }
    if (skinnedLitShader_.Valid()) {
        skinnedLitShader_.Bind();
        skinnedLitShader_.SetInt("uUseClipPlane", use);
        skinnedLitShader_.SetVec4("uClipPlane", plane.x, plane.y, plane.z, plane.w);
    }
}

void Renderer::renderShadowPass(const Level& level, const glm::mat4& lightSpace) {
    if (!shadowMap_.Valid()) {
        return;
    }

    passTimers_.Begin(GpuPassTimer::EPass::Shadow);
    shadowMap_.Begin();

    if (shadowShader_.Valid()) {
        shadowShader_.Bind();
        for (const StaticMeshComponent& object : level.StaticMeshes()) {
            if (!object.isShadowCaster()) {
                continue;
            }
            const glm::mat4 lightMvp = lightSpace * object.EffectiveModelMatrix();
            shadowShader_.SetMat4("uLightMVP", glm::value_ptr(lightMvp));

            const std::size_t subCount = object.subMeshCount();
            for (std::size_t s = 0; s < subCount; ++s) {
                const Material& mat = object.materialForSubMesh(s);
                if (!mat.castsShadows || mat.isTransparent() || mat.shading == EShadingModel::Unlit) {
                    continue;
                }
                object.mesh->DrawSubMesh(s);
            }
        }
    }

    // Queued skeletal draws (Character meshes submitted before DrawScene).
    if (skinnedShadowShader_.Valid()) {
        skinnedShadowShader_.Bind();
        for (const SkeletalDrawItem& item : skeletalDraws_) {
            if (item.mesh == nullptr || !item.mesh->Valid()) {
                continue;
            }
            const Material& material = item.mesh->GetMaterial();
            if (!material.castsShadows || material.isTransparent() ||
                material.shading == EShadingModel::Unlit) {
                continue;
            }
            const glm::mat4 lightMvp = lightSpace * item.model;
            skinnedShadowShader_.SetMat4("uLightMVP", glm::value_ptr(lightMvp));
            if (!item.boneMatrices.empty()) {
                skinnedShadowShader_.SetMat4Array("uBones", glm::value_ptr(item.boneMatrices[0]),
                                                  static_cast<int>(item.boneMatrices.size()));
            }
            item.mesh->Draw();
        }
    }

    shadowMap_.End(fbWidth_, fbHeight_, colorRestoreFbo());
    passTimers_.End(GpuPassTimer::EPass::Shadow);
}

void Renderer::renderPlanarReflectionPass(const Level& level, const Camera& camera, float planeY) {
    const int reflW = std::max(
        1, static_cast<int>(std::lround(static_cast<float>(fbWidth_) * kPlanarReflectionScale)));
    const int reflH = std::max(
        1, static_cast<int>(std::lround(static_cast<float>(fbHeight_) * kPlanarReflectionScale)));
    if (!planarReflection_.EnsureSize(reflW, reflH)) {
        return;
    }

    passTimers_.Begin(GpuPassTimer::EPass::Planar);

    const glm::mat4 reflectMat = makeReflectMatrix(planeY);
    const glm::mat4 view = camera.ViewMatrix() * reflectMat;
    const glm::mat4 projection = camera.ProjectionMatrix();
    const glm::mat4 viewProjection = projection * view;
    const glm::vec3 eye = camera.GetCameraLocation();
    const glm::vec3 reflectedEye{eye.x, (2.0f * planeY) - eye.y, eye.z};

    Frustum reflectedFrustum;
    reflectedFrustum.extractFromViewProjection(viewProjection);

    // Identity light space — reflection pass skips shadows (cheaper mirror).
    const glm::mat4 lightSpace(1.0f);

    planarReflection_.Begin();
    glEnable(GL_CLIP_DISTANCE0);
    setClipPlane(true, glm::vec4{0.0f, 1.0f, 0.0f, -planeY});

    if (litShader_.Valid()) {
        updateCameraUbo(view, projection, reflectedEye);
        updateLightsUbo(level);
        litShader_.Bind();
        bindEnvironment(level);
        bindShadowResources(false);
        bindPlanarReflection(false, glm::mat4{1.0f});
    }

    DrawOptions cheapLit{};
    cheapLit.litPass = true;
    cheapLit.receiveShadows = false;
    cheapLit.useNormalMaps = false;
    cheapLit.bindSharedLitTextures = false;

    DrawOptions unlitOpts{};
    unlitOpts.litPass = false;
    unlitOpts.bindSharedLitTextures = false;

    bool litGlobalsBound = litShader_.Valid();
    for (const StaticMeshComponent& object : level.StaticMeshes()) {
        if (object.hidden || object.mesh == nullptr || !object.mesh->Valid()) {
            continue;
        }

        const Aabb worldBox = worldAabbFromObject(object);
        if (!reflectedFrustum.intersectsAabb(worldBox)) {
            ++frameStats_.planarCulled;
            continue;
        }

        const std::size_t subCount = object.subMeshCount();
        for (std::size_t s = 0; s < subCount; ++s) {
            const Material& mat = object.materialForSubMesh(s);
            if (mat.planarMirror || mat.isTransparent()) {
                continue;
            }
            const bool lit = mat.shading == EShadingModel::BlinnPhong;
            Shader& shader = lit ? litShader_ : unlitShader_;
            if (!shader.Valid()) {
                continue;
            }
            shader.Bind();
            if (lit) {
                if (!litGlobalsBound) {
                    bindEnvironment(level);
                    bindShadowResources(false);
                    bindPlanarReflection(false, glm::mat4{1.0f});
                    litGlobalsBound = true;
                }
                DrawSubMesh(shader, object, s, mat, view, projection, lightSpace, cheapLit);
            } else {
                litGlobalsBound = false;
                DrawSubMesh(shader, object, s, mat, view, projection, lightSpace, unlitOpts);
            }
        }
    }

    drawSkybox(level, view, projection);

    // Characters are queued before DrawScene — include them in the mirror (clip + reflected frustum).
    drawQueuedSkeletal(level, view, projection, lightSpace, false, 0.0f, &reflectedFrustum, true);

    setClipPlane(false, glm::vec4{0.0f, 1.0f, 0.0f, 0.0f});
    glDisable(GL_CLIP_DISTANCE0);
    planarReflection_.End(fbWidth_, fbHeight_, colorRestoreFbo());
    passTimers_.End(GpuPassTimer::EPass::Planar);
}

void Renderer::drawSkybox(const Level& level, const glm::mat4& view,
                          const glm::mat4& projection) const {
    if (level.Environment() == nullptr || !level.Environment()->Valid() || !skyboxShader_.Valid() ||
        skyboxMesh_ == nullptr || !skyboxMesh_->Valid()) {
        return;
    }

    glDepthFunc(GL_LEQUAL);
    glCullFace(GL_FRONT);
    skyboxShader_.Bind();
    skyboxShader_.SetMat4("uView", glm::value_ptr(view));
    skyboxShader_.SetMat4("uProjection", glm::value_ptr(projection));
    skyboxShader_.SetInt("uEnvMap", 0);
    skyboxShader_.SetFloat("uEnvExposure", level.EnvironmentExposure());
    level.Environment()->Bind(0);
    skyboxMesh_->Draw();
    glCullFace(GL_BACK);
    glDepthFunc(GL_LESS);
}

void Renderer::DrawSubMesh(const Shader& shader, const StaticMeshComponent& object,
                           std::size_t subMeshIndex, const Material& material,
                           const glm::mat4& view, const glm::mat4& projection,
                           const glm::mat4& lightSpace, const DrawOptions& options) const {
    if (object.mesh == nullptr || !object.mesh->Valid()) {
        return;
    }

    const glm::mat4 model = object.EffectiveModelMatrix();
    const glm::mat4 mvp = projection * view * model;

    shader.SetMat4("uMVP", glm::value_ptr(mvp));
    shader.SetMat4("uModel", glm::value_ptr(model));
    shader.SetVec3("uAlbedo", material.albedo.x, material.albedo.y, material.albedo.z);
    shader.SetFloat("uAlpha", material.alpha);
    shader.SetVec2("uUvScale", material.uvScale.x, material.uvScale.y);
    shader.SetInt("uAlbedoMap", 0);

    if (options.litPass) {
        const glm::mat3 normal = glm::transpose(glm::inverse(glm::mat3(model)));
        shader.SetMat3("uNormalMatrix", glm::value_ptr(normal));
        shader.SetFloat("uShininess", material.shininess);
        shader.SetFloat("uRoughness", material.roughness);
        shader.SetVec3("uSpecular", material.specular.x, material.specular.y, material.specular.z);
        shader.SetFloat("uMetallic", material.metallic);
        shader.SetMat4("uLightSpaceMatrix", glm::value_ptr(lightSpace));
        shader.SetInt("uNormalMap", 2);
        shader.SetInt("uLightmap", 6);
        const bool useLm = object.UsesLightmap();
        shader.SetInt("uUseLightmap", useLm ? 1 : 0);
        if (options.bindSharedLitTextures) {
            shader.SetInt("uShadowMap", 1);
            shader.SetInt("uReceiveShadows",
                          (options.receiveShadows && shadowMap_.Valid() && !useLm) ? 1 : 0);
            shader.SetFloat("uShadowTexelSize", shadowMap_.Valid()
                                                    ? 1.0f / static_cast<float>(shadowMap_.Size())
                                                    : 0.0f);
            if (options.receiveShadows && shadowMap_.Valid()) {
                shadowMap_.BindDepthTexture(1);
            }
        }
    }

    const Texture* albedo = (material.albedoMap && material.albedoMap->Valid())
                                ? material.albedoMap.get()
                                : whiteTexture_.get();
    albedo->Bind(0);

    if (options.litPass) {
        const Texture* normals =
            (options.useNormalMaps && material.normalMap && material.normalMap->Valid())
                ? material.normalMap.get()
                : flatNormalTexture_.get();
        normals->Bind(2);
        if (object.UsesLightmap()) {
            object.lightmap->Bind(6);
        } else if (whiteTexture_ != nullptr) {
            whiteTexture_->Bind(6);
        }
    }

    object.mesh->DrawSubMesh(subMeshIndex);
}

void Renderer::DrawScene(const Level& level, const Camera& camera) {
    frameStats_ = {};
    passTimers_.BeginFrame();
    frameStats_.shadowMs = passTimers_.Milliseconds(GpuPassTimer::EPass::Shadow);
    frameStats_.planarMs = passTimers_.Milliseconds(GpuPassTimer::EPass::Planar);
    frameStats_.colorMs = passTimers_.Milliseconds(GpuPassTimer::EPass::Color);
    frameStats_.ssaoMs = passTimers_.Milliseconds(GpuPassTimer::EPass::Ssao);
    frameStats_.postMs = passTimers_.Milliseconds(GpuPassTimer::EPass::Post);

    // Editor Player Collision / similar: clear + overlay only (keep timer pairs intact).
    if (!sceneGeometryEnabled_) {
        passTimers_.Begin(GpuPassTimer::EPass::Shadow);
        passTimers_.End(GpuPassTimer::EPass::Shadow);
        passTimers_.Begin(GpuPassTimer::EPass::Planar);
        passTimers_.End(GpuPassTimer::EPass::Planar);
        passTimers_.Begin(GpuPassTimer::EPass::Color);
        passTimers_.End(GpuPassTimer::EPass::Color);
        passTimers_.Begin(GpuPassTimer::EPass::Ssao);
        passTimers_.End(GpuPassTimer::EPass::Ssao);
        passTimers_.Begin(GpuPassTimer::EPass::Post);
        passTimers_.End(GpuPassTimer::EPass::Post);

        if (overlayDebugDraw_.IsValid() && !overlayDebugDraw_.IsEmpty()) {
            glBindFramebuffer(GL_FRAMEBUFFER, drawTargetFbo_);
            glViewport(0, 0, fbWidth_, fbHeight_);
            const glm::mat4 vp = camera.ProjectionMatrix() * camera.ViewMatrix();
            overlayDebugDraw_.Flush(vp);
        }
        overlayDebugDraw_.Clear();
        skeletalDraws_.clear();
        staticDraws_.clear();
        return;
    }

    const bool postOn = post_.enabled && sceneColor_.Valid();

    const glm::mat4 view = camera.ViewMatrix();
    const glm::mat4 projection = camera.ProjectionMatrix();
    const glm::mat4 viewProjection = projection * view;
    const glm::vec3 cameraPos = camera.GetCameraLocation();

    Frustum cameraFrustum;
    cameraFrustum.extractFromViewProjection(viewProjection);

    glm::mat4 lightSpace(1.0f);
    // Shadow map follows directional light 0 when castShadows; extras are lighting-only.
    const bool castDirShadows =
        !level.DirectionalLights().empty() && level.DirectionalLights().front().castShadows;
    if (castDirShadows) {
        const glm::vec3 lightDir = level.DirectionalLights().front().GetDirection();
        glm::vec3 worldMin;
        glm::vec3 worldMax;
        if (computeCasterAabb(level, worldMin, worldMax)) {
            lightSpace = ShadowMap::FitLightSpaceMatrix(lightDir, worldMin, worldMax, 0.75f);
        } else {
            lightSpace = ShadowMap::FitLightSpaceMatrix(lightDir, {-3, 0, -3}, {3, 2, 3}, 0.75f);
        }
        renderShadowPass(level, lightSpace);
    } else {
        // Keep timer queries paired every frame (double-buffered HUD).
        passTimers_.Begin(GpuPassTimer::EPass::Shadow);
        passTimers_.End(GpuPassTimer::EPass::Shadow);
    }

    // Optional horizontal planar mirror (first material with planarMirror=true).
    bool hasPlanarMirror = false;
    float mirrorPlaneY = 0.0f;
    glm::mat4 reflectionViewProj(1.0f);
    for (const StaticMeshComponent& object : level.StaticMeshes()) {
        const std::size_t subCount = object.subMeshCount();
        for (std::size_t s = 0; s < subCount; ++s) {
            if (object.materialForSubMesh(s).planarMirror) {
                hasPlanarMirror = true;
                // Reflect about the visible top of the mirror mesh (not actor origin).
                mirrorPlaneY = worldAabbFromObject(object).max.y;
                break;
            }
        }
        if (hasPlanarMirror) {
            break;
        }
    }
    if (hasPlanarMirror) {
        renderPlanarReflectionPass(level, camera, mirrorPlaneY);
        reflectionViewProj = projection * camera.ViewMatrix() * makeReflectMatrix(mirrorPlaneY);
    } else {
        passTimers_.Begin(GpuPassTimer::EPass::Planar);
        passTimers_.End(GpuPassTimer::EPass::Planar);
    }

    std::vector<DrawItem> opaque;
    std::vector<DrawItem> transparent;
    opaque.reserve(level.StaticMeshes().size());
    transparent.reserve(level.StaticMeshes().size());

    for (std::size_t i = 0; i < level.StaticMeshes().size(); ++i) {
        const StaticMeshComponent& object = level.StaticMeshes()[i];
        if (object.hidden || object.mesh == nullptr || !object.mesh->Valid()) {
            continue;
        }
        ++frameStats_.objectsTotal;

        const Aabb worldBox = worldAabbFromObject(object);
        if (!cameraFrustum.intersectsAabb(worldBox)) {
            ++frameStats_.objectsCulled;
            continue;
        }
        ++frameStats_.objectsVisible;

        const float sortKey = distanceSqToCamera(object, cameraPos);
        const std::size_t subCount = object.subMeshCount();
        frameStats_.drawsSubmitted += static_cast<int>(subCount);
        frameStats_.trianglesSubmitted += object.mesh->TriangleCount();

        for (std::size_t s = 0; s < subCount; ++s) {
            const Material& mat = object.materialForSubMesh(s);
            const DrawItem item{i, s, sortKey};
            if (mat.isTransparent()) {
                transparent.push_back(item);
            } else {
                opaque.push_back(item);
            }
        }
    }

    std::sort(opaque.begin(), opaque.end(),
              [](const DrawItem& a, const DrawItem& b) { return a.sortKey < b.sortKey; });
    std::sort(transparent.begin(), transparent.end(),
              [](const DrawItem& a, const DrawItem& b) { return a.sortKey > b.sortKey; });

    passTimers_.Begin(GpuPassTimer::EPass::Color);

    if (postOn) {
        sceneColor_.Begin();
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, drawTargetFbo_);
        glViewport(0, 0, fbWidth_, fbHeight_);
    }

    const float shadowSourceAngle = castDirShadows ? level.DirectionalLights().front().sourceAngle
                                                   : kDefaultLightSourceAngleDegrees;

    // Optional early-Z: write opaque depth before expensive lit shading.
    if (post_.earlyZ && unlitShader_.Valid() && !opaque.empty()) {
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
        unlitShader_.Bind();
        unlitShader_.SetInt("uUseClipPlane", 0);
        unlitShader_.SetVec4("uClipPlane", 0.0f, 1.0f, 0.0f, 0.0f);
        unlitShader_.SetInt("uAlbedoMap", 0);
        unlitShader_.SetVec2("uUvScale", 1.0f, 1.0f);
        unlitShader_.SetFloat("uAlpha", 1.0f);
        whiteTexture_->Bind(0);
        for (const DrawItem& item : opaque) {
            const StaticMeshComponent& object = level.StaticMeshes()[item.objectIndex];
            const Material& mat = object.materialForSubMesh(item.subMeshIndex);
            if (mat.shading == EShadingModel::Unlit) {
                continue;
            }
            const glm::mat4 model = object.EffectiveModelMatrix();
            const glm::mat4 mvp = projection * view * model;
            unlitShader_.SetMat4("uMVP", glm::value_ptr(mvp));
            unlitShader_.SetMat4("uModel", glm::value_ptr(model));
            unlitShader_.SetVec3("uAlbedo", 1.0f, 1.0f, 1.0f);
            object.mesh->DrawSubMesh(item.subMeshIndex);
        }
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthFunc(GL_LEQUAL);
    }

    if (litShader_.Valid()) {
        updateCameraUbo(camera);
        updateLightsUbo(level);
        litShader_.Bind();
        bindEnvironment(level);
        bindShadowResources(castDirShadows, shadowSourceAngle);
        setClipPlane(false, glm::vec4{0.0f, 1.0f, 0.0f, 0.0f});
        bindPlanarReflection(false, reflectionViewProj);
        if (hasPlanarMirror && planarReflection_.Valid()) {
            planarReflection_.BindColorTexture(5);
        }
    }

    DrawOptions litOpts{};
    litOpts.litPass = true;
    litOpts.receiveShadows = castDirShadows;
    litOpts.useNormalMaps = true;
    litOpts.bindSharedLitTextures = false;

    DrawOptions unlitOpts{};
    unlitOpts.litPass = false;
    unlitOpts.bindSharedLitTextures = false;

    auto drawList = [&](const std::vector<DrawItem>& items, bool transparentPass) {
        bool litGlobalsBound = litShader_.Valid();
        bool mirrorEnabled = false;

        for (const DrawItem& item : items) {
            const StaticMeshComponent& object = level.StaticMeshes()[item.objectIndex];
            const Material& mat = object.materialForSubMesh(item.subMeshIndex);
            const bool lit = mat.shading == EShadingModel::BlinnPhong;
            Shader& shader = lit ? litShader_ : unlitShader_;
            if (!shader.Valid()) {
                continue;
            }

            if (transparentPass) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            } else {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
            }

            shader.Bind();
            if (lit) {
                if (!litGlobalsBound) {
                    bindEnvironment(level);
                    bindShadowResources(castDirShadows, shadowSourceAngle);
                    if (hasPlanarMirror && planarReflection_.Valid()) {
                        planarReflection_.BindColorTexture(5);
                    }
                    litGlobalsBound = true;
                    mirrorEnabled = false;
                }
                const bool useMirror =
                    hasPlanarMirror && mat.planarMirror && planarReflection_.Valid();
                if (useMirror != mirrorEnabled) {
                    bindPlanarReflection(useMirror, reflectionViewProj);
                    mirrorEnabled = useMirror;
                }
                DrawSubMesh(shader, object, item.subMeshIndex, mat, view, projection, lightSpace,
                            litOpts);
            } else {
                litGlobalsBound = false;
                mirrorEnabled = false;
                DrawSubMesh(shader, object, item.subMeshIndex, mat, view, projection, lightSpace,
                            unlitOpts);
            }
        }

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    };

    drawList(opaque, false);
    drawQueuedSkeletal(level, view, projection, lightSpace, castDirShadows, shadowSourceAngle,
                       &cameraFrustum);
    drawQueuedStatic(level, view, projection, lightSpace, castDirShadows, shadowSourceAngle);
    // Skybox before transparent so glass can blend over the environment.
    drawSkybox(level, view, projection);
    // Skybox rebinds cubemap on unit 0; restore lit env units before transparent lit draws.
    if (litShader_.Valid()) {
        litShader_.Bind();
        bindEnvironment(level);
        bindShadowResources(castDirShadows, shadowSourceAngle);
        if (hasPlanarMirror && planarReflection_.Valid()) {
            planarReflection_.BindColorTexture(5);
        }
        bindPlanarReflection(false, reflectionViewProj);
    }
    drawList(transparent, true);

    if (post_.earlyZ) {
        glDepthFunc(GL_LESS);
    }

    passTimers_.End(GpuPassTimer::EPass::Color);

    // Debug into the color target (scene HDR or backbuffer) so depth occlusion stays correct.
    drawDebug(level, camera, lightSpace, castDirShadows);

    if (postOn) {
        renderPostStack(level, camera);
    } else {
        passTimers_.Begin(GpuPassTimer::EPass::Ssao);
        passTimers_.End(GpuPassTimer::EPass::Ssao);
        passTimers_.Begin(GpuPassTimer::EPass::Post);
        passTimers_.End(GpuPassTimer::EPass::Post);
        glBindFramebuffer(GL_FRAMEBUFFER, drawTargetFbo_);
        glViewport(0, 0, fbWidth_, fbHeight_);
    }

    if (overlayDebugDraw_.IsValid() && !overlayDebugDraw_.IsEmpty()) {
        glBindFramebuffer(GL_FRAMEBUFFER, drawTargetFbo_);
        glViewport(0, 0, fbWidth_, fbHeight_);
        const glm::mat4 vp = camera.ProjectionMatrix() * camera.ViewMatrix();
        overlayDebugDraw_.Flush(vp);
    }
    overlayDebugDraw_.Clear();
    skeletalDraws_.clear();
    staticDraws_.clear();
}

void Renderer::renderPostStack(const Level& level, const Camera& camera) {
    // Flow: SceneColor(+Depth) → SSAO → bilateral blur → composite+tonemap → FXAA → present
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    const bool wantAo = post_.ambientOcclusion && ssaoShader_.Valid() && ssaoBlurShader_.Valid() &&
                        ssaoTarget_.Valid() && post_.aoSampleCount > 0;
    int aoReadIndex = 1;

    passTimers_.Begin(GpuPassTimer::EPass::Ssao);
    if (wantAo) {
        const glm::mat4 projection = camera.ProjectionMatrix();
        const glm::mat4 invProjection = glm::inverse(projection);
        const int sampleCount = std::clamp(post_.aoSampleCount, 1, kMaxAoSamples);

        ssaoTarget_.BindWrite(0);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ssaoShader_.Bind();
        ssaoShader_.SetInt("uDepth", 0);
        ssaoShader_.SetInt("uNoise", 1);
        ssaoShader_.SetInt("uSampleCount", sampleCount);
        ssaoShader_.SetMat4("uProjection", glm::value_ptr(projection));
        ssaoShader_.SetMat4("uInvProjection", glm::value_ptr(invProjection));
        ssaoShader_.SetFloat("uRadius", post_.aoRadius);
        ssaoShader_.SetFloat("uBias", post_.aoBias);
        const float noiseScaleX =
            static_cast<float>(ssaoTarget_.Width()) / 4.0f;
        const float noiseScaleY =
            static_cast<float>(ssaoTarget_.Height()) / 4.0f;
        ssaoShader_.SetVec2("uNoiseScale", noiseScaleX, noiseScaleY);
        for (int i = 0; i < sampleCount; ++i) {
            const glm::vec3& s = aoKernel_[static_cast<std::size_t>(i)];
            const std::string name = "uSamples[" + std::to_string(i) + "]";
            ssaoShader_.SetVec3(name.c_str(), s.x, s.y, s.z);
        }
        sceneColor_.BindDepthTexture(0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, aoNoiseTexture_);
        drawFullscreenTriangle();

        // Horizontal then vertical spatial blur (dissolves noise without keeping depth bands).
        const float texelX = 1.0f / static_cast<float>(ssaoTarget_.Width());
        const float texelY = 1.0f / static_cast<float>(ssaoTarget_.Height());
        ssaoBlurShader_.Bind();
        ssaoBlurShader_.SetInt("uAo", 0);
        ssaoBlurShader_.SetVec2("uDirection", texelX, 0.0f);

        ssaoTarget_.BindWrite(1);
        ssaoTarget_.BindColorTexture(0, 0);
        drawFullscreenTriangle();

        ssaoTarget_.BindWrite(0);
        ssaoBlurShader_.SetVec2("uDirection", 0.0f, texelY);
        ssaoTarget_.BindColorTexture(1, 0);
        drawFullscreenTriangle();
        aoReadIndex = 0;
    }
    passTimers_.End(GpuPassTimer::EPass::Ssao);

    passTimers_.Begin(GpuPassTimer::EPass::Post);
    const float exposure = std::max(0.01f, level.EnvironmentExposure() * post_.exposure);
    const bool wantFxaa = post_.fxaa && fxaaShader_.Valid() && ldrColor_.Valid();

    if (wantFxaa) {
        ldrColor_.BindWrite();
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, drawTargetFbo_);
        glViewport(0, 0, fbWidth_, fbHeight_);
    }

    if (postCompositeShader_.Valid()) {
        postCompositeShader_.Bind();
        postCompositeShader_.SetInt("uSceneColor", 0);
        postCompositeShader_.SetInt("uAo", 1);
        postCompositeShader_.SetInt("uUseAo", wantAo ? 1 : 0);
        postCompositeShader_.SetFloat("uAoIntensity", post_.aoIntensity);
        postCompositeShader_.SetFloat("uAoPower", post_.aoPower);
        postCompositeShader_.SetFloat("uExposure", exposure);
        sceneColor_.BindColorTexture(0);
        if (wantAo) {
            ssaoTarget_.BindColorTexture(aoReadIndex, 1);
        } else if (whiteTexture_ != nullptr) {
            whiteTexture_->Bind(1);
        }
        drawFullscreenTriangle();
    }

    if (wantFxaa) {
        glBindFramebuffer(GL_FRAMEBUFFER, drawTargetFbo_);
        glViewport(0, 0, fbWidth_, fbHeight_);
        fxaaShader_.Bind();
        fxaaShader_.SetInt("uColor", 0);
        fxaaShader_.SetVec2("uInvResolution", 1.0f / static_cast<float>(fbWidth_),
                            1.0f / static_cast<float>(fbHeight_));
        ldrColor_.BindColorTexture(0);
        drawFullscreenTriangle();
    }
    passTimers_.End(GpuPassTimer::EPass::Post);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, drawTargetFbo_);
    glViewport(0, 0, fbWidth_, fbHeight_);
}

void Renderer::drawQueuedSkeletal(const Level& level, const glm::mat4& view,
                                  const glm::mat4& projection, const glm::mat4& lightSpace,
                                  bool receiveShadows, float shadowSourceAngle,
                                  const Frustum* cameraFrustum, bool useWorldClipPlane) {
    if (!skinnedLitShader_.Valid() || skeletalDraws_.empty() || whiteTexture_ == nullptr) {
        return;
    }

    skinnedLitShader_.Bind();

    const bool hasEnv = level.Environment() != nullptr && level.Environment()->Valid();
    const bool hasIrr = hasEnv && level.Environment()->HasIrradiance();
    skinnedLitShader_.SetInt("uHasEnvMap", hasEnv ? 1 : 0);
    skinnedLitShader_.SetInt("uHasIrradiance", hasIrr ? 1 : 0);
    skinnedLitShader_.SetFloat("uEnvExposure", level.EnvironmentExposure());
    skinnedLitShader_.SetFloat("uEnvMaxLod", hasEnv ? level.Environment()->MaxLod() : 0.0f);
    skinnedLitShader_.SetInt("uEnvMap", 3);
    skinnedLitShader_.SetInt("uIrradianceMap", 4);
    if (hasEnv) {
        level.Environment()->Bind(3);
    }
    if (hasIrr) {
        level.Environment()->BindIrradiance(4);
    }

    float texel = shadowMap_.Valid() ? 1.0f / static_cast<float>(shadowMap_.Size()) : 0.0f;
    if (receiveShadows && texel > 0.0f) {
        const float soft =
            std::clamp(shadowSourceAngle / kDefaultLightSourceAngleDegrees, 0.25f, 16.0f);
        texel *= soft;
    }
    skinnedLitShader_.SetInt("uShadowMap", 1);
    skinnedLitShader_.SetInt("uReceiveShadows", (receiveShadows && shadowMap_.Valid()) ? 1 : 0);
    skinnedLitShader_.SetFloat("uShadowTexelSize", texel);
    if (receiveShadows && shadowMap_.Valid()) {
        shadowMap_.BindDepthTexture(1);
    }

    skinnedLitShader_.SetInt("uHasPlanarReflection", 0);
    if (!useWorldClipPlane) {
        skinnedLitShader_.SetInt("uUseClipPlane", 0);
        skinnedLitShader_.SetVec4("uClipPlane", 0.0f, 1.0f, 0.0f, 0.0f);
    }

    for (const SkeletalDrawItem& item : skeletalDraws_) {
        if (item.mesh == nullptr || !item.mesh->Valid()) {
            continue;
        }

        ++frameStats_.objectsTotal;
        if (cameraFrustum != nullptr) {
            const Aabb worldBox =
                Aabb::fromLocalTransformed(item.mesh->LocalMin(), item.mesh->LocalMax(), item.model);
            if (!cameraFrustum->intersectsAabb(worldBox)) {
                ++frameStats_.objectsCulled;
                continue;
            }
        }
        ++frameStats_.objectsVisible;

        const Material& material = item.mesh->GetMaterial();
        const glm::mat4& model = item.model;
        const glm::mat4 mvp = projection * view * model;
        const glm::mat3 model3(model);
        const float det = glm::determinant(model3);
        const glm::mat3 normal =
            (std::abs(det) < 1.0e-12f) ? glm::mat3(1.0f) : glm::transpose(glm::inverse(model3));

        skinnedLitShader_.SetMat4("uMVP", glm::value_ptr(mvp));
        skinnedLitShader_.SetMat4("uModel", glm::value_ptr(model));
        skinnedLitShader_.SetMat3("uNormalMatrix", glm::value_ptr(normal));
        skinnedLitShader_.SetMat4("uLightSpaceMatrix", glm::value_ptr(lightSpace));
        skinnedLitShader_.SetVec3("uAlbedo", material.albedo.x, material.albedo.y,
                                  material.albedo.z);
        skinnedLitShader_.SetFloat("uAlpha", material.alpha);
        skinnedLitShader_.SetVec2("uUvScale", material.uvScale.x, material.uvScale.y);
        skinnedLitShader_.SetFloat("uShininess", material.shininess);
        skinnedLitShader_.SetFloat("uRoughness", material.roughness);
        skinnedLitShader_.SetVec3("uSpecular", material.specular.x, material.specular.y,
                                  material.specular.z);
        skinnedLitShader_.SetFloat("uMetallic", material.metallic);
        skinnedLitShader_.SetInt("uAlbedoMap", 0);
        skinnedLitShader_.SetInt("uNormalMap", 2);
        // Keep pass-level shadow uniforms (valid map + soft texel); do not overwrite per draw.

        if (!item.boneMatrices.empty()) {
            skinnedLitShader_.SetMat4Array("uBones", glm::value_ptr(item.boneMatrices[0]),
                                           static_cast<int>(item.boneMatrices.size()));
        }

        const Texture* albedo = material.albedoMap && material.albedoMap->Valid()
                                    ? material.albedoMap.get()
                                    : whiteTexture_.get();
        albedo->Bind(0);
        const Texture* normals = material.normalMap && material.normalMap->Valid()
                                     ? material.normalMap.get()
                                     : flatNormalTexture_.get();
        normals->Bind(2);

        item.mesh->Draw();
        ++frameStats_.drawsSubmitted;
        frameStats_.trianglesSubmitted += item.mesh->TriangleCount();
    }
}

void Renderer::drawQueuedStatic(const Level& level, const glm::mat4& view,
                                const glm::mat4& projection, const glm::mat4& lightSpace,
                                bool receiveShadows, float shadowSourceAngle) {
    if (staticDraws_.empty() || whiteTexture_ == nullptr || !unlitShader_.Valid()) {
        return;
    }

    // Attachments: unlit + two-pass depth (avoids grey ghost from two-sided z-fight).
    glDisable(GL_CLIP_DISTANCE0);
    glDisable(GL_BLEND);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    unlitShader_.Bind();
    unlitShader_.SetInt("uUseClipPlane", 0);
    unlitShader_.SetVec4("uClipPlane", 0.0f, 1.0f, 0.0f, 0.0f);
    unlitShader_.SetInt("uAlbedoMap", 0);
    unlitShader_.SetVec2("uUvScale", 1.0f, 1.0f);
    unlitShader_.SetFloat("uAlpha", 1.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    whiteTexture_->Bind(0);

    for (const StaticDrawItem& item : staticDraws_) {
        if (item.mesh == nullptr || !item.mesh->Valid()) {
            continue;
        }

        const Material& material = item.material;
        const glm::mat4& model = item.model;
        const glm::mat4 mvp = projection * view * model;

        unlitShader_.SetMat4("uMVP", glm::value_ptr(mvp));
        unlitShader_.SetMat4("uModel", glm::value_ptr(model));
        unlitShader_.SetVec3("uAlbedo", material.albedo.x, material.albedo.y, material.albedo.z);

        // Pass 1: establish closest depth (both windings compete).
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthFunc(GL_LESS);
        item.mesh->Draw();
        // Pass 2: solid color only on the winning depth samples.
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthFunc(GL_EQUAL);
        item.mesh->Draw();
        glDepthFunc(GL_LESS);

        ++frameStats_.objectsTotal;
        ++frameStats_.objectsVisible;
        ++frameStats_.drawsSubmitted;
        frameStats_.trianglesSubmitted += item.mesh->TriangleCount();
        (void)level;
        (void)lightSpace;
        (void)receiveShadows;
        (void)shadowSourceAngle;
    }

    glEnable(GL_CULL_FACE);
}

void Renderer::drawDebug(const Level& level, const Camera& camera, const glm::mat4& lightSpace,
                         bool hasLightSpace) {
    if (!debugDrawEnabled_ || !debugDraw_.IsValid()) {
        return;
    }

    debugDraw_.Clear();

    constexpr glm::vec3 kAabbColor{0.2f, 0.95f, 0.35f};
    constexpr glm::vec3 kHiddenAabbColor{0.95f, 0.35f, 0.85f}; // BlockingVolume / hidden
    constexpr glm::vec3 kFrustumColor{1.0f, 0.85f, 0.15f};

    for (const StaticMeshComponent& object : level.StaticMeshes()) {
        if (object.mesh == nullptr || !object.mesh->Valid()) {
            continue;
        }
        const Aabb box = worldAabbFromObject(object);
        debugDraw_.AddAabb(box.min, box.max, object.hidden ? kHiddenAabbColor : kAabbColor);
    }

    if (hasLightSpace) {
        debugDraw_.AddLightFrustum(lightSpace, kFrustumColor);
    }

    const glm::mat4 viewProjection = camera.ProjectionMatrix() * camera.ViewMatrix();
    debugDraw_.Flush(viewProjection);
}

} // namespace leon
