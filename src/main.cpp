#include <vulkan/vulkan.hpp>

#include <VkBootstrap.h>
#include <cstddef>
#include <iostream>

#include "bindless.hpp"
#include "camera.hpp"
#include "defer.hpp"
#include "geometry.hpp"
#include "globals.hpp"
#include "light.hpp"
#include "shader_objects.hpp"
#include "vulkan_flow.hpp"
#include "window.hpp"

// Vulkan confuses the leak sanitizer.
extern "C" auto __asan_default_options() -> char const* {  // NOLINT
    return "detect_leaks=0";
}

auto main() -> int {
    vk::DynamicLoader vkloader;
    vulk.init();

    vkb::InstanceBuilder instance_builder;
    vkb::Result maybe_instance = instance_builder.request_validation_layers()
                                     .require_api_version(1, 3, 0)
                                     .use_default_debug_messenger()
                                     .build();
    if (!maybe_instance) {
        std::cout << maybe_instance.error().message() << '\n';
        std::quick_exit(1);
    }
    vkb::Instance instance = *maybe_instance;

    vulk.init(vk::Instance{instance.instance});

    my_window win;
    win.SetTitle("");
    win.SetWinSize(game_width, game_height);
    vk::SurfaceKHR surface =
        static_cast<VkSurfaceKHR>(win.GetSurface(instance));

    // Initialize global device.
    g_device = make_device(instance, surface);
    vulk.init(g_device);

    create_first_swapchain();
    defer {
        vkb::destroy_swapchain(g_swapchain);
    };

    create_command_pool();
    defer {
        g_device.destroyCommandPool(g_command_pool);
    };

    g_color_image = vku::ColorAttachmentImage(
        g_device, g_physical_device.memory_properties, game_width, game_height,
        vk::Format::eR32G32B32A32Sfloat);

    g_normal_image = vku::ColorAttachmentImage(
        g_device, g_physical_device.memory_properties, game_width, game_height,
        vk::Format::eR32G32B32A32Sfloat);

    g_xyz_image = vku::ColorAttachmentImage(
        g_device, g_physical_device.memory_properties, game_width, game_height,
        vk::Format::eR32G32B32A32Sfloat);

    g_id_image = vku::ColorAttachmentImage(
        g_device, g_physical_device.memory_properties, game_width, game_height,
        vk::Format::eR32Uint);

    g_depth_image =
        vku::DepthStencilImage(g_device, g_physical_device.memory_properties,
                               game_width, game_height, depth_format);

    vku::SamplerMaker sampler_maker;
    g_nearest_neighbor_sampler = sampler_maker.create(g_device);

    create_sync_objects();
    defer {
        for (auto& semaphore : g_finished_semaphore) {
            g_device.destroySemaphore(semaphore);
        }
        for (auto& semaphore : g_available_semaphores) {
            g_device.destroySemaphore(semaphore);
        }
        for (auto& fence : g_in_flight_fences) {
            g_device.destroyFence(fence);
        }
    };

    create_command_buffers();

    vku::DescriptorSetLayoutMaker dslm;
    g_descriptor_layout =
        dslm
            // Bindless world data.
            .buffer(0, vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eAllGraphics, 1)
            // Color/normal/xyz/ID/depth maps.
            .image(1, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 5)
            // Light maps.
            .image(2, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, g_lights.capacity())
            .createUnique(g_device)
            .release();

    vk::PipelineLayoutCreateInfo pipeline_info;
    pipeline_info.setSetLayouts(g_descriptor_layout)
        .setFlags(vk::PipelineLayoutCreateFlags());
    g_pipeline_layout = g_device.createPipelineLayout(pipeline_info);

    std::vector<vk::DescriptorPoolSize> pool_sizes;
    pool_sizes.emplace_back(vk::DescriptorType::eStorageBuffer, 1);
    pool_sizes.emplace_back(vk::DescriptorType::eCombinedImageSampler,
                            // Five compositing textures, plus light maps.
                            5 + g_lights.capacity());

    // Create an arbitrary number of descriptors in a pool.
    // Allow the descriptors to be freed, possibly not optimal behaviour.
    vk::DescriptorPoolCreateInfo descriptor_pool_info;
    descriptor_pool_info
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setPoolSizes(pool_sizes)
        .setMaxSets(1);

    vk::DescriptorPool descriptor_pool =
        g_device.createDescriptorPool(descriptor_pool_info);
    defer {
        g_device.destroyDescriptorPool(descriptor_pool);
    };

    // TODO: `g_buffer` is hard coded to 2 kibibytes, which might be a problem
    // later.
    g_buffer = vku::GenericBuffer(g_device, g_physical_device.memory_properties,
                                  vk::BufferUsageFlagBits::eStorageBuffer |
                                      vk::BufferUsageFlagBits::eTransferDst |
                                      vk::BufferUsageFlagBits::eVertexBuffer |
                                      vk::BufferUsageFlagBits::eIndexBuffer |
                                      vk::BufferUsageFlagBits::eIndirectBuffer,
                                  g_bindless_data.capacity());

    vku::DescriptorSetMaker dsm;
    dsm.layout(g_descriptor_layout);
    g_descriptor_set = dsm.create(g_device, descriptor_pool).front();

    // Push a light into the scene.
    glm::mat4x4 light_transform = glm::identity<glm::mat4x4>();
    glm::vec3 light_position = {3, -2, 3};
    light_transform = glm::rotate(light_transform, -45.f, {.5f, -.5f, 0});
    light_transform = glm::translate(light_transform, light_position);
    light_transform =
        glm::lookAt(glm::vec3{2.5f, 2.f, 2.f}, glm::vec3{0.f, 0.f, 0.f},
                    glm::vec3{0.f, 1.f, 0.f});

    g_lights.push_back({light_transform, projection_matrix, light_position});

    update_descriptors();

    // Compile and link shaders.
    shader_objects.add_compute_shader(getexepath().parent_path() /
                                      "../culling.spv");
    shader_objects.add_vertex_shader(getexepath().parent_path() /
                                     "../vertex_camera.spv");
    shader_objects.add_vertex_shader(getexepath().parent_path() /
                                     "../vertex_light.spv");
    shader_objects.add_fragment_shader(getexepath().parent_path() /
                                       "../fragment.spv");

    shader_objects.add_vertex_shader(getexepath().parent_path() /
                                     "../composite_vertex.spv");
    shader_objects.add_fragment_shader(getexepath().parent_path() /
                                       "../composite_fragment.spv");
    defer {
        shader_objects.destroy();
    };

    glm::mat4x4 proj = projection_matrix;
    proj[1][1] *= -1.f;
    g_bindless_data.set_proj_matrix(proj);

    g_camera.position.z = 2.f;

    static float rotation = 0.f;

    // Game loop.
    while (win.ProcessEvents()) {
        g_bindless_data.reset();
        g_bindless_data.set_proj_matrix(proj);

        // Update camera.
        glm::mat4x4 const view = g_camera.make_view_matrix();
        g_bindless_data.set_view_matrix(view);

        // Add cubes and planes to be rendered.
        g_bindless_data.push_mesh(g_cube_mesh);
        g_bindless_data.push_mesh(g_plane_mesh);
        g_bindless_data.push_indices();

        rotation += 0.05f;

        glm::mat4x4 a = glm::identity<glm::mat4x4>();
        a = glm::translate(a, {-0.5f, -0.5f, -0.5f});
        a = glm::rotate(a, -rotation, {1, 1, 1});

        glm::mat4x4 b = glm::identity<glm::mat4x4>();
        b = glm::rotate(b, rotation, {1, 1, 1});

        mesh_instance const cube_inst1 = {
            .position = {-1, 0, 0},
            .rotation = glm::toQuat(a),
            .index_count =
                static_cast<index_type>(g_cube_mesh.m_indices.size()),
        };

        mesh_instance const cube_inst2 = {
            .position = {1, 0.15f, 0.5f},
            .rotation = glm::toQuat(b),
            .index_count =
                static_cast<index_type>(g_cube_mesh.m_indices.size()),
        };

        mesh_instance const plane_inst = {
            .position = {0.f, -0.75f, 0.f},
            // clang-format off
            .scaling = {10.f, 10.f, 10.f},
            // clang-format on
            .index_count =
                static_cast<index_type>(g_plane_mesh.m_indices.size()),
        };

        g_bindless_data.push_instances_of(0, {cube_inst1, cube_inst2});
        g_bindless_data.push_instances_of(1, {plane_inst});

        // Finalize data to be transferred.
        g_bindless_data.push_properties();

        g_buffer.upload(g_device, g_physical_device.memory_properties,
                        g_command_pool, g_graphics_queue,
                        g_bindless_data.data(), g_bindless_data.capacity());

        std::int16_t width;
        std::int16_t height;
        win.GetWinSize(width, height);
        g_screen_width = static_cast<std::uint32_t>(width);
        g_screen_height = static_cast<std::uint32_t>(height);

        record();

        render_and_present();
    }

    g_device.waitIdle();
}
