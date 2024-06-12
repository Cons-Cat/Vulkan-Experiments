#pragma once

#include <globals.hpp>

auto make_device(vkb::Instance instance, vk::SurfaceKHR surface) -> vk::Device;

void create_swapchain();
void create_command_pool();
void create_command_buffers();
void create_sync_objects();
void recreate_swapchain();
void render_and_present();
void record_rendering(vk::CommandBuffer& cmd);
void record_compositing(vk::CommandBuffer& cmd, std::size_t frame);
void record();
void set_all_render_state(vk::CommandBuffer cmd);
