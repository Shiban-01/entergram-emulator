#include "renderer.hpp"
#include <cassert>
#include <cstring>

namespace entergram {

// =============================================================================
// OpenGL 3.3 Core Profile Sprite Renderer
//
// Uses a simple vertex + fragment shader pipeline:
//   - Vertex shader: orthographic projection, passes UV to fragment shader
//   - Fragment shader: samples texture, multiplies by vertex color
//   - Video frames are uploaded as RGBA textures (YUV→RGB done in CPU)
//
// Vertex format: posX, posY, texCoordX, texCoordY, colorRGBA
// =============================================================================

// Vertex shader source (GLSL 3.30 core)
static constexpr const char* VERTEX_SHADER_SRC = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec4 aColor;

out vec2 texCoord;
out vec4 vertexColor;

uniform mat4 uProjection;

void main() {
    texCoord = aTexCoord;
    vertexColor = aColor;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
)glsl";

// Fragment shader source (GLSL 3.30 core)
static constexpr const char* FRAGMENT_SHADER_SRC = R"glsl(
#version 330 core
in vec2 texCoord;
in vec4 vertexColor;

out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    vec4 texColor = texture(uTexture, texCoord);
    FragColor = texColor * vertexColor;
    if (FragColor.a == 0.0) discard;
}
)glsl";

// Simple shader compilation and linking utilities
static uint32_t compile_shader(uint32_t type, const char* source) {
    uint32_t shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // Check for compilation errors
    int32_t success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
        // In a real implementation, log this error
        return 0;
    }

    return shader;
}

SpriteRenderer::SpriteRenderer() = default;
SpriteRenderer::~SpriteRenderer() = default;

bool SpriteRenderer::initialize() {
    // Create shader program
    shader_program_ = glCreateProgram();

    uint32_t vs = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER_SRC);
    uint32_t fs = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SRC);

    if (!vs || !fs) {
        return false;
    }

    glAttachShader(shader_program_, vs);
    glAttachShader(shader_program_, fs);
    glLinkProgram(shader_program_);

    // Check for linking errors
    int32_t success;
    glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);
    if (!success) {
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    // Create VAO for sprite rendering
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    // Quad vertices (positions + texture coords)
    // Using a full-screen quad that can be transformed
    float vertices[] = {
        // Position    // TexCoord
        0.0f, 0.0f,    0.0f, 0.0f,
        1.0f, 0.0f,    1.0f, 0.0f,
        1.0f, 1.0f,    1.0f, 1.0f,
        0.0f, 1.0f,    0.0f, 1.0f,
    };

    uint32_t indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &ebo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // TexCoord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Unbind VAO
    glBindVertexArray(0);

    return true;
}

void SpriteRenderer::render(const Sprite& sprite, const Texture& texture) {
    glUseProgram(shader_program_);
    texture.bind(0);

    // Set up model-view-projection (use simple translation + scale)
    // The vertex shader applies the projection; we pass position/scale via
    // a simple uniform for the quad's position and size.

    // For now, we'll use glViewport for positioning
    glViewport(
        static_cast<int>(sprite.x),
        static_cast<int>(viewport_height_ - sprite.y - sprite.height),
        static_cast<int>(sprite.width),
        static_cast<int>(sprite.height)
    );

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void SpriteRenderer::render_video(const Sprite& sprite, const Texture& texture) {
    // Video rendering uses the same sprite pipeline (RGBA texture)
    render(sprite, texture);
}

void SpriteRenderer::set_viewport(int width, int height) {
    viewport_width_ = width;
    viewport_height_ = height;
    glViewport(0, 0, width, height);
}

void SpriteRenderer::clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void SpriteRenderer::begin() {
    glUseProgram(shader_program_);
}

void SpriteRenderer::end() {
    glBindVertexArray(0);
    glUseProgram(0);
}

void SpriteRenderer::set_projection(float left, float right, float bottom, float top) {
    // Orthographic projection matrix (column-major for GLSL)
    float matrix[16] = {
        2.0f / (right - left), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -(right + left) / (right - left),
        -(top + bottom) / (top - bottom),
        0.0f, 1.0f
    };

    // TODO: upload matrix to shader uniform
    // For now the vertex shader uses an identity-like approach
}

// Texture implementation
void Texture::upload(int width, int height, const uint8_t* data, TextureFormat format) {
    width_ = width;
    height_ = height;

    if (texture_id_ == 0) {
        glGenTextures(1, &texture_id_);
    }

    glBindTexture(GL_TEXTURE_2D, texture_id_);

    GLenum internal_format = GL_RGBA8;
    GLenum src_format = GL_RGBA;

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width_, height_,
                 0, src_format, GL_UNSIGNED_BYTE, data);

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::bind(uint32_t unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
}

void Texture::unbind(uint32_t unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
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
