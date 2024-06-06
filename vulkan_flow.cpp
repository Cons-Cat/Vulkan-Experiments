#include "vulkan_flow.hpp"

#include "bindless.hpp"
#include "globals.hpp"
#include "shader_objects.hpp"

auto make_device(vkb::Instance instance, vk::SurfaceKHR surface) -> vk::Device {
    vkb::PhysicalDeviceSelector physical_device_selector(instance);
    physical_device_selector
        .add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)
        .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
        .add_required_extension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);

    auto maybe_physical_device =
        physical_device_selector.set_surface(surface).select();
    if (!maybe_physical_device) {
        std::cout << maybe_physical_device.error().message() << '\n';
        std::quick_exit(1);
    }
    g_physical_device = *maybe_physical_device;
    std::cout << g_physical_device.name << '\n';

    vk::PhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature(
        vk::True);
    vk::PhysicalDeviceBufferDeviceAddressFeaturesKHR device_address_feature(
        vk::True, vk::True, vk::True, &dynamic_rendering_feature);
    vk::PhysicalDeviceShaderObjectFeaturesEXT shader_object_feature(
        vk::True, &device_address_feature);

    vkb::DeviceBuilder device_builder{g_physical_device};
    device_builder.add_pNext(&shader_object_feature);
    auto maybe_device = device_builder.build();
    if (!maybe_device) {
        std::cout << maybe_device.error().message() << '\n';
    }
    auto device = *maybe_device;

    // Initialize queues.
    g_graphics_queue = *device.get_queue(vkb::QueueType::graphics);
    g_graphics_queue_index = *device.get_queue_index(vkb::QueueType::graphics);

    g_present_queue = *device.get_queue(vkb::QueueType::present);
    g_present_queue_index = *device.get_queue_index(vkb::QueueType::present);

    swapchain_builder = vkb::SwapchainBuilder{device};

    return device.device;
}

void create_swapchain() {
    assert(swapchain_builder);

    // Initialize swapchain.
    auto maybe_swapchain =
        swapchain_builder->set_old_swapchain(g_swapchain).build();
    if (!maybe_swapchain) {
        std::cout << maybe_swapchain.error().message() << ' '
                  << maybe_swapchain.vk_result() << '\n';
    }

    // Destroy the old swapchain if it exists, and create a new one.
    // vkb::destroy_swapchain(g_swapchain);
    g_swapchain = *maybe_swapchain;
    g_swapchain_images = *g_swapchain.get_images();
    g_swapchain_views = *g_swapchain.get_image_views();
}

void create_command_pool() {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = g_graphics_queue_index;

    vulk.vkCreateCommandPool(g_device, &pool_info, nullptr, &g_command_pool);
}

void create_command_buffers() {
    vk::CommandBufferAllocateInfo info{
        g_command_pool, vk::CommandBufferLevel::ePrimary, max_frames_in_flight};
    g_command_buffers = g_device.allocateCommandBuffers(info);
}

void create_sync_objects() {
    g_available_semaphores.resize(max_frames_in_flight);
    g_finished_semaphore.resize(max_frames_in_flight);
    g_in_flight_fences.resize(max_frames_in_flight);
    g_image_in_flight.resize(g_swapchain.image_count, VK_NULL_HANDLE);

    // TODO: Use hpp bindings for this.

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < max_frames_in_flight; i++) {
        vulk.vkCreateSemaphore(g_device, &semaphore_info, nullptr,
                               &g_available_semaphores[i]);

        vulk.vkCreateSemaphore(g_device, &semaphore_info, nullptr,
                               &g_finished_semaphore[i]);

        vulk.vkCreateFence(g_device, &fence_info, nullptr,
                           &g_in_flight_fences[i]);
    }
}

