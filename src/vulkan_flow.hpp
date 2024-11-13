#pragma once

#include <globals.hpp>

auto make_device(vkb::Instance instance, vk::SurfaceKHR surface) -> vk::Device;

void create_first_swapchain();
void create_command_pool();
void create_command_buffers();
void create_sync_objects();
void update_descriptors();
void recreate_swapchain();
void draw_skybox(vk::CommandBuffer cmd);
void record_skybox(vk::CommandBuffer cmd);
void render_and_present(unsigned);
void record_rendering(vk::CommandBuffer cmd);
void record_lights(vk::CommandBuffer cmd);
void record_compositing(vk::CommandBuffer cmd, std::size_t frame);
void record_frame(unsigned int i);
void set_all_render_state(vk::CommandBuffer cmd);
