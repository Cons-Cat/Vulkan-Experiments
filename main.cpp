#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

// Include this before `vku/vku.hpp`.
#include "defer.hpp"
//

#include <vku/vku.hpp>
#include <vulkan/vulkan.hpp>

#include <VkBootstrap.h>
#include <WSIWindow.h>
#include <cstddef>
#include <fstream>
#include <iostream>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

inline constexpr uint32_t max_frames_in_flight = 3;
inline constexpr uint32_t game_width = 480;
inline constexpr uint32_t game_height = 320;
inline constexpr auto depth_format = vk::Format::eD32Sfloat;

inline std::vector<vk::DescriptorSet> g_descriptor_sets;

inline vkb::PhysicalDevice g_physical_device;
inline std::optional<vkb::SwapchainBuilder> swapchain_builder;

inline vk::Queue g_graphics_queue;
inline uint32_t g_graphics_queue_index;
inline vk::Queue g_present_queue;
inline uint32_t g_present_queue_index;
inline vk::Queue g_transfer_queue;
inline uint32_t g_transfer_queue_index;

inline vkb::Swapchain g_swapchain;
inline std::vector<VkImage> g_swapchain_images{};
inline std::vector<VkImageView> g_swapchain_views{};
// std::vector<vku::ColorAttachmentImage> g_swapchain_images;
inline vku::DepthStencilImage g_depth_image;

inline VkCommandPool g_command_pool;
inline std::vector<vk::CommandBuffer> g_command_buffers;

inline std::vector<VkSemaphore> g_available_semaphores;
inline std::vector<VkSemaphore> g_finished_semaphore;
inline std::vector<VkFence> g_in_flight_fences;
inline std::vector<VkFence> g_image_in_flight;

auto make_device(vkb::Instance instance, vk::SurfaceKHR surface) -> vk::Device {
    vkb::PhysicalDeviceSelector physical_device_selector(instance);
    physical_device_selector
        .add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)
        .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
        .add_required_extension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);

    auto maybe_physical_device =
        physical_device_selector.set_surface(surface).select();
    if (!maybe_physical_device) {
        std::cout << maybe_physical_device.error().message() << "\n";
    }
    g_physical_device = maybe_physical_device.value();

    vk::PhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature(true);
    vk::PhysicalDeviceBufferDeviceAddressFeaturesKHR device_address_feature(
        true, true, true, &dynamic_rendering_feature);
    vk::PhysicalDeviceShaderObjectFeaturesEXT shader_object_feature(
        true, &device_address_feature);

    vkb::DeviceBuilder device_builder{g_physical_device};
    device_builder.add_pNext(&shader_object_feature);
    auto maybe_device = device_builder.build();
    if (!maybe_device) {
        std::cout << maybe_device.error().message() << "\n";
    }
    auto device = maybe_device.value();

    // Initialize queues.
    g_graphics_queue = device.get_queue(vkb::QueueType::graphics).value();
    g_graphics_queue_index =
        device.get_queue_index(vkb::QueueType::graphics).value();

    g_present_queue = device.get_queue(vkb::QueueType::present).value();
    g_present_queue_index =
        device.get_queue_index(vkb::QueueType::present).value();

    g_transfer_queue = device.get_queue(vkb::QueueType::transfer).value();
    g_transfer_queue_index =
        device.get_queue_index(vkb::QueueType::transfer).value();

    swapchain_builder = vkb::SwapchainBuilder{device};

    return device.device;
}

inline vk::Device device;

void create_swapchain() {
    // Initialize swapchain.
    auto maybe_swapchain =
        swapchain_builder->set_old_swapchain(g_swapchain).build();
    if (!maybe_swapchain) {
        std::cout << maybe_swapchain.error().message() << " "
                  << maybe_swapchain.vk_result() << "\n";
    }

    // Destroy the old swapchain if it exists, and create a new one.
    // vkb::destroy_swapchain(g_swapchain);
    g_swapchain = maybe_swapchain.value();
    g_swapchain_images = g_swapchain.get_images().value();
    g_swapchain_views = g_swapchain.get_image_views().value();

    // for (int i = 0; i < g_swapchain.image_count; ++i) {
    //     g_swapchain_images.emplace_back(
    //         device.device, g_physical_device.memory_properties, game_width,
    //         game_height, vk::Format::eR8G8B8A8Unorm);
    // }
}

auto read_file(std::filesystem::path const& file_name) -> vector<char> {
    std::ifstream file(file_name, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(file_size));

    file.close();

    return buffer;
}

struct shader_objects_t {
    std::vector<vk::ShaderEXT> objects;

