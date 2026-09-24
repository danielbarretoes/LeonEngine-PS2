#pragma once


/// OpenGL VBO attribute offset (byte offset encoded as a pointer while ARRAY_BUFFER is bound).
template <typename T, typename Member>
[[nodiscard]] inline const void* GlAttribOffset(Member T::* member) noexcept {
    const T* base = nullptr;
    return static_cast<const void*>(&(base->*member));
}

