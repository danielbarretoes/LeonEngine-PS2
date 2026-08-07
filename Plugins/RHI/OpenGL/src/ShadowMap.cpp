#include <glad/glad.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <leon/render/ShadowMap.h>
#include <limits>

namespace leon {

ShadowMap::~ShadowMap() {
    Destroy();
}

bool ShadowMap::Create(int size) {
    Destroy();
    size = std::max(size, 64);
    size_ = size;

    glGenFramebuffers(1, &fbo_);
    glGenTextures(1, &depthTexture_);

    glBindTexture(GL_TEXTURE_2D, depthTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size_, size_, 0, GL_DEPTH_COMPONENT,
                 GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const std::array<float, 4> border = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border.data());

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ShadowMap framebuffer incomplete\n";
        Destroy();
        return false;
    }
    return true;
}

void ShadowMap::Destroy() {
    if (depthTexture_ != 0) {
        glDeleteTextures(1, &depthTexture_);
        depthTexture_ = 0;
    }
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    size_ = 0;
}

void ShadowMap::Begin() const {
    glViewport(0, 0, size_, size_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glClear(GL_DEPTH_BUFFER_BIT);
    // No face cull so one-sided casters (planes, cards) still write depth.
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 2.0f);
}

void ShadowMap::End(int framebufferWidth, int framebufferHeight, unsigned int restoreFbo) const {
    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, restoreFbo);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
}

void ShadowMap::BindDepthTexture(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, depthTexture_);
}

glm::mat4 ShadowMap::FitLightSpaceMatrix(const glm::vec3& lightDirection, const glm::vec3& worldMin,
                                         const glm::vec3& worldMax, float padding) {
    glm::vec3 dir = lightDirection;
    if (glm::dot(dir, dir) < 1e-8f) {
        dir = {0.35f, -1.0f, -0.45f};
    }
    dir = glm::normalize(dir);

    const glm::vec3 center = (worldMin + worldMax) * 0.5f;
    const glm::vec3 extents = (worldMax - worldMin) * 0.5f + glm::vec3(padding);
    const float radius = glm::length(extents);

    glm::vec3 up{0.0f, 1.0f, 0.0f};
    if (std::abs(glm::dot(dir, up)) > 0.95f) {
        up = {0.0f, 0.0f, 1.0f};
    }

    const glm::vec3 eye = center - (dir * (radius + 1.0f));
    const glm::mat4 lightView = glm::lookAt(eye, center, up);

    glm::vec3 minLS(std::numeric_limits<float>::max());
    glm::vec3 maxLS(std::numeric_limits<float>::lowest());

    const std::array<glm::vec3, 8> corners = {{
        {worldMin.x, worldMin.y, worldMin.z},
        {worldMax.x, worldMin.y, worldMin.z},
        {worldMin.x, worldMax.y, worldMin.z},
        {worldMax.x, worldMax.y, worldMin.z},
        {worldMin.x, worldMin.y, worldMax.z},
        {worldMax.x, worldMin.y, worldMax.z},
        {worldMin.x, worldMax.y, worldMax.z},
        {worldMax.x, worldMax.y, worldMax.z},
    }};

    for (const glm::vec3& corner : corners) {
        const glm::vec3 ls = glm::vec3(lightView * glm::vec4(corner, 1.0f));
        minLS = glm::min(minLS, ls);
        maxLS = glm::max(maxLS, ls);
    }

    // Eye-space Z is negative in front of the light camera.
    const float zNear = std::max(0.05f, -maxLS.z + padding);
    const float zFar = std::max(zNear + 0.1f, -minLS.z + padding);

    const glm::mat4 lightProj = glm::ortho(minLS.x - padding, maxLS.x + padding, minLS.y - padding,
                                           maxLS.y + padding, zNear, zFar);
    return lightProj * lightView;
}

} // namespace leon
