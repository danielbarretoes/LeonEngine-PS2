#include <glad/glad.h>

#include <iostream>
#include "LdrColorTarget.h"


LdrColorTarget::~LdrColorTarget() {
    Destroy();
}

bool LdrColorTarget::EnsureSize(int width, int height) {
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

    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width_, height_, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture_, 0);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "LdrColorTarget framebuffer incomplete\n";
        Destroy();
        return false;
    }
    return true;
}

void LdrColorTarget::Destroy() {
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

void LdrColorTarget::BindWrite() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
}

void LdrColorTarget::BindColorTexture(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
}

