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

constexpr FRHITextureId kInvalidTexture = 0;
constexpr FRHIFramebufferId kInvalidFramebuffer = 0;
constexpr FRHIBufferId kInvalidBuffer = 0;
constexpr FRHIVertexArrayId kInvalidVertexArray = 0;
constexpr FRHIProgramId kInvalidProgram = 0;
constexpr FRHIQueryId kInvalidQuery = 0;
constexpr FRHIRenderbufferId kInvalidRenderbuffer = 0;

