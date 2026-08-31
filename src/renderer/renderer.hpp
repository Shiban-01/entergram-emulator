#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace entergram {

// Texture format enum
enum class TextureFormat {
    RGBA8,
    RGB8,
};

// Texture filtering modes
enum class FilterMode {
    Nearest,  // Pixel-perfect, for pixel art
    Linear,   // Smooth interpolation
};

// A GPU texture for rendering sprites and video frames.
class Texture {
public:
    Texture() = default;
    ~Texture() = default;

    // Upload pixel data to the texture (replaces existing content)
    void upload(int width, int height, const uint8_t* data, TextureFormat format = TextureFormat::RGBA8);

    // Bind texture for rendering (sets active texture unit)
    void bind(uint32_t unit = 0) const;

    // Unbind the current texture
    static void unbind(uint32_t unit = 0);

    // Set filtering mode
    void set_filter(FilterMode min, FilterMode mag);

    // Accessors
    int width() const { return width_; }
    int height() const { return height_; }
    uint32_t gl_texture_id() const { return texture_id_; }

private:
    uint32_t texture_id_ = 0;
    int width_ = 0;
    int height_ = 0;
};

// Simple sprite — a textured quad with position and color.
struct Sprite {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float u1 = 0.0f;  // Texture coords
    float v1 = 0.0f;
    float u2 = 1.0f;
    float v2 = 1.0f;
    uint32_t color = 0xFFFFFFFF;  // RGBA packed
    TextureFormat format = TextureFormat::RGBA8;
};

// Sprite renderer using OpenGL 3.3
// Renders textured quads using a sprite shader (YUV→RGB handled in CPU).
class SpriteRenderer {
public:
    SpriteRenderer();
    ~SpriteRenderer();

    // Initialize OpenGL resources (shaders, VAO, VBO)
    bool initialize();

    // Render a sprite to the current framebuffer
    void render(const Sprite& sprite, const Texture& texture);

    // Render video frame as RGBA texture (used by VideoPlayer)
    void render_video(const Sprite& sprite, const Texture& texture);

    // Set viewport dimensions (for orthographic projection)
    void set_viewport(int width, int height);

    // Clear screen with a color (RGBA 0-1)
    void clear(float r = 0, float g = 0, float b = 0, float a = 1);

    // Begin/end batch rendering
    void begin();
    void end();

private:
    uint32_t shader_program_ = 0;
    uint32_t vao_ = 0;
    uint32_t vbo_ = 0;
    uint32_t ebo_ = 0;
    int viewport_width_ = 800;
    int viewport_height_ = 600;

    // Compile shader source
    bool load_shaders();

    // Simple orthographic projection matrix
    void set_projection(float left, float right, float bottom, float top);
};

} // namespace entergram
