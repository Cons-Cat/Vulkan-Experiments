#include "camera.hpp"

#include <glm/fwd.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>

auto camera_t::make_rotation_matrix() const -> glm::mat4x4 {
    glm::quat pitch_rotation = glm::angleAxis(pitch, glm::vec3{1.f, 0.f, 0.f});
    glm::quat yaw_rotation = glm::angleAxis(yaw, glm::vec3{0.f, -1.f, 0.f});

    return glm::toMat4(yaw_rotation) * glm::toMat4(pitch_rotation);
}

auto camera_t::make_view_matrix() const -> glm::mat4x4 {
    glm::mat4x4 camera_translation = glm::translate(glm::mat4x4(1.f), position);
    glm::mat4x4 camera_rotation = make_rotation_matrix();
    return glm::inverse(camera_translation * camera_rotation);
}

void camera_t::update() {
    // glm::mat4x4 camera_rotation = get_rotation_matrix();
    // position += glm::vec3(camera_rotation * glm::vec4(velocity * 0.5f, 0.f));
}