    void add_shader(
        std::filesystem::path const& shader_path,
        vk::ShaderStageFlagBits shader_stage,
        vk::ShaderStageFlagBits next_stage = vk::ShaderStageFlagBits{0}) {
        std::vector spirv_source = read_file(shader_path);

        VkShaderCreateInfoEXT info =
            vk::ShaderCreateInfoEXT{}
                .setStage(shader_stage)
                .setNextStage(next_stage)
                .setCodeType(vk::ShaderCodeTypeEXT::eSpirv)
                .setPName("main")
                .setCodeSize(spirv_source.size())
                .setPCode(spirv_source.data());

        VkShaderEXT p_shader;

        VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateShadersEXT(device, 1, &info,
                                                         nullptr, &p_shader);
        objects.emplace_back(p_shader);
    }

    void add_vertex_shader(std::filesystem::path const& shader_path) {
        add_shader(shader_path, vk::ShaderStageFlagBits::eVertex,
                   vk::ShaderStageFlagBits::eFragment);
    }

    void add_fragment_shader(std::filesystem::path const& shader_path) {
        add_shader(shader_path, vk::ShaderStageFlagBits::eFragment);
    }

    void bind_vertex(vk::CommandBuffer cmd, uint32_t index) {
        auto vert_bit = VK_SHADER_STAGE_VERTEX_BIT;
        VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBindShadersEXT(
            cmd, 1, &vert_bit, (VkShaderEXT*)&objects[index]);
    }

    void bind_fragment(vk::CommandBuffer cmd, uint32_t index) {
        auto frag_bit = VK_SHADER_STAGE_FRAGMENT_BIT;
        VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBindShadersEXT(
            cmd, 1, &frag_bit, (VkShaderEXT*)&objects[index]);
    }

    void destroy() {
        for (auto& shader : objects) {
            VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyShaderEXT(device, shader,
                                                             nullptr);
        }
    }
} shader_objects;

void create_command_pool() {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = g_graphics_queue_index;

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateCommandPool(device, &pool_info,
                                                      nullptr, &g_command_pool);
}

void create_command_buffers() {
    vk::CommandBufferAllocateInfo info{
        g_command_pool, vk::CommandBufferLevel::ePrimary, max_frames_in_flight};
    g_command_buffers = device.allocateCommandBuffers(info);
}

void create_sync_objects() {
    g_available_semaphores.resize(max_frames_in_flight);
    g_finished_semaphore.resize(max_frames_in_flight);
    g_in_flight_fences.resize(max_frames_in_flight);
    g_image_in_flight.resize(g_swapchain.image_count, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < max_frames_in_flight; i++) {
        VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateSemaphore(
            device, &semaphore_info, nullptr, &g_available_semaphores[i]);

        VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateSemaphore(
            device, &semaphore_info, nullptr, &g_finished_semaphore[i]);

        VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateFence(
            device, &fence_info, nullptr, &g_in_flight_fences[i]);
    }
}

void recreate_swapchain();

void render_and_present() {
    static uint32_t frame = 0;

    auto timeout = std::numeric_limits<uint64_t>::max();

    // Wait for host to signal the fence for this swapchain frame.
    VULKAN_HPP_DEFAULT_DISPATCHER.vkWaitForFences(
        device, 1, &g_in_flight_fences[frame], vk::True, timeout);

    // Get a swapchain index that is currently presentable.
    uint32_t image_index;
    auto error = static_cast<vk::Result>(
        VULKAN_HPP_DEFAULT_DISPATCHER.vkAcquireNextImageKHR(
            device, g_swapchain.swapchain, timeout,
            g_available_semaphores[frame], nullptr, &image_index));

    if (error == vk::Result::eErrorOutOfDateKHR) {
        recreate_swapchain();
        return;
    }

    if (g_image_in_flight[image_index] != VK_NULL_HANDLE) {
        VULKAN_HPP_DEFAULT_DISPATCHER.vkWaitForFences(
            device, 1, &g_image_in_flight[image_index], vk::True, timeout);
    }

    g_image_in_flight[image_index] = g_in_flight_fences[frame];

    std::array<vk::Semaphore, 1> wait_semaphores = {
        g_available_semaphores[frame],
    };
    std::array<vk::Semaphore, 1> signal_semaphores = {
        g_finished_semaphore[frame],
    };
    std::array<vk::PipelineStageFlags, 1> wait_stages = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
    };

    // Submit commands to the graphics queue.
    vk::SubmitInfo submit_info;
    submit_info.setWaitSemaphores(wait_semaphores)
        .setWaitDstStageMask(wait_stages)
        .setCommandBufferCount(1)
        .setPCommandBuffers(&g_command_buffers[image_index])
        .setSignalSemaphores(signal_semaphores);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkResetFences(device, 1,
                                                &g_in_flight_fences[frame]);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkQueueSubmit(g_graphics_queue, 1,
                                                (VkSubmitInfo*)&submit_info,
                                                g_in_flight_fences[frame]);

    // After rendering to the swapchain frame completes, present it to the
    // surface.
    std::array present_indices = {image_index};
    std::array<vk::SwapchainKHR, 1> swapchains = {g_swapchain.swapchain};
    vk::PresentInfoKHR present_info;
    present_info.setImageIndices(present_indices)
        .setWaitSemaphores(signal_semaphores)
        .setSwapchains(swapchains);

    error = (vk::Result)VULKAN_HPP_DEFAULT_DISPATCHER.vkQueuePresentKHR(
        g_present_queue, (VkPresentInfoKHR*)&present_info);
    if (error == vk::Result::eErrorOutOfDateKHR) {
        recreate_swapchain();
        return;
    }

    frame = (frame + 1) % max_frames_in_flight;
}

