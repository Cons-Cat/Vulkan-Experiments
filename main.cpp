#include <vulkan/vulkan.hpp>

#include <VkBootstrap.h>
#include <WSIWindow.h>
#include <cstddef>
#include <iostream>

#include "defer.hpp"

inline constexpr uint32_t game_width = 480;
inline constexpr uint32_t game_height = 320;

struct my_window final : public WSIWindow {
    // Override virtual functions.
    void OnResizeEvent(uint16_t width, uint16_t height) {
    }
};

int main() {
    // TODO: Use these to query for the extensions.
    vkb::Result maybe_system_info = vkb::SystemInfo::get_system_info();
    vkb::SystemInfo system_info = maybe_system_info.value();
    // assert(system_info.is_extension_available("VK_KHR_dynamic_rendering"));
    // std::vector ext = system_info.available_extensions;
    // for (auto& e : ext) {
    //     std::cout << e.extensionName << '\n';
    // }

    vkb::InstanceBuilder instance_builder;

// Enable WSI layers.
#ifdef VK_KHR_XCB_SURFACE_EXTENSION_NAME
    if (system_info.is_extension_available(VK_KHR_XCB_SURFACE_EXTENSION_NAME)) {
        instance_builder.enable_extension(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    }
#endif
#ifdef VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    if (system_info.is_extension_available(
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
        instance_builder.enable_extension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    }
#endif

    vkb::Result maybe_instance =
        instance_builder.request_validation_layers()
            .set_app_name("game")
            .enable_extension(VK_KHR_SURFACE_EXTENSION_NAME)
            .enable_extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME)

            //.enable_layer("VK_LAYER_NV_nomad_release_public_2023_3_2")

            .require_api_version(1, 3, 0)
            .build();
    vkb::Instance instance = maybe_instance.value();
    defer {
        vkb::destroy_instance(instance);
    };

    my_window win;
    win.SetTitle("");
    win.SetWinSize(game_width, game_height);
    vk::SurfaceKHR present_surface =
        static_cast<VkSurfaceKHR>(win.GetSurface(instance));

    vkb::PhysicalDeviceSelector physical_device_selector(instance);
    vkb::Result maybe_physical_device_selector =
        physical_device_selector.set_surface(present_surface)
            .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
            //.add_required_extension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)
            .add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)
            //.add_required_extension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME)
            .add_required_extension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)
            .select(vkb::DeviceSelectionMode::only_fully_suitable);
    vkb::PhysicalDevice physical_device =
        maybe_physical_device_selector.value();

    vkb::DeviceBuilder logical_device_builder(physical_device);

    vk::PhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature(true);
    vk::PhysicalDeviceBufferDeviceAddressFeaturesKHR device_address_feature(
        true, true, true, &dynamic_rendering_feature);
    logical_device_builder.add_pNext(&device_address_feature);

    vkb::Result maybe_device = logical_device_builder.build();
    // Create two handles to the same logical device, for use in different APIs:
    vkb::Device bootstrap_device = maybe_device.value();
    vk::Device device(bootstrap_device.device);
    defer {
        vkb::destroy_device(bootstrap_device);
    };

    vkb::Result maybe_present_queue =
        bootstrap_device.get_queue(vkb::QueueType::present);
    // vk::Queue present_queue = maybe_present_queue.value();
    // uint32_t present_queue_index =
    //     bootstrap_device.get_queue_index(vkb::QueueType::present).value();

    vkb::Result maybe_queue =
        bootstrap_device.get_queue(vkb::QueueType::graphics);
    vk::Queue queue = maybe_queue.value();
    uint32_t queue_index =
        bootstrap_device.get_queue_index(vkb::QueueType::graphics).value();

    vkb::SwapchainBuilder swapchain_builder{bootstrap_device};
    vkb::Result maybe_swapchain =
        swapchain_builder
            .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
            .set_desired_extent(game_width, game_height)
            .build();
    vkb::Swapchain swapchain = maybe_swapchain.value();
    std::vector<VkImage> swapchain_images = swapchain.get_images().value();
    std::vector<VkImageView> swapchain_image_views =
        swapchain.get_image_views().value();
    defer {
        swapchain.destroy_image_views(swapchain_image_views);
        vkb::destroy_swapchain(swapchain);
    };

    vk::Format color_format = static_cast<vk::Format>(swapchain.image_format);
    vk::Format depth_format = vk::Format::eD32Sfloat;

    /*
    vk::RenderPass render_pass =
        make_render_pass(device, color_format, depth_format);
    defer {
        device.destroyRenderPass(render_pass);
    };

    auto depth_image =
        make_image(device, physical_device.physical_device, depth_format,
                   vk::ImageAspectFlagBits::eDepth, game_width, game_height);
    defer {
        depth_image.destroy(device);
    };

    std::vector<vk::Framebuffer> framebuffers;

    for (size_t i = 0; i < swapchain_image_views.size(); ++i) {
        std::array<vk::ImageView, 2> attachments = {swapchain_image_views[i],
                                                    depth_image.view};

        vk::FramebufferCreateInfo framebuffer_create_info(
            vk::FramebufferCreateFlags{}, render_pass, attachments, game_width,
            game_height, 1);

        framebuffers.push_back(
            device.createFramebuffer(framebuffer_create_info));
    }

    defer {
        for (auto& framebuffer : framebuffers) {
            device.destroy(framebuffer);
        }
    };
 */

    // TODO: Figure out the vulkan.hpp dynamic dispatch here:
    vk::CommandPool command_pool = device.createCommandPool(
        vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlags{}, queue_index));
    defer {
        device.destroyCommandPool(command_pool);
    };

    // Allocate 1 command buffer.
    vk::CommandBuffer command_buffer =
        device
            .allocateCommandBuffers(vk::CommandBufferAllocateInfo(
                command_pool, vk::CommandBufferLevel::ePrimary, 1))
            .front();
    defer {
        device.freeCommandBuffers(command_pool, command_buffer);
    };

    vk::Semaphore image_acquired_semaphore =
        device.createSemaphore(vk::SemaphoreCreateInfo{});
    defer {
        device.destroySemaphore(image_acquired_semaphore);
    };
    vk::ResultValue<uint32_t> current_buffer = device.acquireNextImageKHR(
        swapchain.swapchain, 100'000'000, image_acquired_semaphore, nullptr);

    uint32_t swap_index = current_buffer.value;
    vk::RenderingAttachmentInfo color_attachment_info;
    color_attachment_info.setImageView(swapchain_image_views[swap_index])
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue{1, 1, 0, 1});

    vk::Rect2D rendering_dimensions({0, 0}, {game_width, game_height});
    vk::RenderingInfo rendering_info{};
    rendering_info.setColorAttachmentCount(1)
        .setRenderArea(rendering_dimensions)
        .setLayerCount(1)
        .setColorAttachmentCount(1)
        .setColorAttachments(color_attachment_info);

    command_buffer.begin(
        vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags{}));

    VkImageSubresourceRange const image_range{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };

    VkImageMemoryBarrier const image_memory_barrier_render{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = swapchain_images[swap_index],
        .subresourceRange = image_range,
    };

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &image_memory_barrier_render);

    command_buffer.beginRendering(rendering_info);

    command_buffer.endRendering();

    // Transition to present:
    VkImageMemoryBarrier const image_memory_barrier_present{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        //.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = swapchain_images[swap_index],
        .subresourceRange = image_range};

    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &image_memory_barrier_present);

    command_buffer.end();

    vk::Fence draw_fence = device.createFence(vk::FenceCreateInfo());
    defer {
        device.destroyFence(draw_fence);
    };

    vk::PipelineStageFlags wait_destination_stage_mask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);

    vk::SubmitInfo submit_info(image_acquired_semaphore,
                               wait_destination_stage_mask, command_buffer);
    queue.submit(submit_info, draw_fence);

    while (vk::Result::eTimeout ==
           device.waitForFences(draw_fence, VK_TRUE, 100'000'000))
        ;

    std::array<vk::SwapchainKHR, 1> swaps = {swapchain.swapchain};
    auto _ = queue.presentKHR(vk::PresentInfoKHR({}, swaps, swap_index));

    while (win.ProcessEvents()) {
        device.waitIdle();
    }
}
