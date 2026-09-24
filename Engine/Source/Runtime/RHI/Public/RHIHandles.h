#pragma once

#include <cstdint>


/// Opaque GPU object ids for public Engine headers.
/// Under the OpenGL plugin these are GLuint-compatible; 0 means invalid / default FB.
using RHITextureId = std::uint32_t;
using RHIFramebufferId = std::uint32_t;
using RHIBufferId = std::uint32_t;
using RHIVertexArrayId = std::uint32_t;
using RHIProgramId = std::uint32_t;
using RHIQueryId = std::uint32_t;
using RHIRenderbufferId = std::uint32_t;

constexpr RHITextureId kInvalidTexture = 0;
constexpr RHIFramebufferId kInvalidFramebuffer = 0;
constexpr RHIBufferId kInvalidBuffer = 0;
constexpr RHIVertexArrayId kInvalidVertexArray = 0;
constexpr RHIProgramId kInvalidProgram = 0;
constexpr RHIQueryId kInvalidQuery = 0;
constexpr RHIRenderbufferId kInvalidRenderbuffer = 0;

