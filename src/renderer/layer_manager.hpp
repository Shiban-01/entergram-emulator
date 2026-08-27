#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

// Forward declarations
namespace entergram {
    class RomReader;
    class VideoPlayer;
}

namespace entergram {

// A visual novel layer — sprites, backgrounds, text, and video layers
// managed by the SNR VM's LOAD/LAYER/MOVE commands.
//
// The Entergram engine uses a layer-based compositing system:
// - Multiple layers stacked on top of each other
// - Each layer can have a sprite/picture loaded
// - Alpha blending for transitions
// - Position/size can be animated via MOVE commands
enum class LayerType {
    Background,  // Full-screen background
    Sprite,      // Character sprite (bustup)
    Text,        // Text box
    Movie,       // Video layer (intro, cutscenes)
    System,      // System UI (menu, choices)
};

struct LayerProperties {
    std::string name;
    LayerType type = LayerType::Sprite;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float alpha = 1.0f;        // 0.0 = transparent, 1.0 = opaque
    bool visible = true;
    int z_order = 0;           // Render order (higher = on top)
    uint32_t color = 0xFFFFFFFF;  // Tint color (RGBA)
};

// Layer manager — manages all visual layers and renders them in Z-order.
class LayerManager {
public:
    LayerManager();
    ~LayerManager();

    // Add a new layer
    int add_layer(const LayerProperties& props);

    // Remove a layer by ID
    void remove_layer(int layer_id);

    // Find a layer by name
    int find_layer(const std::string& name) const;

    // Load a picture into a layer
    void load_picture(int layer_id, const std::string& file_path);

    // Set layer properties
    void set_properties(int layer_id, const LayerProperties& props);

    // Get layer properties
    const LayerProperties& get_properties(int layer_id) const;

    // Set alpha blending
    void set_alpha(int layer_id, float alpha);

    // Move a layer to absolute coordinates
    void move_to(int layer_id, float x, float y);

    // Get all visible layers sorted by Z-order
    std::vector<const LayerProperties*> get_render_list() const;

    // Clear all layers
    void clear();

private:
    struct Layer {
        int id;
        LayerProperties props;
        std::string picture_path;
    };

    std::vector<Layer> layers_;
    int next_id_ = 1;
};

} // namespace entergram
