#pragma once

#include <glm/mat4x4.hpp>
#include <vulkan/vulkan.hpp>

struct light {
    glm::mat4x4 transform;
    vk::Image depth;
    vk::ImageView depth_view;
};
