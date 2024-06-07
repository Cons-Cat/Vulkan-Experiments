#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <vku/vku.hpp>
#pragma GCC diagnostic pop

#include <vulkan/vulkan.hpp>

#include <VkBootstrap.h>
#include <cstdint>
#include <optional>

namespace vk {
inline constinit DispatchLoaderDynamic defaultDispatchLoaderDynamic;
}

// `vulk` is a dispatcher to make Vulkan API calls on.
inline constinit auto& vulk = vk::defaultDispatchLoaderDynamic;

inline vk::Queue g_graphics_queue;
inline std::uint32_t g_graphics_queue_index;
inline vk::Queue g_present_queue;
inline std::uint32_t g_present_queue_index;

inline vkb::Swapchain g_swapchain;
inline std::vector<VkImage> g_swapchain_images{};
inline std::vector<VkImageView> g_swapchain_views{};
inline vku::ColorAttachmentImage g_color_image;
inline vku::ColorAttachmentImage g_normal_image;
inline vku::ColorAttachmentImage g_id_image;
inline vku::DepthStencilImage g_depth_image;

inline vk::CommandPool g_command_pool;
inline std::vector<vk::CommandBuffer> g_command_buffers;

inline std::vector<VkSemaphore> g_available_semaphores;
inline std::vector<VkSemaphore> g_finished_semaphore;
inline std::vector<VkFence> g_in_flight_fences;
inline std::vector<VkFence> g_image_in_flight;

inline vk::DescriptorSet g_descriptor_set;
inline vk::DescriptorSetLayout g_descriptor_layout;
inline vk::PipelineLayout g_pipeline_layout;

inline vkb::PhysicalDevice g_physical_device;
// This is `optional` to defer initialization:
inline std::optional<vkb::SwapchainBuilder> swapchain_builder;

inline vk::Device g_device;

inline constexpr uint32_t max_frames_in_flight = 3;
inline constexpr uint32_t game_width = 480;
inline constexpr uint32_t game_height = 320;
inline constexpr auto depth_format = vk::Format::eD32Sfloat;

#ifdef DEBUG_VERTICES
inline vku::VertexBuffer g_dbg_vertex_buffer;
inline vku::IndexBuffer g_dbg_index_buffer;
#endif
