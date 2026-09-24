#include <glad/glad.h>

#include <iostream>
#include "PostProcess.h"


SsaoTarget::~SsaoTarget() {
    Destroy();
}

bool SsaoTarget::EnsureSize(int width, int height) {
    if (width < 1 || height < 1) {
        return false;
    }
    if (Valid() && width_ == width && height_ == height) {
        return true;
    }

    Destroy();
    width_ = width;
    height_ = height;

    for (int i = 0; i < 2; ++i) {
        glGenFramebuffers(1, &fbo_[i]);
        glGenTextures(1, &color_[i]);
        glBindTexture(GL_TEXTURE_2D, color_[i]);
        // R16F avoids contour banding that R8 + aoPower/tonemap makes visible on flat materials.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width_, height_, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_[i], 0);
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "SsaoTarget framebuffer incomplete\n";
            Destroy();
            return false;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void SsaoTarget::Destroy() {
    for (int i = 0; i < 2; ++i) {
        if (color_[i] != 0) {
            glDeleteTextures(1, &color_[i]);
            color_[i] = 0;
        }
        if (fbo_[i] != 0) {
            glDeleteFramebuffers(1, &fbo_[i]);
            fbo_[i] = 0;
        }
    }
    width_ = 0;
    height_ = 0;
}

void SsaoTarget::BindWrite(int index) const {
    const int i = (index == 0) ? 0 : 1;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_[i]);
    glViewport(0, 0, width_, height_);
}

void SsaoTarget::BindColorTexture(int index, unsigned int unit) const {
    const int i = (index == 0) ? 0 : 1;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, color_[i]);
}

