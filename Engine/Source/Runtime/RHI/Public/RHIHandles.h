#pragma once

#include "CoreTypes.h"

/** Opaque GPU object ids for public Engine headers: GLuint-compatible under OpenGLDrv; 0 means invalid / default FB. */
using FRHITextureId = uint32;
using FRHIFramebufferId = uint32;
using FRHIBufferId = uint32;
using FRHIVertexArrayId = uint32;
using FRHIProgramId = uint32;
using FRHIQueryId = uint32;
using FRHIRenderbufferId = uint32;

constexpr FRHITextureId InvalidTexture = 0;
constexpr FRHIFramebufferId InvalidFramebuffer = 0;
constexpr FRHIBufferId InvalidBuffer = 0;
constexpr FRHIVertexArrayId InvalidVertexArray = 0;
constexpr FRHIProgramId InvalidProgram = 0;
constexpr FRHIQueryId InvalidQuery = 0;
constexpr FRHIRenderbufferId InvalidRenderbuffer = 0;
