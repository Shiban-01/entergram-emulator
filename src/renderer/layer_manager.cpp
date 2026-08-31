#include "layer_manager.hpp"
#include <algorithm>

namespace entergram {

LayerManager::LayerManager() = default;
LayerManager::~LayerManager() = default;

int LayerManager::add_layer(const LayerProperties& props) {
    Layer layer;
    layer.id = next_id_++;
    layer.props = props;
    layers_.push_back(std::move(layer));
    return layer.id;
}

void LayerManager::remove_layer(int layer_id) {
    layers_.erase(
        std::remove_if(layers_.begin(), layers_.end(),
            [layer_id](const Layer& l) { return l.id == layer_id; }),
        layers_.end()
    );
}

int LayerManager::find_layer(const std::string& name) const {
    for (const auto& layer : layers_) {
        if (layer.props.name == name) {
            return layer.id;
        }
    }
    return -1;
}

void LayerManager::load_picture(int layer_id, const std::string& file_path) {
    for (auto& layer : layers_) {
        if (layer.id == layer_id) {
            layer.picture_path = file_path;
            return;
        }
    }
}

void LayerManager::set_properties(int layer_id, const LayerProperties& props) {
    for (auto& layer : layers_) {
        if (layer.id == layer_id) {
            layer.props = props;
            return;
        }
    }
}

const LayerProperties& LayerManager::get_properties(int layer_id) const {
    static LayerProperties empty;
    for (const auto& layer : layers_) {
        if (layer.id == layer_id) {
            return layer.props;
        }
    }
    return empty;
}

void LayerManager::set_alpha(int layer_id, float alpha) {
    for (auto& layer : layers_) {
        if (layer.id == layer_id) {
            layer.props.alpha = alpha;
            return;
        }
    }
}

void LayerManager::move_to(int layer_id, float x, float y) {
    for (auto& layer : layers_) {
        if (layer.id == layer_id) {
            layer.props.x = x;
            layer.props.y = y;
            return;
        }
    }
}

// ---- VM callback methods ----

void LayerManager::load_layer(int layer_id, int layer_type, int param1, int param2) {
    LayerProperties props;
    props.type = static_cast<LayerType>(layer_type);
    props.x = static_cast<float>(param1);
    props.y = static_cast<float>(param2);
    props.visible = true;
    props.alpha = 1.0f;
    props.z_order = layer_id;
    int id = add_layer(props);
    // Store the VM-assigned layer_id as the name for lookup
    for (auto& layer : layers_) {
        if (layer.id == id) {
            layer.props.name = "layer_" + std::to_string(layer_id);
        }
    }
}

void LayerManager::update_layer(int layer_id, int property_id, int target, int duration, int flags, int easing) {
    // property_id determines what to change:
    // 0 = position, 1 = alpha, 2 = color, 3 = scale, etc.
    // For now, update properties on layers matching the VM layer_id
    for (auto& layer : layers_) {
        if (layer.props.name == "layer_" + std::to_string(layer_id)) {
            switch (property_id) {
                case 0: // position
                    layer.props.x = static_cast<float>(target);
                    break;
                case 1: // alpha (0-255 range)
                    layer.props.alpha = static_cast<float>(target) / 255.0f;
                    break;
                case 2: // color
                    layer.props.color = static_cast<uint32_t>(target) | 0xFF000000;
                    break;
            }
            layer.props.visible = true;
            return;
        }
    }
}

void LayerManager::unload_layer(int layer_id) {
    // Remove all layers matching this VM layer_id
    layers_.erase(
        std::remove_if(layers_.begin(), layers_.end(),
            [layer_id](const Layer& l) {
                return l.props.name == "layer_" + std::to_string(layer_id);
            }),
        layers_.end()
    );
}

void LayerManager::move_sprite(int layer_id, int x, int y, int duration, int flags) {
    for (auto& layer : layers_) {
        if (layer.props.name == "layer_" + std::to_string(layer_id)) {
            layer.props.x = static_cast<float>(x);
            layer.props.y = static_cast<float>(y);
            return;
        }
    }
}

void LayerManager::set_sprite_alpha(int layer_id, int alpha, int duration, int flags) {
    for (auto& layer : layers_) {
        if (layer.props.name == "layer_" + std::to_string(layer_id)) {
            layer.props.alpha = static_cast<float>(alpha) / 255.0f;
            return;
        }
    }
}

std::vector<const LayerProperties*> LayerManager::get_render_list() const {
    std::vector<const LayerProperties*> render_list;

    for (const auto& layer : layers_) {
        if (layer.props.visible) {
            render_list.push_back(&layer.props);
        }
    }

    // Sort by Z-order (back to front)
    std::sort(render_list.begin(), render_list.end(),
        [](const LayerProperties* a, const LayerProperties* b) {
            return a->z_order < b->z_order;
        });

    return render_list;
}

void LayerManager::clear() {
    layers_.clear();
    next_id_ = 1;
}

} // namespace entergram
