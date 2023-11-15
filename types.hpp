#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

// ABI compatible with `entity` defined in `shaders.slang`
struct gpu_entity {
    glm::vec3 world_position;
    alignas(4) glm::quat rotation;
};
