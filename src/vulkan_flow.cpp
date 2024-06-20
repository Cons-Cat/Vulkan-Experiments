#include "vulkan_flow.hpp"

#include "bindless.hpp"
#include "globals.hpp"
#include "light.hpp"
#include "shader_objects.hpp"

auto make_device(vkb::Instance instance, vk::SurfaceKHR surface) -> vk::Device {
    vk::PhysicalDeviceFeatures vulkan_1_0_features;
    vulkan_1_0_features.setSampleRateShading(vk::True);
    vulkan_1_0_features.setShaderStorageImageMultisample(vk::True);

    vk::PhysicalDeviceVulkan12Features vulkan_1_2_features{};
    vulkan_1_2_features.setDrawIndirectCount(vk::True);
    vulkan_1_2_features.setBufferDeviceAddress(vk::True);
    vulkan_1_2_features.setRuntimeDescriptorArray(vk::True);

    vkb::PhysicalDeviceSelector physical_device_selector(instance);
    physical_device_selector.add_required_extension("VK_KHR_dynamic_rendering")
        .add_required_extension("VK_EXT_shader_object")
        .add_required_extension("VK_EXT_depth_clip_enable")
        .add_required_extension("VK_KHR_buffer_device_address")
        .set_required_features(vulkan_1_0_features)
        .set_required_features_12(vulkan_1_2_features);

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
    vk::PhysicalDeviceDepthClipEnableFeaturesEXT depth_clipping(
        vk::True, &dynamic_rendering_feature);
    vk::PhysicalDeviceShaderObjectFeaturesEXT shader_object_feature(
        vk::True, &depth_clipping);

    vkb::DeviceBuilder device_builder{g_physical_device};
    device_builder.add_pNext(&shader_object_feature);
    auto maybe_device = device_builder.build();
    if (!maybe_device) {
        std::cout << maybe_device.error().message() << '\n';
        std::quick_exit(1);
    }
    auto device = *maybe_device;

    // Initialize queues.
    g_graphics_queue = *device.get_queue(vkb::QueueType::graphics);
    g_graphics_queue_index = *device.get_queue_index(vkb::QueueType::graphics);

    g_present_queue = *device.get_queue(vkb::QueueType::present);
    g_present_queue_index = *device.get_queue_index(vkb::QueueType::present);

    g_swapchain_builder = vkb::SwapchainBuilder{device};

    return device.device;
}

void create_first_swapchain() {
    assert(g_swapchain_builder);

    // Initialize swapchain.
    auto maybe_swapchain =
        g_swapchain_builder->set_old_swapchain(g_swapchain).build();
    if (!maybe_swapchain) {
        std::cout << maybe_swapchain.error().message() << ' '
                  << maybe_swapchain.vk_result() << '\n';
        std::quick_exit(1);
    }

    g_swapchain = *maybe_swapchain;
    g_swapchain_images = *g_swapchain.get_images();
    g_swapchain_views = *g_swapchain.get_image_views();
}

void create_command_pool() {
    vk::CommandPoolCreateInfo pool_info = {};
    pool_info.setQueueFamilyIndex(g_graphics_queue_index)
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

    g_command_pool = g_device.createCommandPool(pool_info);
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
    g_image_in_flight.resize(g_swapchain.image_count, nullptr);

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

void update_descriptors() {
    vku::DescriptorSetUpdater dsu_camera;
    dsu_camera.beginDescriptorSet(g_descriptor_set);

    dsu_camera.beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer)
        .buffer(g_buffer.buffer(), 0, vk::WholeSize)

        .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
        // Color map.
        .image(g_nearest_neighbor_sampler, g_color_image.imageView(),
               vk::ImageLayout::eShaderReadOnlyOptimal)
        // Normal map.
        .image(g_nearest_neighbor_sampler, g_normal_image.imageView(),
               vk::ImageLayout::eShaderReadOnlyOptimal)
        // XYZ map.
        .image(g_nearest_neighbor_sampler, g_xyz_image.imageView(),
               vk::ImageLayout::eShaderReadOnlyOptimal)
        // Instance ID map.
        .image(g_nearest_neighbor_sampler, g_id_image.imageView(),
               vk::ImageLayout::eShaderReadOnlyOptimal)
        // Rasterization depth map.
        .image(g_nearest_neighbor_sampler, g_depth_image.imageView(),
               vk::ImageLayout::eDepthReadOnlyOptimal);

    if (!g_lights.light_maps.empty()) {
        // Light depth textures.
        dsu_camera.beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler);
        // Add every light-source to the light map bindings.
        for (auto&& image : g_lights.light_maps) {
            dsu_camera.image(g_nearest_neighbor_sampler, image.imageView(),
                             vk::ImageLayout::eDepthReadOnlyOptimal);
        }
    }

    dsu_camera.update(g_device);
    assert(dsu_camera.ok());
}

