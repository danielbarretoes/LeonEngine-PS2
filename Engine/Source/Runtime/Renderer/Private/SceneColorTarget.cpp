#include <glad/glad.h>

#include <iostream>
#include "SceneColorTarget.h"


FSceneColorTarget::~FSceneColorTarget() {
    Destroy();
}

bool FSceneColorTarget::EnsureSize(int InWidth, int InHeight) {
    if (InWidth < 1 || InHeight < 1) {
        return false;
    }
    if (Valid() && Width == InWidth && Height == InHeight) {
        return true;
    }

    Destroy();
    Width = InWidth;
    Height = InHeight;

    glGenFramebuffers(1, &Fbo);
    glGenTextures(1, &ColorTexture);
    glGenTextures(1, &DepthTexture);

    glBindTexture(GL_TEXTURE_2D, ColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, Width, Height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, DepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, Width, Height, 0, GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Sample as raw depth in SSAO / composite (not shadow-compare).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ColorTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, DepthTexture, 0);

    const GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (Status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "SceneColorTarget framebuffer incomplete\n";
        Destroy();
        return false;
    }
    return true;
}

void FSceneColorTarget::Destroy() {
    if (DepthTexture != 0) {
        glDeleteTextures(1, &DepthTexture);
        DepthTexture = 0;
    }
    if (ColorTexture != 0) {
        glDeleteTextures(1, &ColorTexture);
        ColorTexture = 0;
    }
    if (Fbo != 0) {
        glDeleteFramebuffers(1, &Fbo);
        Fbo = 0;
    }
    Width = 0;
    Height = 0;
}

void FSceneColorTarget::Begin() const {
    glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
    glViewport(0, 0, Width, Height);
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void FSceneColorTarget::End(int FramebufferWidth, int FramebufferHeight,
                           unsigned int RestoreFbo) const {
    glBindFramebuffer(GL_FRAMEBUFFER, RestoreFbo);
    glViewport(0, 0, FramebufferWidth, FramebufferHeight);
}

void FSceneColorTarget::BindColorTexture(unsigned int Unit) const {
    glActiveTexture(GL_TEXTURE0 + Unit);
    glBindTexture(GL_TEXTURE_2D, ColorTexture);
}

void FSceneColorTarget::BindDepthTexture(unsigned int Unit) const {
    glActiveTexture(GL_TEXTURE0 + Unit);
    glBindTexture(GL_TEXTURE_2D, DepthTexture);
}