void set_all_render_state(vk::CommandBuffer cmd) {
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetLineWidth(cmd, 1.0);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetPolygonModeEXT(cmd,
                                                         VK_POLYGON_MODE_FILL);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetDepthWriteEnable(cmd, VK_FALSE);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetRasterizerDiscardEnable(cmd,
                                                                  VK_FALSE);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetRasterizationSamplesEXT(
        cmd, VK_SAMPLE_COUNT_1_BIT);

    VkSampleMask sample_mask = 0x1;
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetSampleMaskEXT(
        cmd, VK_SAMPLE_COUNT_1_BIT, &sample_mask);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetAlphaToCoverageEnableEXT(cmd,
                                                                   VK_FALSE);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetPrimitiveTopology(
        cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetPrimitiveRestartEnable(cmd, VK_FALSE);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetVertexInputEXT(cmd, 0, nullptr, 0,
                                                         nullptr);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetDepthClampEnableEXT(cmd, VK_FALSE);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetDepthBiasEnable(cmd, VK_FALSE);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetDepthTestEnable(cmd, VK_TRUE);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetDepthWriteEnable(cmd, VK_TRUE);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetDepthBoundsTestEnable(cmd, VK_FALSE);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetFrontFace(cmd,
                                                    VK_FRONT_FACE_CLOCKWISE);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetDepthCompareOp(
        cmd, VK_COMPARE_OP_LESS_OR_EQUAL);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetStencilTestEnable(cmd, VK_FALSE);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetLogicOpEnableEXT(cmd, VK_FALSE);

    VkBool32 color_blend = VK_FALSE;
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetColorBlendEnableEXT(cmd, 0, 1,
                                                              &color_blend);

    VkColorComponentFlags color_write_mask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetColorWriteMaskEXT(cmd, 0, 1,
                                                            &color_write_mask);
}

void record_rendering(uint32_t const frame) {
    vk::CommandBuffer& cmd = g_command_buffers[frame];
    vk::CommandBufferBeginInfo begin_info;
    cmd.begin(begin_info);

    vk::ClearColorValue clear_color = {
        std::array{1.f, 0.f, 1.f, 0.f}
    };

    vk::Viewport viewport;
    viewport.setWidth(game_width)
        .setHeight(game_height)
        .setX(0)
        .setY(0)
        .setMinDepth(0.f)
        .setMaxDepth(1.f);
    vk::Rect2D scissor;
    scissor.setOffset({0, 0}).setExtent(g_swapchain.extent);
    vk::Rect2D render_area;
    render_area.setOffset({0, 0}).setExtent(g_swapchain.extent);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetViewportWithCount(
        cmd, 1, (VkViewport*)&viewport);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdSetScissorWithCount(cmd, 1,
                                                           (VkRect2D*)&scissor);

    vk::RenderingAttachmentInfoKHR color_attachment_info;
    color_attachment_info.setClearValue(clear_color)
        .setImageLayout(vk::ImageLayout::eAttachmentOptimal)
        .setImageView(g_swapchain_views[frame])
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(clear_color);

    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(render_area)
        .setLayerCount(1)
        .setPColorAttachments(&color_attachment_info)
        .setColorAttachmentCount(1);

    //
    VkImageMemoryBarrier const render_memory_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = g_swapchain_images[frame],
        .subresourceRange = {
                             .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1,
                             }
    };

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,              // srcStageMask
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // dstStageMask
        0, 0, nullptr, 0, nullptr,
        1,                      // imageMemoryBarrierCount
        &render_memory_barrier  // pImageMemoryBarriers
    );

    // g_swapchain_images[frame].setLayout(
    //     cmd, vk::ImageLayout::eColorAttachmentOptimal);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBeginRendering(
        cmd, (VkRenderingInfo*)&rendering_info);

    set_all_render_state(cmd);

    shader_objects.bind_vertex(cmd, 0);
    shader_objects.bind_fragment(cmd, 1);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdDraw(cmd, 3, 1, 0, 0);

    VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdEndRendering(cmd);

    // swapchain_images[frame].setLayout(cmd, vk::ImageLayout::ePresentSrcKHR);

    //
    VkImageMemoryBarrier const present_memory_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = g_swapchain_images[frame],
        .subresourceRange = {
                             .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1,
                             }
    };

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,           // dstStageMask
        0, 0, nullptr, 0, nullptr,
        1,                       // imageMemoryBarrierCount
        &present_memory_barrier  // pImageMemoryBarriers
    );

    cmd.end();
}