constinit unsigned frame = 0;

auto render_and_present() -> bool {
    constexpr auto timeout = std::numeric_limits<uint64_t>::max();

    // Wait for host to signal the fence for this swapchain frame.
    vulk.vkWaitForFences(g_device, 1, &g_in_flight_fences[frame], vk::True,
                         timeout);

    // Get a swapchain index that is currently presentable.
    unsigned image_index;
    auto error = static_cast<vk::Result>(vulk.vkAcquireNextImageKHR(
        g_device, g_swapchain.swapchain, timeout, g_available_semaphores[frame],
        nullptr, &image_index));

    if (error == vk::Result::eErrorOutOfDateKHR) {
        recreate_swapchain();
        return false;
    }

    if (g_image_in_flight[image_index] != nullptr) {
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

    g_device.resetFences({g_in_flight_fences[frame]});
    g_graphics_queue.submit(submit_info, g_in_flight_fences[frame]);

    // After rendering to the swapchain frame completes, present it to the
    // surface.
    std::array present_indices = {image_index};
    std::array<vk::SwapchainKHR, 1> swapchains = {g_swapchain.swapchain};
    vk::PresentInfoKHR present_info;
    present_info.setImageIndices(present_indices)
        .setWaitSemaphores(signal_semaphores)
        .setSwapchains(swapchains);

    error = g_graphics_queue.presentKHR(present_info);
    if (error == vk::Result::eErrorOutOfDateKHR) {
        recreate_swapchain();

        // create_command_pool();
        // create_command_buffers();
        // record();

        // TODO: Re-render buffer or blit to presentation surface again.
        return false;
    }

    frame = (frame + 1) % max_frames_in_flight;
    return true;
}

constexpr float depth_bias_constant = 0.0f;
constexpr float depth_bias_slope = 0.25f;

// TODO: Pre-record these once, before rendering frames.
void set_all_render_state(vk::CommandBuffer cmd) {
    cmd.setLineWidth(1.0);
    cmd.setPolygonModeEXT(vk::PolygonMode::eFill);
    vk::ColorBlendEquationEXT color_blend_equations[3]{};
    cmd.setColorBlendEquationEXT(3, color_blend_equations);
    cmd.setRasterizerDiscardEnable(vk::False);
    cmd.setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);

    vk::SampleMask sample_mask = 0x1;
    cmd.setSampleMaskEXT(vk::SampleCountFlagBits::e1, sample_mask);
    cmd.setAlphaToCoverageEnableEXT(vk::True);

    cmd.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
    cmd.setPrimitiveRestartEnable(vk::False);

    // Per-vertex bindings and attributes:
    vk::VertexInputBindingDescription2EXT per_vertex_binding{};
    per_vertex_binding.setBinding(0)
        .setInputRate(vk::VertexInputRate::eVertex)
        .setStride(sizeof(vertex))
        .setDivisor(1);

    vk::VertexInputAttributeDescription2EXT per_vertex_position_attribute{};
    per_vertex_position_attribute.setBinding(0)
        .setLocation(0)
        .setOffset(0)
        .setFormat(vk::Format::eR32G32B32A32Sfloat);  // `glm::vec4`

    vk::VertexInputAttributeDescription2EXT per_vertex_normal_attribute{};
    per_vertex_normal_attribute.setBinding(0)
        .setLocation(1)
        .setOffset(offsetof(vertex, normal))
        .setFormat(vk::Format::eR32G32B32A32Sfloat);  // `glm::vec3`

    // Per-instance bindings and attributes:
    vk::VertexInputBindingDescription2EXT per_instance_binding{};
    per_instance_binding.setBinding(1)
        .setInputRate(vk::VertexInputRate::eInstance)
        .setStride(sizeof(buffer_storage::property))
        .setDivisor(1);

    vk::VertexInputAttributeDescription2EXT per_instance_position_attribute{};
    per_instance_position_attribute.setBinding(1)
        .setLocation(2)
        .setOffset(offsetof(buffer_storage::property, position))
        .setFormat(vk::Format::eR32G32B32Sfloat);  // `glm::vec3`

    vk::VertexInputAttributeDescription2EXT per_instance_rotation_attribute{};
    per_instance_rotation_attribute.setBinding(1)
        .setLocation(3)
        .setOffset(offsetof(buffer_storage::property, rotation))
        .setFormat(vk::Format::eR32G32B32A32Sfloat);  // `glm::fquat`

    vk::VertexInputAttributeDescription2EXT per_instance_scaling_attribute{};
    per_instance_scaling_attribute.setBinding(1)
        .setLocation(4)
        .setOffset(offsetof(buffer_storage::property, scaling))
        .setFormat(vk::Format::eR32G32B32Sfloat);  // `glm::vec3`

    vk::VertexInputAttributeDescription2EXT per_instance_id_attribute{};
    per_instance_id_attribute.setBinding(1)
        .setLocation(5)
        .setOffset(offsetof(buffer_storage::property, id))
        .setFormat(vk::Format::eR32Uint);  // `unsigned`

    cmd.setVertexInputEXT(
        {per_vertex_binding, per_instance_binding},
        {per_vertex_position_attribute, per_vertex_normal_attribute,
         per_instance_position_attribute, per_instance_rotation_attribute,
         per_instance_scaling_attribute, per_instance_id_attribute});

    cmd.setDepthClampEnableEXT(vk::False);
    cmd.setDepthClipEnableEXT(vk::False);

    cmd.setDepthBiasEnable(vk::True);
    cmd.setDepthBias(depth_bias_constant, 0, depth_bias_slope);
    cmd.setDepthTestEnable(vk::True);
    cmd.setDepthWriteEnable(vk::True);
    cmd.setDepthBoundsTestEnable(vk::False);

    cmd.setFrontFace(vk::FrontFace::eCounterClockwise);
    cmd.setDepthCompareOp(vk::CompareOp::eLess);

    cmd.setStencilTestEnable(vk::False);

    cmd.setLogicOpEnableEXT(vk::False);

    cmd.setColorBlendEnableEXT(0, vk::False);
    cmd.setColorBlendEnableEXT(1, vk::False);
    cmd.setColorBlendEnableEXT(2, vk::False);
    cmd.setColorBlendEnableEXT(3, vk::False);
    cmd.setColorBlendEnableEXT(4, vk::False);

    constexpr vk::Flags color_write_mask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    cmd.setColorWriteMaskEXT(0, 1, &color_write_mask);
    cmd.setColorWriteMaskEXT(1, 1, &color_write_mask);
    cmd.setColorWriteMaskEXT(2, 1, &color_write_mask);

    constexpr vk::Flags id_write_mask = vk::ColorComponentFlagBits::eR;
    cmd.setColorWriteMaskEXT(3, 1, &id_write_mask);

    constexpr vk::Flags depth_write_mask = vk::ColorComponentFlagBits::eR;
    cmd.setColorWriteMaskEXT(4, 1, &depth_write_mask);

    // Bind color attachments to descriptors so they can be read after being
    // written.
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, g_pipeline_layout,
                           0, g_descriptor_set, {});
}

