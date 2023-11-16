#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <liblava/resource/buffer.hpp>
#include <liblava/resource/primitive.hpp>

// ABI compatible with `entity` defined in `shaders.slang`
struct alignas(32) gpu_entity {
    glm::vec3 world_position;
    alignas(4) glm::quat rotation;
};

struct vertex : lava::vertex {
    alignas(4) uint32_t id;
};

using cameras = glm::mat4[2];

inline cameras viewproj;

inline gpu_entity bindless_data[2] = {
    {{0, 0, 0}, {1, 0, 0, 1}},
    {{0, 0, 0}, {1, 0, 0, 1}},
};

inline constexpr int game_width = 480;
inline constexpr int game_height = 320;

// TODO: This should be an inplace vertex.
inline std::vector<vertex> render_vertices(1024);
inline std::vector<uint32_t> render_indices(1024);
inline lava::buffer::ptr vertices_buffer = nullptr;
inline lava::buffer::ptr indices_buffer = nullptr;

inline lava::device_p device = nullptr;
