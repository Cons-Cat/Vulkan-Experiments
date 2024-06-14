#pragma once

#include <glm/mat4x4.hpp>

#include <vector>

#include "globals.hpp"

struct lights {
    lights(unsigned capacity) : m_capacity(capacity) {
        transforms.reserve(capacity);
        light_maps.reserve(capacity);
    }

    struct light {
        alignas(16) glm::mat4x4 transform;
        alignas(16) glm::mat4x4 projection;
        alignas(16) glm::vec3 position;
    };

    [[nodiscard]]
    auto capacity() const -> unsigned {
        return m_capacity;
    }

    // Create a new light source.
    void push_back(light const& transform) {
        transforms.push_back(transform);
        light_maps.emplace_back(g_device, g_physical_device.memory_properties,
                                game_width, game_height, depth_format);
    }

    std::vector<light> transforms;
    std::vector<vku::DepthStencilImage> light_maps;

  private:
    unsigned m_capacity;
};

inline lights g_lights(10);
