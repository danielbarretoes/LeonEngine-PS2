#pragma once

namespace leon::editor {

/// Color + depth FBO used as the Viewport panel image (`ImGui::Image`).
class EditorViewportTarget {
public:
    EditorViewportTarget() = default;
    ~EditorViewportTarget();

    EditorViewportTarget(const EditorViewportTarget&) = delete;
    EditorViewportTarget& operator=(const EditorViewportTarget&) = delete;

    /// Allocate or resize; returns false if the framebuffer is incomplete.
    [[nodiscard]] bool EnsureSize(int width, int height);
    void Destroy();

    void Begin() const;
    void End() const;

    [[nodiscard]] unsigned int fbo() const { return fbo_; }
    [[nodiscard]] unsigned int colorTexture() const { return colorTexture_; }
    [[nodiscard]] bool Valid() const { return fbo_ != 0 && colorTexture_ != 0; }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

private:
    unsigned int fbo_ = 0;
    unsigned int colorTexture_ = 0;
    unsigned int depthRbo_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace leon::editor
