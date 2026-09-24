#include <glad/glad.h>

#include <iostream>
#include "PlanarReflection.h"


FPlanarReflection::~FPlanarReflection() {
    Destroy();
}

bool FPlanarReflection::EnsureSize(int InWidth, int InHeight) {
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
    glGenRenderbuffers(1, &DepthRbo);

    glBindTexture(GL_TEXTURE_2D, ColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, Width, Height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindRenderbuffer(GL_RENDERBUFFER, DepthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, Width, Height);

    glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ColorTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DepthRbo);

    const GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (Status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "PlanarReflection framebuffer incomplete\n";
        Destroy();
        return false;
    }
    return true;
}

void FPlanarReflection::Destroy() {
    if (DepthRbo != 0) {
        glDeleteRenderbuffers(1, &DepthRbo);
        DepthRbo = 0;
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

void FPlanarReflection::Begin() const {
    glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
    glViewport(0, 0, Width, Height);
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Reflection matrix flips winding — cull what was front.
    glCullFace(GL_FRONT);
}

void FPlanarReflection::End(int FramebufferWidth, int FramebufferHeight,
                           FRHIFramebufferId RestoreFbo) const {
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, RestoreFbo);
    glViewport(0, 0, FramebufferWidth, FramebufferHeight);
}

void FPlanarReflection::BindColorTexture(unsigned int Unit) const {
    glActiveTexture(GL_TEXTURE0 + Unit);
    glBindTexture(GL_TEXTURE_2D, ColorTexture);
}

