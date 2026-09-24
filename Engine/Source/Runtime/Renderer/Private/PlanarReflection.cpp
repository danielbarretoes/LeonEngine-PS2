#include <glad/glad.h>

#include <iostream>
#include "PlanarReflection.h"


FPlanarReflection::~FPlanarReflection() {
    Destroy();
}

bool FPlanarReflection::EnsureSize(int width, int height) {
    if (width < 1 || height < 1) {
        return false;
    }
    if (Valid() && width_ == width && height_ == height) {
        return true;
    }

    Destroy();
    width_ = width;
    height_ = height;

    glGenFramebuffers(1, &fbo_);
    glGenTextures(1, &colorTexture_);
    glGenRenderbuffers(1, &depthRbo_);

    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width_, height_, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo_);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "PlanarReflection framebuffer incomplete\n";
        Destroy();
        return false;
    }
    return true;
}

void FPlanarReflection::Destroy() {
    if (depthRbo_ != 0) {
        glDeleteRenderbuffers(1, &depthRbo_);
        depthRbo_ = 0;
    }
    if (colorTexture_ != 0) {
        glDeleteTextures(1, &colorTexture_);
        colorTexture_ = 0;
    }
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    width_ = 0;
    height_ = 0;
}

void FPlanarReflection::Begin() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Reflection matrix flips winding — cull what was front.
    glCullFace(GL_FRONT);
}

void FPlanarReflection::End(int framebufferWidth, int framebufferHeight,
                           FRHIFramebufferId restoreFbo) const {
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, restoreFbo);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
}

void FPlanarReflection::BindColorTexture(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
}