constexpr vk::ClearColorValue clear_color = {1.f, 0.f, 1.f, 0.f};
constexpr vk::ClearColorValue black_clear_color = {0, 0, 0, 1};
constexpr vk::ClearColorValue depth_clear_color = {1.f, 1.f, 1.f, 1.f};

void draw_meshes(vk::CommandBuffer& cmd) {
    // TODO: Use the sized buffers so that debuggers have more info once
    // RenderDoc supports this feature.
    // cmd.bindVertexBuffers2(0, g_buffer.buffer(),
    //                        g_bindless_data.vertices_offset,
    //                        g_bindless_data.get_vertex_count(),
    //                        sizeof(vertex));

    // cmd.bindIndexBuffer2KHR(
    //     g_buffer.buffer(), g_bindless_data.get_index_offset(),
    //     g_bindless_data.get_index_offset() +
    //     g_bindless_data.get_index_count(), vk::IndexType::eUint32);

    cmd.bindVertexBuffers(0, {g_buffer.buffer(), g_buffer.buffer()},
                          {g_bindless_data.vertices_offset,
                           g_bindless_data.get_properties_offset()});

    cmd.bindIndexBuffer(g_buffer.buffer(), g_bindless_data.get_index_offset(),
                        vk::IndexType::eUint32);

    // 16 is the byte offset of the instance count into the bindless buffer.
    cmd.drawIndexedIndirectCount(
        g_buffer.buffer(), g_bindless_data.get_instance_commands_offset(),
        g_buffer.buffer(), 16, g_bindless_data.get_instance_commands_count(),
        sizeof(vk::DrawIndexedIndirectCommand));
}

