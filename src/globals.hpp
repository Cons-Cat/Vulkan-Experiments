#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <vku/vku.hpp>
#pragma GCC diagnostic pop

#include <vulkan/vulkan_handles.hpp>

#include <VkBootstrap.h>
#include <cstdint>
#include <ktxvulkan.h>
#include <optional>

namespace vk {
inline constinit DispatchLoaderDynamic defaultDispatchLoaderDynamic;
}

// `vulk` is a dispatcher to make Vulkan API calls on.
inline constinit auto& vulk = vk::defaultDispatchLoaderDynamic;

inline constexpr std::uint32_t max_frames_in_flight = 2;
inline vk::Queue g_graphics_queue;
inline std::uint32_t g_graphics_queues_index;
inline vk::Queue g_present_queue;
inline std::uint32_t g_present_queue_index;

inline vkb::Swapchain g_swapchain;
inline std::vector<VkImage> g_swapchain_images{};
inline std::vector<VkImageView> g_swapchain_views{};
inline vku::ColorAttachmentImage g_color_image;
inline vku::ColorAttachmentImage g_normal_image;
inline vku::ColorAttachmentImage g_xyz_image;
inline vku::ColorAttachmentImage g_id_image;
inline vku::DepthStencilImage g_depth_image;

inline ktxVulkanTexture g_ktx_skybox;
inline vku::TextureImageCube g_skybox;
inline vk::ImageView g_skybox_view;

inline vk::Sampler g_nearest_neighbor_sampler;

inline vk::CommandPool g_command_pool;
inline std::vector<vk::CommandBuffer> g_command_buffers;

// TODO: Use `timeline_semaphore`.
inline std::array<vk::Semaphore, max_frames_in_flight> g_available_semaphores;
inline std::array<vk::Semaphore, max_frames_in_flight> g_finished_semaphore;
inline std::array<vk::Fence, max_frames_in_flight> g_in_flight_fences;
inline std::array<vk::Fence, max_frames_in_flight> g_image_in_flight;

inline vk::DescriptorSet g_descriptor_set;
inline vk::DescriptorSetLayout g_descriptor_layout;
inline vk::DescriptorSetLayout g_descriptor_layout_lights;
inline vk::PipelineLayout g_pipeline_layout;

// Push constants contain a 4-byte index for light rasterization passes to use
// for indexing into their respective light source.
inline constexpr vk::PushConstantRange g_push_constants = {
    vk::ShaderStageFlagBits::eVertex, 0, 4};

inline vkb::PhysicalDevice g_physical_device;
// This is `optional` to defer initialization:
inline std::optional<vkb::SwapchainBuilder> g_swapchain_builder;

inline vk::Device g_device;

inline constexpr std::uint32_t game_width = 480;
inline constexpr std::uint32_t game_height = 320;
inline constinit std::uint32_t g_screen_width = game_width;
inline constinit std::uint32_t g_screen_height = game_height;

// TODO: Dynamically select a supported depth format.
inline constexpr auto depth_format = vk::Format::eD24UnormS8Uint;

inline vku::GenericBuffer g_device_local_buffer;
inline vku::GenericBuffer g_instance_properties;
inline constinit unsigned g_next_instance_id;
