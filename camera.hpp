#pragma once

#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>

#include "globals.hpp"

// TODO: Make this `constexpr`.
inline glm::mat4 const projection_matrix = glm::perspective(
    glm::radians(70.f),
    static_cast<float>(game_width) / static_cast<float>(game_height), 10000.f,
    0.1f);

struct camera_t {
    // TODO: Updating position should be done elsewhere.
    glm::vec3 position = {0, 0, 0};

    float pitch = 0;
    float yaw = 0;

    [[nodiscard]]
    auto make_rotation_matrix() const -> glm::mat4;

    [[nodiscard]]
    auto make_view_matrix() const -> glm::mat4;

    void update();
};

inline camera_t g_camera{};
