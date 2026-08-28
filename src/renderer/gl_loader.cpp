#include "gl_loader.hpp"
#include <cstdio>

namespace entergram {
namespace gl_loader_detail {

// Function pointer storage
PFNGLGENVERTEXARRAYSPROC          p_glGenVertexArrays            = nullptr;
PFNGLBINDVERTEXARRAYPROC          p_glBindVertexArray            = nullptr;
PFNGLCREATESHADERPROC             p_glCreateShader               = nullptr;
PFNGLSHADERSOURCEPROC             p_glShaderSource               = nullptr;
PFNGLCOMPILESHADERPROC            p_glCompileShader              = nullptr;
PFNGLDELETESHADERPROC             p_glDeleteShader               = nullptr;
PFNGLCREATEPROGRAMPROC            p_glCreateProgram              = nullptr;
PFNGLATTACHSHADERPROC             p_glAttachShader               = nullptr;
PFNGLLINKPROGRAMPROC              p_glLinkProgram                = nullptr;
PFNGLDELETEPROGRAMPROC            p_glDeleteProgram              = nullptr;
PFNGLUSEPROGRAMPROC               p_glUseProgram                 = nullptr;
PFNGLGENBUFFERSPROC               p_glGenBuffers                 = nullptr;
PFNGLBINDBUFFERPROC               p_glBindBuffer                 = nullptr;
PFNGLBUFFERDATAPROC               p_glBufferData                 = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC      p_glVertexAttribPointer        = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC  p_glEnableVertexAttribArray     = nullptr;
PFNGLGETUNIFORMLOCATIONPROC       p_glGetUniformLocation         = nullptr;
PFNGLUNIFORMMATRIX4FVPROC         p_glUniformMatrix4fv           = nullptr;
PFNGLUNIFORM1IPROC                p_glUniform1i                   = nullptr;
PFNGLUNIFORM2IPROC                p_glUniform2i                   = nullptr;
PFNGLUNIFORM2FPROC                p_glUniform2f                   = nullptr;
PFNGLUNIFORM4FPROC                p_glUniform4f                   = nullptr;
PFNGLACTIVETEXTUREPROC            p_glActiveTexture              = nullptr;
PFNGLGENERATEMIPMAPPROC           p_glGenerateMipmap             = nullptr;
PFNGLGETSHADERIVPROC              p_glGetShaderiv                = nullptr;
PFNGLGETSHADERINFOLOGPROC         p_glGetShaderInfoLog           = nullptr;
PFNGLGETPROGRAMIVPROC             p_glGetProgramiv               = nullptr;
PFNGLGETPROGRAMINFOLOGPROC        p_glGetProgramInfoLog          = nullptr;
PFNGLDRAWELEMENTSPROC              p_glDrawElements             = nullptr;

} // namespace gl_loader_detail

namespace gl_loader {

bool load() {
    using namespace gl_loader_detail;

    p_glGenVertexArrays         = (PFNGLGENVERTEXARRAYSPROC)        SDL_GL_GetProcAddress("glGenVertexArrays");
    p_glBindVertexArray         = (PFNGLBINDVERTEXARRAYPROC)        SDL_GL_GetProcAddress("glBindVertexArray");
    p_glCreateShader            = (PFNGLCREATESHADERPROC)           SDL_GL_GetProcAddress("glCreateShader");
    p_glShaderSource            = (PFNGLSHADERSOURCEPROC)          SDL_GL_GetProcAddress("glShaderSource");
    p_glCompileShader           = (PFNGLCOMPILESHADERPROC)          SDL_GL_GetProcAddress("glCompileShader");
    p_glDeleteShader            = (PFNGLDELETESHADERPROC)           SDL_GL_GetProcAddress("glDeleteShader");
    p_glCreateProgram           = (PFNGLCREATEPROGRAMPROC)          SDL_GL_GetProcAddress("glCreateProgram");
    p_glAttachShader            = (PFNGLATTACHSHADERPROC)           SDL_GL_GetProcAddress("glAttachShader");
    p_glLinkProgram             = (PFNGLLINKPROGRAMPROC)            SDL_GL_GetProcAddress("glLinkProgram");
    p_glDeleteProgram           = (PFNGLDELETEPROGRAMPROC)          SDL_GL_GetProcAddress("glDeleteProgram");
    p_glUseProgram              = (PFNGLUSEPROGRAMPROC)            SDL_GL_GetProcAddress("glUseProgram");
    p_glGenBuffers              = (PFNGLGENBUFFERSPROC)           SDL_GL_GetProcAddress("glGenBuffers");
    p_glBindBuffer              = (PFNGLBINDBUFFERPROC)           SDL_GL_GetProcAddress("glBindBuffer");
    p_glBufferData              = (PFNGLBUFFERDATAPROC)           SDL_GL_GetProcAddress("glBufferData");
    p_glVertexAttribPointer     = (PFNGLVERTEXATTRIBPOINTERPROC)    SDL_GL_GetProcAddress("glVertexAttribPointer");
    p_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
    p_glActiveTexture           = (PFNGLACTIVETEXTUREPROC)          SDL_GL_GetProcAddress("glActiveTexture");
    p_glGetUniformLocation      = (PFNGLGETUNIFORMLOCATIONPROC)     SDL_GL_GetProcAddress("glGetUniformLocation");
    p_glUniformMatrix4fv        = (PFNGLUNIFORMMATRIX4FVPROC)       SDL_GL_GetProcAddress("glUniformMatrix4fv");
    p_glUniform1i                = (PFNGLUNIFORM1IPROC)              SDL_GL_GetProcAddress("glUniform1i");
    p_glUniform2i                = (PFNGLUNIFORM2IPROC)              SDL_GL_GetProcAddress("glUniform2i");
    p_glUniform2f                = (PFNGLUNIFORM2FPROC)              SDL_GL_GetProcAddress("glUniform2f");
    p_glUniform4f                = (PFNGLUNIFORM4FPROC)              SDL_GL_GetProcAddress("glUniform4f");
    p_glGenerateMipmap          = (PFNGLGENERATEMIPMAPPROC)         SDL_GL_GetProcAddress("glGenerateMipmap");
    p_glGetShaderiv             = (PFNGLGETSHADERIVPROC)            SDL_GL_GetProcAddress("glGetShaderiv");
    p_glGetShaderInfoLog        = (PFNGLGETSHADERINFOLOGPROC)       SDL_GL_GetProcAddress("glGetShaderInfoLog");
    p_glGetProgramiv            = (PFNGLGETPROGRAMIVPROC)           SDL_GL_GetProcAddress("glGetProgramiv");
    p_glGetProgramInfoLog       = (PFNGLGETPROGRAMINFOLOGPROC)       SDL_GL_GetProcAddress("glGetProgramInfoLog");
    p_glDrawElements             = (PFNGLDRAWELEMENTSPROC)              SDL_GL_GetProcAddress("glDrawElements");

    if (!p_glGenVertexArrays || !p_glCreateShader || !p_glCreateProgram ||
        !p_glGenBuffers || !p_glUseProgram) {
        fprintf(stderr, "Failed to load OpenGL 3.3 functions\n");
        return false;
    }
    return true;
}

bool is_available() {
    return gl_loader_detail::p_glGenVertexArrays != nullptr;
}

} // namespace gl_loader
} // namespace entergram