void render_and_present() {
    static uint32_t frame = 0;

    constexpr auto timeout = std::numeric_limits<uint64_t>::max();

    // Wait for host to signal the fence for this swapchain frame.
    vulk.vkWaitForFences(g_device, 1, &g_in_flight_fences[frame], vk::True,
                         timeout);

    // Get a swapchain index that is currently presentable.
    uint32_t image_index;
    auto error = static_cast<vk::Result>(vulk.vkAcquireNextImageKHR(
        g_device, g_swapchain.swapchain, timeout, g_available_semaphores[frame],
        nullptr, &image_index));

    if (error == vk::Result::eErrorOutOfDateKHR) {
        recreate_swapchain();
        return;
    }

    if (g_image_in_flight[image_index] != VK_NULL_HANDLE) {
        vulk.vkWaitForFences(g_device, 1, &g_image_in_flight[image_index],
                             vk::True, timeout);
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

    vulk.vkResetFences(g_device, 1, &g_in_flight_fences[frame]);

    vulk.vkQueueSubmit(g_graphics_queue, 1, (VkSubmitInfo*)&submit_info,
                       g_in_flight_fences[frame]);

    // After rendering to the swapchain frame completes, present it to the
    // surface.
    std::array present_indices = {image_index};
    std::array<vk::SwapchainKHR, 1> swapchains = {g_swapchain.swapchain};
    vk::PresentInfoKHR present_info;
    present_info.setImageIndices(present_indices)
        .setWaitSemaphores(signal_semaphores)
        .setSwapchains(swapchains);

    error = (vk::Result)vulk.vkQueuePresentKHR(
        g_present_queue, (VkPresentInfoKHR*)&present_info);
    if (error == vk::Result::eErrorOutOfDateKHR) {
        recreate_swapchain();
        return;
    }

    frame = (frame + 1) % max_frames_in_flight;
}

void set_all_render_state(vk::CommandBuffer cmd) {
    cmd.setLineWidth(1.0);
    cmd.setCullMode(vk::CullModeFlagBits::eNone);
    cmd.setPolygonModeEXT(vk::PolygonMode::eFill);
    vk::ColorBlendEquationEXT color_blend_equations[4]{};
    cmd.setColorBlendEquationEXT(4, color_blend_equations);
    cmd.setRasterizerDiscardEnable(vk::False);
    cmd.setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);

    VkSampleMask sample_mask = 0x1;
    cmd.setSampleMaskEXT(vk::SampleCountFlagBits::e1, &sample_mask);
    cmd.setAlphaToCoverageEnableEXT(vk::False);

    cmd.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
    cmd.setPrimitiveRestartEnable(vk::False);

    cmd.setVertexInputEXT(0, nullptr, 0, nullptr);

    cmd.setDepthClampEnableEXT(vk::False);

    cmd.setDepthBiasEnable(vk::False);
    cmd.setDepthTestEnable(vk::True);
    cmd.setDepthWriteEnable(vk::True);
    cmd.setDepthBoundsTestEnable(vk::False);

    cmd.setFrontFace(vk::FrontFace::eClockwise);
    cmd.setDepthCompareOp(vk::CompareOp::eLessOrEqual);

    cmd.setStencilTestEnable(vk::False);

    cmd.setLogicOpEnableEXT(vk::False);

    cmd.setColorBlendEnableEXT(0, vk::False);
    cmd.setColorBlendEnableEXT(1, vk::False);
    cmd.setColorBlendEnableEXT(2, vk::False);

    constexpr vk::Flags color_write_mask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    cmd.setColorWriteMaskEXT(0, 1, &color_write_mask);

    constexpr vk::Flags normal_write_mask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    cmd.setColorWriteMaskEXT(1, 1, &normal_write_mask);

    constexpr vk::Flags id_write_mask = vk::ColorComponentFlagBits::eR;
    cmd.setColorWriteMaskEXT(2, 1, &id_write_mask);

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, g_pipeline_layout,
                           0, g_descriptor_set, nullptr);
}

void record_rendering(std::size_t const frame) {
    vk::CommandBuffer& cmd = g_command_buffers[frame];
    vk::CommandBufferBeginInfo begin_info;
    cmd.begin(begin_info);

    vk::ClearColorValue clear_color = {1.f, 0.f, 1.f, 0.f};
    vk::ClearColorValue black_clear_color = {0, 0, 0, 1};
    vk::ClearColorValue depth_clear_color = {1.f, 0.f, 1.f, 1.f};

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

    cmd.setViewportWithCount(1, &viewport);
    cmd.setScissorWithCount(1, &scissor);

    vk::RenderingAttachmentInfoKHR color_attachment_info;
    color_attachment_info.setClearValue(clear_color)
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(g_color_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfoKHR normal_attachment_info;
    normal_attachment_info.setClearValue(black_clear_color)
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(g_normal_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfoKHR id_attachment_info;
    id_attachment_info.setClearValue(black_clear_color)
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(g_id_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setResolveMode(vk::ResolveModeFlagBits::eNone);

    std::array attachments = {color_attachment_info, normal_attachment_info,
                              id_attachment_info};

    vk::RenderingAttachmentInfoKHR depth_attachment_info;
    depth_attachment_info.setClearValue(depth_clear_color)
        .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
        .setImageView(g_depth_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setResolveMode(vk::ResolveModeFlagBits::eNone);

    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(render_area)
        .setLayerCount(1)
        .setColorAttachments(attachments)
        .setPDepthAttachment(&depth_attachment_info);

    g_color_image.setLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    g_normal_image.setLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    g_id_image.setLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    g_depth_image.setLayout(cmd, vk::ImageLayout::eDepthAttachmentOptimal,
                            vk::ImageAspectFlagBits::eDepth);

    cmd.beginRendering(rendering_info);

    set_all_render_state(cmd);

    // Draw world.
    shader_objects.bind_vertex(cmd, 0);
    shader_objects.bind_fragment(cmd, 1);

#ifdef DEBUG_VERTICES
    // TODO: Instead of a dedicated vector for debug verts, just offset into
    // bindless data.
    cmd.bindVertexBuffers(0, g_dbg_vertex_buffer.buffer(), {0});
    cmd.bindIndexBuffer(g_dbg_index_buffer.buffer(), 0, vk::IndexType::eUint32);
    cmd.drawIndexed(g_bindless_data.get_index_count(), 1, 0, 0, 0);
#else
    cmd.draw(g_bindless_data.get_vertex_count(), 1, 0, 0);
#endif

    cmd.endRendering();

    // Post processing.
    g_color_image.setLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
    g_normal_image.setLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
    g_id_image.setLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);

    cmd.setDepthTestEnable(vk::False);
    cmd.setDepthWriteEnable(vk::False);

    // Transition swapchain image layout to color write.
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

    vk::RenderingAttachmentInfoKHR swapchain_attachment_info;
    swapchain_attachment_info
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(g_swapchain_views[frame])
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    rendering_info.setColorAttachments(swapchain_attachment_info)
        .setPDepthAttachment(nullptr);

    cmd.beginRendering(rendering_info);

    shader_objects.bind_vertex(cmd, 2);
    shader_objects.bind_fragment(cmd, 3);

    cmd.draw(3, 1, 0, 0);

    cmd.endRendering();

    // Transition swapchain image layout to present.
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
    g_device.waitIdle();
    g_device.destroyCommandPool(g_command_pool);
    g_swapchain.destroy_image_views(g_swapchain_views);

    create_swapchain();
    create_command_pool();
    create_command_buffers();

    for (uint32_t i = 0; i < g_command_buffers.size(); ++i) {
        record_rendering(i);
    }
}