#pragma once

#include <globals.hpp>

auto make_device(vkb::Instance instance, vk::SurfaceKHR surface) -> vk::Device;

void create_swapchain();
void create_command_pool();
void create_command_buffers();
void create_sync_objects();
void recreate_swapchain();
void render_and_present();
void record_rendering(std::size_t);
void set_all_render_state(vk::CommandBuffer cmd);
