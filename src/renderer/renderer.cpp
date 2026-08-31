#include "renderer.hpp"
#include "gl_loader.hpp"

// Enable OpenGL extension prototypes so glClear/glGenTextures etc. are declared
#define GL_GLEXT_PROTOTYPES
#include <SDL_opengl.h>

#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdint>

namespace entergram {
using namespace gl_loader_detail;

static constexpr const char* VERTEX_SHADER_SRC = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec4 aColor;
out vec2 texCoord;
out vec4 vertexColor;
uniform vec2 uScale;  // scale factor for NDC space
void main() {
    texCoord = aTexCoord;
    vertexColor = aColor;
    // Direct NDC coordinates: pos is already in [0,1] range, convert to [-1,1]
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";

static constexpr const char* FRAGMENT_SHADER_SRC = R"glsl(
#version 330 core
in vec2 texCoord;
in vec4 vertexColor;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec4 uColor;
void main() {
    vec4 texColor = texture(uTexture, texCoord);
    vec4 color = uColor;
    if (color.a == 0.0) color.a = 1.0;
    FragColor = texColor * color;
}
)glsl";

SpriteRenderer::SpriteRenderer() = default;
SpriteRenderer::~SpriteRenderer() = default;

bool SpriteRenderer::initialize() {
    if (!gl_loader::is_available()) {
        if (!gl_loader::load()) {
            fprintf(stderr, "Failed to load OpenGL functions\n");
            return false;
        }
    }

    printf("OpenGL functions loaded: p_glCreateShader=%p p_glUseProgram=%p p_glDrawElements=%s\n",
           (void*)p_glCreateShader, (void*)p_glUseProgram,
           (void*)glDrawElements ? "available" : "null");

    shader_program_ = p_glCreateProgram();
    if (shader_program_ == 0) {
        fprintf(stderr, "glCreateProgram returned 0\n");
        return false;
    }

    GLuint vs = p_glCreateShader(GL_VERTEX_SHADER);
    p_glShaderSource(vs, 1, &VERTEX_SHADER_SRC, nullptr);
    p_glCompileShader(vs);

    GLint success = 0;
    p_glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        p_glGetShaderInfoLog(vs, sizeof(info_log), nullptr, info_log);
        fprintf(stderr, "Vertex shader error: %s\n", info_log);
        return false;
    }

    GLuint fs = p_glCreateShader(GL_FRAGMENT_SHADER);
    p_glShaderSource(fs, 1, &FRAGMENT_SHADER_SRC, nullptr);
    p_glCompileShader(fs);

    p_glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        p_glGetShaderInfoLog(fs, sizeof(info_log), nullptr, info_log);
        fprintf(stderr, "Fragment shader error: %s\n", info_log);
        return false;
    }

    p_glAttachShader(shader_program_, vs);
    p_glAttachShader(shader_program_, fs);
    p_glLinkProgram(shader_program_);

    p_glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        p_glGetProgramInfoLog(shader_program_, sizeof(info_log), nullptr, info_log);
        fprintf(stderr, "Link error: %s\n", info_log);
        return false;
    }
    printf("Shader program linked: %u\n", shader_program_);

    p_glDeleteShader(vs);
    p_glDeleteShader(fs);

    p_glGenVertexArrays(1, &vao_);
    p_glBindVertexArray(vao_);

    float vertices[] = {
        // pos      tex
        0.0f, 0.0f,    0.0f, 0.0f,
        1.0f, 0.0f,    1.0f, 0.0f,
        1.0f, 1.0f,    1.0f, 1.0f,
        0.0f, 1.0f,    0.0f, 1.0f,
    };
    uint32_t indices[] = {0, 1, 2, 2, 3, 0};

    p_glGenBuffers(1, &vbo_);
    p_glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    p_glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    p_glGenBuffers(1, &ebo_);
    p_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    p_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute (location = 0)
    p_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    p_glEnableVertexAttribArray(0);
    // TexCoord attribute (location = 1)
    p_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    p_glEnableVertexAttribArray(1);

    p_glBindVertexArray(0);
    return true;
}

void SpriteRenderer::render(const Sprite& sprite, const Texture& texture) {
    p_glUseProgram(shader_program_);
    texture.bind(0);
    
    // Set texture unit for the sampler uniform
    GLint tex_loc = p_glGetUniformLocation(shader_program_, "uTexture");
    if (tex_loc >= 0) {
        p_glUniform1i(tex_loc, 0);
    }
    
    p_glBindVertexArray(vao_);

    // Set sprite transform (position, size, color)
    GLint scale_loc = p_glGetUniformLocation(shader_program_, "uScale");
    if (scale_loc >= 0) {
        // Position and size of sprite in normalized [0,1] device space
        float pos_x = sprite.x;
        float pos_y = sprite.y;
        float w = sprite.width;
        float h = sprite.height;
        p_glUniform2f(scale_loc, w, h);
    }

    // Set color (default: white opaque if sprite color not set)
    float r = ((sprite.color >> 24) & 0xFF) / 255.0f;
    float g = ((sprite.color >> 16) & 0xFF) / 255.0f;
    float b = ((sprite.color >> 8) & 0xFF) / 255.0f;
    float a = (sprite.color & 0xFF) / 255.0f;
    if (a == 0.0f) a = 1.0f;  // Default to opaque if not set
    GLint color_loc = p_glGetUniformLocation(shader_program_, "uColor");
    if (color_loc >= 0) {
        p_glUniform4f(color_loc, r, g, b, a);
    }

    // Use core OpenGL function (available on all platforms)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
}

void SpriteRenderer::render_video(const Sprite& sprite, const Texture& texture) {
    render(sprite, texture);
}

void SpriteRenderer::set_viewport(int w, int h) {
    viewport_width_ = w;
    viewport_height_ = h;
    glViewport(0, 0, w, h);
}

void SpriteRenderer::clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void SpriteRenderer::begin() { p_glUseProgram(shader_program_); }
void SpriteRenderer::end() { p_glBindVertexArray(0); p_glUseProgram(0); }

void SpriteRenderer::set_projection(float left, float right, float bottom, float top) {
    float m[16] = {
        2.0f / (right - left), 0, 0, 0,
        0, 2.0f / (top - bottom), 0, 0,
        0, 0, -1.0f, 0,
        -(right + left) / (right - left),
        -(top + bottom) / (top - bottom),
        0, 1
    };
    GLint loc = p_glGetUniformLocation(shader_program_, "uProjection");
    if (loc >= 0) p_glUniformMatrix4fv(loc, 1, GL_FALSE, m);
}

void Texture::upload(int width, int height, const uint8_t* data, TextureFormat format) {
    width_ = width;
    height_ = height;
    if (texture_id_ == 0) glGenTextures(1, &texture_id_);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    GLenum src_format = (format == TextureFormat::RGBA8) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, src_format, GL_UNSIGNED_BYTE, data);
    p_glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::bind(uint32_t unit) const {
    p_glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
}

void Texture::unbind(uint32_t unit) {
    p_glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::set_filter(FilterMode min, FilterMode mag) {
    if (texture_id_ == 0) return;
    GLint min_filter = (min == FilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;
    GLint mag_filter = (mag == FilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace entergram
