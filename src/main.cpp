#include <vulkan/vulkan.hpp>

#include <VkBootstrap.h>
#include <cstddef>
#include <iostream>
#include <ktxvulkan.h>

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

inline void load_skybox() {
    ktxTexture2* p_ktx_skybox_data;
    ktx_error_code_e result;
    ktxVulkanDeviceInfo vdi;

    ktxVulkanDeviceInfo_Construct(&vdi, g_physical_device, g_device,
                                  g_graphics_queue, g_command_pool, nullptr);

    result = ktxTexture2_CreateFromNamedFile(
        (getexepath().parent_path() / "skybox.ktx2").c_str(),
        KTX_TEXTURE_CREATE_NO_FLAGS, &p_ktx_skybox_data);
    assert(result == KTX_SUCCESS);

    result = ktxTexture2_VkUploadEx(
        p_ktx_skybox_data, &vdi, &g_ktx_skybox, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    assert(result == KTX_SUCCESS);

    ktxTexture2_Destroy(p_ktx_skybox_data);
    ktxVulkanDeviceInfo_Destruct(&vdi);

    vk::ImageViewCreateInfo skybox_view_info;
    // Set the non-default values.
    skybox_view_info.image = g_ktx_skybox.image;
    skybox_view_info.format = static_cast<vk::Format>(g_ktx_skybox.imageFormat);
    skybox_view_info.viewType =
        static_cast<vk::ImageViewType>(g_ktx_skybox.viewType);
    skybox_view_info.subresourceRange.aspectMask =
        vk::ImageAspectFlagBits::eColor;
    skybox_view_info.subresourceRange.layerCount = g_ktx_skybox.layerCount;
    skybox_view_info.subresourceRange.levelCount = g_ktx_skybox.levelCount;
    // vk::ImageView skybox_view(view_info);
    g_skybox_view = g_device.createImageView(skybox_view_info);

    // image_view = vkctx.device.createImageView(viewInfo);
    // g_skybox = vku::TextureImageCube();
    // g_physical_device.memory_properties, 0, 0);
    // g_skybox.update()
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

    my_window window;
    window.SetTitle("");
    window.SetWinSize(game_width, game_height);
    vk::SurfaceKHR surface =
        static_cast<VkSurfaceKHR>(window.GetSurface(instance));

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

    create_command_buffers();

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
        for (auto&& semaphore : g_finished_semaphore) {
            g_device.destroySemaphore(semaphore);
        }
        for (auto&& semaphore : g_available_semaphores) {
            g_device.destroySemaphore(semaphore);
        }
        for (auto&& fence : g_in_flight_fences) {
            g_device.destroyFence(fence);
        }
    };

    vku::DescriptorSetLayoutMaker dslm;
    g_descriptor_layout =
        dslm
            // Bindless world data.
            .buffer(0, vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eAllGraphics, 1)
            // Color/normal/xyz/ID/depth maps.
            // TODO: Put mesh textures here.
            .image(1, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 5)
            // Light maps.
            .image(2, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, g_lights.capacity())
            // Skybox texture map.
            .image(3, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 1)
            .createUnique(g_device)
            .release();

    vk::PipelineLayoutCreateInfo pipeline_info;
    pipeline_info.setSetLayouts(g_descriptor_layout)
        .setFlags(vk::PipelineLayoutCreateFlags())
        .setPushConstantRanges(g_push_constants);
    g_pipeline_layout = g_device.createPipelineLayout(pipeline_info);

    std::vector<vk::DescriptorPoolSize> pool_sizes;
    pool_sizes.emplace_back(vk::DescriptorType::eStorageBuffer, 1);
    pool_sizes.emplace_back(vk::DescriptorType::eCombinedImageSampler,
                            // 5 compositing textures, plus light maps, plus
                            // 1 skybox texture.
                            5 + g_lights.capacity() + 1);

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
    g_device_local_buffer =
        vku::GenericBuffer(g_device, g_physical_device.memory_properties,
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
    glm::mat4x4 light1_transform = glm::identity<glm::mat4x4>();
    glm::mat4x4 light2_transform = glm::identity<glm::mat4x4>();

    glm::vec3 light1_position = {3, 2, 3};
    light1_transform =
        glm::lookAt(light1_position, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});

    glm::vec3 light2_position = {-3, 2, 3};
    light2_transform =
        glm::lookAt(light2_position, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});

    g_lights.push_back({.transform = light1_transform,
                        .projection = projection_matrix,
                        .position = light1_position});
    g_lights.push_back({.transform = light2_transform,
                        .projection = projection_matrix,
                        .position = light2_position});

    load_skybox();
    defer {
        g_device.destroy(g_skybox_view);
        ktxVulkanTexture_Destruct(&g_ktx_skybox, g_device, nullptr);
    };

    // Initialize the descriptors.
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

    shader_objects.add_vertex_shader(getexepath().parent_path() /
                                     "../skybox_vertex.spv");
    shader_objects.add_fragment_shader(getexepath().parent_path() /
                                       "../skybox_fragment.spv");

    defer {
        shader_objects.destroy();
    };

    glm::mat4x4 proj = projection_matrix;
    proj[1][1] *= -1.f;  // Invert Y.
    g_bindless_data.set_proj_matrix(proj);

    g_camera.position.z = 2.f;

    static float rotation = 0.f;

    // Game loop.
    while (window.ProcessEvents()) {
        g_next_instance_id = 0;
        g_bindless_data.reset();
        g_bindless_data.set_proj_matrix(proj);

        // Update camera.
        glm::mat4x4 const view = g_camera.make_view_matrix();
        g_bindless_data.set_view_matrix(view);
        g_bindless_data.set_camera_position(g_camera.position);

        // Add cubes and planes to be rendered.

        // TODO: It is necessary for rendering skybox that the cube mesh is the
        // 0-index mesh. This should be moved into a special constant region of
        // the buffer.
        g_bindless_data.push_mesh(g_cube_mesh);
        g_bindless_data.push_mesh(g_plane_mesh);
        g_bindless_data.push_indices();

        rotation += 0.05f;

        glm::mat4x4 a = glm::identity<glm::mat4x4>();
        a = glm::translate(a, {-0.5f, 0.5f, -0.5f});

        // The braced initializers get formatted incorrectly here.
        // clang-format off
        mesh_instance const cube_inst1 = {
            .position = {-1, 0, 0},
            .rotation = glm::toQuat(glm::rotate(a, -rotation, {1, 1, 1})),
            .index_count =
                static_cast<index_type>(g_cube_mesh.m_indices.size()),
        };

        mesh_instance const cube_inst2 = {
            .position = {1, 0.15f, 0.5f},
            .rotation = glm::toQuat(glm::rotate(a, rotation, {1, 1, 1})),
            .color_blend = {-1, -1, -1, 1},
            .index_count =
                static_cast<index_type>(g_cube_mesh.m_indices.size()),
        };
        // clang-format on

        mesh_instance const grid_inst_even = {
            .color_blend = {1, 2, 1, 1},
            .index_count =
                static_cast<index_type>(g_plane_mesh.m_indices.size()),
        };
        mesh_instance const grid_inst_odd = {
            .color_blend = {-0.9, -0.9, -0.9, 1},
            .index_count =
                static_cast<index_type>(g_plane_mesh.m_indices.size()),
        };

        std::vector plane_instances =
            make_checkerboard_plane({0, -0.8f, -0.5f}, 1.25f, 0.75f, 5, 5,
                                    grid_inst_even, grid_inst_odd);

        g_bindless_data.push_instances_of(0, {cube_inst1, cube_inst2});
        g_bindless_data.push_instances_of(1, std::move(plane_instances));

        // Finalize data to be transferred.
        g_bindless_data.push_properties();

        // TODO: Make this part of the frame buffer recording.
        g_device_local_buffer.upload(
            g_device, g_physical_device.memory_properties, g_command_pool,
            g_graphics_queue, g_bindless_data.data(),
            g_bindless_data.capacity());

        short width;
        short height;
        window.GetWinSize(width, height);
        g_screen_width = static_cast<unsigned>(width);
        g_screen_height = static_cast<unsigned>(height);

        for (unsigned i = 0; i < max_frames_in_flight; ++i) {
            // Attempt to render each frame in a loop.
            record_frame(i);
            try {
                render_and_present(i);
            } catch (vk::OutOfDateKHRError const&) {
                recreate_swapchain();

                // TODO: Reset the command buffer rather than reallocating
                // so much.
                create_command_pool();
                create_command_buffers();
                // TODO: Only rerender the compositing layer, or simply blit the
                // render to the new window's surface.
                record_frame(i);
            }
        }
    }

    g_device.waitIdle();
}
