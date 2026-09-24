#pragma once

#include <cstdint>


/// Opaque GPU object ids for public Engine headers.
/// Under the OpenGL plugin these are GLuint-compatible; 0 means invalid / default FB.
using FRHITextureId = std::uint32_t;
using FRHIFramebufferId = std::uint32_t;
using FRHIBufferId = std::uint32_t;
using FRHIVertexArrayId = std::uint32_t;
using FRHIProgramId = std::uint32_t;
using FRHIQueryId = std::uint32_t;
using FRHIRenderbufferId = std::uint32_t;

constexpr FRHITextureId InvalidTexture = 0;
constexpr FRHIFramebufferId InvalidFramebuffer = 0;
constexpr FRHIBufferId InvalidBuffer = 0;
constexpr FRHIVertexArrayId InvalidVertexArray = 0;
constexpr FRHIProgramId InvalidProgram = 0;
constexpr FRHIQueryId InvalidQuery = 0;
constexpr FRHIRenderbufferId InvalidRenderbuffer = 0;