void record_rendering(vk::CommandBuffer& cmd) {
    vk::Viewport viewport;
    viewport.setWidth(game_width)
        .setHeight(game_height)
        .setX(0)
        .setY(0)
        .setMinDepth(0.f)
        .setMaxDepth(1.f);
    vk::Rect2D scissor;
    scissor.setOffset({0, 0}).setExtent({game_width, game_height});
    vk::Rect2D render_area;
    render_area.setOffset({0, 0}).setExtent({game_width, game_height});

    cmd.setCullMode(vk::CullModeFlagBits::eBack);
    cmd.setViewportWithCount(1, &viewport);
    cmd.setScissorWithCount(1, &scissor);

    vk::RenderingAttachmentInfoKHR color_attachment_info;
    color_attachment_info.setClearValue(clear_color)
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(g_color_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfoKHR normal_attachment_info;
    normal_attachment_info
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(g_normal_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfoKHR xyz_attachment_info;
    xyz_attachment_info.setClearValue(black_clear_color)
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(g_xyz_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfoKHR id_attachment_info;
    id_attachment_info.setClearValue(black_clear_color)
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(g_id_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setResolveMode(vk::ResolveModeFlagBits::eNone);

    std::array const attachments = {color_attachment_info,
                                    normal_attachment_info, xyz_attachment_info,
                                    id_attachment_info};

    vk::RenderingAttachmentInfoKHR depth_attachment_info;
    depth_attachment_info.setClearValue(depth_clear_color)
        .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
        .setImageView(g_depth_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(render_area)
        .setLayerCount(1)
        .setColorAttachments(attachments)
        .setPDepthAttachment(&depth_attachment_info);

    g_color_image.setLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    g_normal_image.setLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    g_xyz_image.setLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    g_id_image.setLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    g_depth_image.setLayout(cmd, vk::ImageLayout::eDepthAttachmentOptimal,
                            vk::ImageAspectFlagBits::eDepth);

    // Modify the bindless buffer with this culling shader.
    shader_objects.bind_compute(cmd, 0);
    cmd.dispatch(4, 0, 0);

    cmd.beginRendering(rendering_info);

    set_all_render_state(cmd);

    // Rasterizing color, normals, IDs, and depth for the world in view.
    shader_objects.bind_vertex(cmd, 1);
    shader_objects.bind_fragment(cmd, 3);

    // 16 is the byte offset of the instance count into the bindless buffer.
    draw_meshes(cmd);

    cmd.endRendering();
}

void record_lights(vk::CommandBuffer& cmd) {
    vk::Viewport viewport;
    viewport.setWidth(game_width)
        .setHeight(game_height)
        .setX(0)
        .setY(0)
        .setMinDepth(0.f)
        .setMaxDepth(1.f);
    vk::Rect2D scissor;
    scissor.setOffset({0, 0}).setExtent({game_width, game_height});
    vk::Rect2D render_area;
    render_area.setOffset({0, 0}).setExtent({game_width, game_height});

    cmd.setCullMode(vk::CullModeFlagBits::eFront);
    cmd.setViewportWithCount(1, &viewport);
    cmd.setScissorWithCount(1, &scissor);

    // `current_light_idx` should be 32-bit, as `current_light_invocation`
    // is in the shader.
    for (unsigned current_light_idx = 0; current_light_idx < g_lights.size();
         ++current_light_idx) {
        auto& image = g_lights.light_maps[current_light_idx];

        vk::RenderingAttachmentInfoKHR depth_attachment_info;
        depth_attachment_info.setClearValue(depth_clear_color)
            .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
            .setImageView(image.imageView())
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore);

        vk::RenderingInfo rendering_info;
        rendering_info.setRenderArea(render_area)
            .setLayerCount(1)
            .setPDepthAttachment(&depth_attachment_info);

        image.setLayout(cmd, vk::ImageLayout::eDepthAttachmentOptimal,
                        vk::ImageAspectFlagBits::eDepth);

        cmd.beginRendering(rendering_info);

        set_all_render_state(cmd);

        cmd.pushConstants(g_pipeline_layout, vk::ShaderStageFlagBits::eVertex,
                          0, sizeof(current_light_idx), &current_light_idx);

        // Rasterizing depth for the world in view.
        shader_objects.bind_vertex(cmd, 2);
        shader_objects.bind_fragment(cmd, 3);

        draw_meshes(cmd);

        cmd.endRendering();
    }
}

void record_compositing(vk::CommandBuffer& cmd, std::size_t frame) {
    // Post processing.
    g_color_image.setLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
    g_normal_image.setLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
    g_xyz_image.setLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
    g_id_image.setLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
    g_depth_image.setLayout(cmd, vk::ImageLayout::eDepthReadOnlyOptimal,
                            vk::ImageAspectFlagBits::eDepth);

    for (auto&& image : g_lights.light_maps) {
        image.setLayout(cmd, vk::ImageLayout::eDepthReadOnlyOptimal,
                        vk::ImageAspectFlagBits::eDepth);
    }

    // The hard-coded compositing triangle does not require depth-testing.
    cmd.setCullMode(vk::CullModeFlagBits::eBack);
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

    vk::Rect2D render_area;
    render_area.setOffset({0, 0}).setExtent(g_swapchain.extent);

    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(render_area)
        .setLayerCount(1)
        .setColorAttachments(swapchain_attachment_info)
        .setPDepthAttachment(nullptr);

    cmd.beginRendering(rendering_info);

    shader_objects.bind_vertex(cmd, 4);
    shader_objects.bind_fragment(cmd, 5);

    // Draw a hard-coded triangle.
    cmd.setVertexInputEXT({}, {});
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
}

void record() {
    for (std::size_t i = 0; i < max_frames_in_flight; ++i) {
        vk::CommandBuffer& cmd = g_command_buffers[i];
        vk::CommandBufferBeginInfo begin_info;
        cmd.begin(begin_info);

        record_rendering(cmd);
        record_lights(cmd);
        record_compositing(cmd, i);

        cmd.end();
    }
}

void recreate_swapchain() {
    g_device.waitIdle();
    g_device.destroyCommandPool(g_command_pool);
    g_swapchain.destroy_image_views(g_swapchain_views);

    auto maybe_swapchain =
        g_swapchain_builder->set_old_swapchain(g_swapchain).build();
    if (!maybe_swapchain) {
        std::cout << maybe_swapchain.error().message() << '\n';
        std::quick_exit(1);
    }

    // Free the previously allocated swapchain.
    vkb::destroy_swapchain(g_swapchain);

    // Replace it with the new swapchain.
    g_swapchain = maybe_swapchain.value();
    g_swapchain_images = *g_swapchain.get_images();
    g_swapchain_views = *g_swapchain.get_image_views();
}
