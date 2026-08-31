// gl_loader.hpp - OpenGL 3.3 loader
// Loads OpenGL 3.3 core profile functions via SDL_GL_GetProcAddress

#pragma once

// Include SDL.h for SDL_GL_GetProcAddress, and SDL_opengl.h for GL types + extensions
#define GL_GLEXT_PROTOTYPES
#include <SDL.h>
#include <SDL_opengl.h>
#include <GL/glext.h>
#ifndef PFNGLDRAWELEMENTSPROC
typedef void (APIENTRYP PFNGLDRAWELEMENTSPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices);
#endif

namespace entergram {

// glDrawElements is part of OpenGL 1.x and available directly via opengl32.lib
// No need to load it as a function pointer

namespace gl_loader_detail {
    // Function pointers for OpenGL 3.3 Core profile functions loaded at runtime
    extern PFNGLGENVERTEXARRAYSPROC          p_glGenVertexArrays;
    extern PFNGLBINDVERTEXARRAYPROC          p_glBindVertexArray;
    extern PFNGLCREATESHADERPROC             p_glCreateShader;
    extern PFNGLSHADERSOURCEPROC             p_glShaderSource;
    extern PFNGLCOMPILESHADERPROC            p_glCompileShader;
    extern PFNGLDELETESHADERPROC             p_glDeleteShader;
    extern PFNGLCREATEPROGRAMPROC            p_glCreateProgram;
    extern PFNGLATTACHSHADERPROC             p_glAttachShader;
    extern PFNGLLINKPROGRAMPROC              p_glLinkProgram;
    extern PFNGLDELETEPROGRAMPROC            p_glDeleteProgram;
    extern PFNGLUSEPROGRAMPROC               p_glUseProgram;
    extern PFNGLGENBUFFERSPROC               p_glGenBuffers;
    extern PFNGLBINDBUFFERPROC               p_glBindBuffer;
    extern PFNGLBUFFERDATAPROC               p_glBufferData;
    extern PFNGLVERTEXATTRIBPOINTERPROC      p_glVertexAttribPointer;
    extern PFNGLENABLEVERTEXATTRIBARRAYPROC  p_glEnableVertexAttribArray;
    extern PFNGLGETUNIFORMLOCATIONPROC       p_glGetUniformLocation;
    extern PFNGLUNIFORMMATRIX4FVPROC         p_glUniformMatrix4fv;
    extern PFNGLUNIFORM1IPROC                p_glUniform1i;
    extern PFNGLUNIFORM2IPROC                p_glUniform2i;
    extern PFNGLUNIFORM2FPROC               p_glUniform2f;
    extern PFNGLUNIFORM4FPROC               p_glUniform4f;
    extern PFNGLACTIVETEXTUREPROC            p_glActiveTexture;
    extern PFNGLGENERATEMIPMAPPROC           p_glGenerateMipmap;
    extern PFNGLGETSHADERIVPROC              p_glGetShaderiv;
    extern PFNGLGETSHADERINFOLOGPROC         p_glGetShaderInfoLog;
    extern PFNGLGETPROGRAMIVPROC             p_glGetProgramiv;
    extern PFNGLGETPROGRAMINFOLOGPROC        p_glGetProgramInfoLog;
    extern PFNGLDRAWELEMENTSPROC              p_glDrawElements;
}

namespace gl_loader {
    bool load();
    bool is_available();
}
} // namespace entergram
