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