void recreate_swapchain() {
    device.waitIdle();
    device.destroyCommandPool(g_command_pool);
    g_swapchain.destroy_image_views(g_swapchain_views);

    create_swapchain();
    create_command_pool();
    create_command_buffers();

    for (uint32_t i = 0; i < g_command_buffers.size(); ++i) {
        record_rendering(i);
    }
}

struct my_window final : public WSIWindow {
    // Override virtual functions.
    void OnResizeEvent(uint16_t width, uint16_t height) final {
    }
};

auto main() -> int {
    vk::DynamicLoader vkloader;
    VULKAN_HPP_DEFAULT_DISPATCHER.init();

    vkb::InstanceBuilder instance_builder;
    auto maybe_instance = instance_builder.request_validation_layers()
                              .require_api_version(1, 3, 0)
                              .build();
    if (!maybe_instance) {
        std::cout << maybe_instance.error().message() << "\n";
    }
    vkb::Instance instance = maybe_instance.value();

    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance{instance.instance});

    my_window win;
    win.SetTitle("");
    win.SetWinSize(game_width, game_height);
    vk::SurfaceKHR surface =
        static_cast<VkSurfaceKHR>(win.GetSurface(instance));

    // Initialize global device.
    device = make_device(instance, surface);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

    create_swapchain();
    defer {
        vkb::destroy_swapchain(g_swapchain);
    };

    // Compile and link shaders.
    shader_objects.add_vertex_shader("/home/conscat/game/demo_vertex.spv");
    shader_objects.add_fragment_shader("/home/conscat/game/demo_fragment.spv");
    defer {
        shader_objects.destroy();
    };

    g_depth_image =
        vku::DepthStencilImage(device, g_physical_device.memory_properties,
                               game_width, game_height, depth_format);

    vku::DescriptorSetLayoutMaker dslm{};
    dslm.buffer(
        0U, vk::DescriptorType::eStorageBuffer,
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        1);
    dslm.image(1U, vk::DescriptorType::eCombinedImageSampler,
               vk::ShaderStageFlagBits::eFragment, 1);
    auto layout = dslm.createUnique(device);

    std::vector<vk::DescriptorPoolSize> pool_sizes;
    pool_sizes.emplace_back(vk::DescriptorType::eStorageBuffer, 1);
    pool_sizes.emplace_back(vk::DescriptorType::eCombinedImageSampler, 3);

    // Create an arbitrary number of descriptors in a pool.
    // Allow the descriptors to be freed, possibly not optimal behaviour.
    vk::DescriptorPoolCreateInfo descriptor_pool_info{};
    descriptor_pool_info.flags =
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    descriptor_pool_info.maxSets = 1;
    descriptor_pool_info.poolSizeCount = pool_sizes.size();
    descriptor_pool_info.pPoolSizes = pool_sizes.data();
    auto descriptor_pool =
        device.createDescriptorPoolUnique(descriptor_pool_info);

    vku::DescriptorSetMaker dsm{};
    dsm.layout(*layout);
    g_descriptor_sets = dsm.create(device, descriptor_pool.get());

    create_command_pool();
    defer {
        device.destroyCommandPool(g_command_pool);
    };

    create_sync_objects();
    defer {
        for (auto& semaphore : g_finished_semaphore) {
            device.destroySemaphore(semaphore);
        }
        for (auto& semaphore : g_available_semaphores) {
            device.destroySemaphore(semaphore);
        }
        for (auto& fence : g_in_flight_fences) {
            device.destroyFence(fence);
        }
    };

    create_command_buffers();

    for (uint32_t i = 0; i < g_command_buffers.size(); ++i) {
        record_rendering(i);
    }

    // Game loop.
    while (win.ProcessEvents()) {
        if (win.GetKeyState(eKeycode::KEY_Escape)) {
            win.Close();
        }
        render_and_present();
    }

    VULKAN_HPP_DEFAULT_DISPATCHER.vkDeviceWaitIdle(device);
}
