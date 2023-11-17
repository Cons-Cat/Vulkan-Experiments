#include <liblava/lava.hpp>  // IWYU pragma: keep

#include "pipelines.hpp"
#include "types.hpp"

void add_shader_to(lava::render_pipeline::ptr& pipeline,
                   std::filesystem::path shader_path,
                   VkShaderStageFlagBits stage);

lava::render_pass::ptr init_color_pass();
lava::render_pass::ptr init_composite_pass(lava::render_target::ptr&);

// Reserve two screen-sized textures.
inline std::vector<std::byte> textures_data(game_width* game_height * 2);

auto main(int argc, char* argv[]) -> int {
    lava::frame frame({argc, argv});
    if (!frame.ready()) {
        // return error::not_ready;
    }

    lava::window window("game");
    if (!window.create()) {
        // return error::create_failed;
    }
    window.set_resizable(false);
    window.set_size(game_width, game_height);

    lava::input input;
    window.assign(&input);

    constexpr float speed = 0.1;
    input.key.listeners.add([&](lava::key_event::ref event) -> bool {
        if (event.pressed(lava::key::left)) {
            bindless_data[1].world_position.x -= speed;
        }
        if (event.pressed(lava::key::right)) {
            bindless_data[1].world_position.x += speed;
        }
        if (event.pressed(lava::key::down)) {
            bindless_data[1].world_position.z += speed;
        }
        if (event.pressed(lava::key::up)) {
            bindless_data[1].world_position.z -= speed;
        }

        if (event.pressed(lava::key::escape)) {
            return frame.shut_down();
        }

        return lava::input_done;
    });

    frame.platform.on_create_param = [&](lava::device::create_param& param) {
        param.features.independentBlend = VK_TRUE;
    };

    device = frame.platform.create_device(0);
    if (!device) {
        /// return error::create_failed;
    }

    lava::render_target::ptr final_render_target =
        create_target(&window, device);

    lava::image::ptr color_image =
        lava::image::make(VK_FORMAT_R32G32B32A32_SFLOAT);
    color_image->set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    color_image->create(device, {game_width, game_height});

    auto depth_format =
        lava::find_supported_depth_format(device->get_vk_physical_device());
    lava::image::ptr depth_image = lava::image::make(depth_format.value());
    depth_image->set_usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    depth_image->set_layout(VK_IMAGE_LAYOUT_UNDEFINED);
    depth_image->set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT |
                                 VK_IMAGE_ASPECT_STENCIL_BIT);
    depth_image->set_component();
    depth_image->create(device, {game_width, game_height});

    lava::image::ptr entity_id_image = lava::image::make(VK_FORMAT_R32_UINT);
    entity_id_image->set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    entity_id_image->create(device, {game_width, game_height});

    lava::render_pass::ptr color_render_pass = init_color_pass();
    lava::render_pass::ptr composite_render_pass =
        init_composite_pass(final_render_target);

    lava::ui32 frame_count = final_render_target->get_frame_count();
    lava::block block;
    if (!block.create(device, frame_count, device->graphics_queue().family)) {
        // return error::create_failed;
    }

    final_render_target->on_create_attachments = [&] -> lava::VkAttachments {
        lava::VkAttachments result;

        if (!depth_image->create(device, final_render_target->get_size())) {
            return {};
        }

        for (auto& backbuffer : final_render_target->get_backbuffers()) {
            lava::VkImageViews attachments;

            attachments.push_back(backbuffer->get_view());
            attachments.push_back(depth_image->get_view());
            attachments.push_back(entity_id_image->get_view());

            result.push_back(attachments);
        }

        return result;
    };

    final_render_target->add_callback(
        &color_render_pass->get_target_callback());
    final_render_target->add_callback(
        &composite_render_pass->get_target_callback());

    lava::renderer renderer;
    if (!renderer.create(final_render_target->get_swapchain())) {
        // return error::create_failed;
    }

    // initialize camera
    glm::vec3 pos = {0, -4, 4};
    viewproj[0] = glm::lookAt(pos, {0, 0, 0}, {0, 0, 1});
    viewproj[1] = glm::perspective(
        glm::radians(45.f),
        static_cast<float>(final_render_target->get_size().x) /
            static_cast<float>(final_render_target->get_size().y),
        0.1f, 10.0f);

    // all shapes will share the same rotation value
    lava::buffer bindless_buffer;
    bindless_buffer.create_mapped(device, std::data(bindless_data),
                                  std::size(bindless_data) * sizeof(gpu_entity),
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // Put a cube into the `render_vertices`.
    lava::mesh_template<vertex>::ptr mesh =
        create_mesh<vertex>(device, lava::mesh_type::cube);
    render_vertices.clear();
    render_vertices.insert(std::begin(render_vertices),
                           std::begin(mesh->get_vertices()),
                           std::end(mesh->get_vertices()));

    render_indices.clear();
    render_indices.insert(std::begin(render_indices),
                          std::begin(mesh->get_indices()),
                          std::end(mesh->get_indices()));

    for (auto& v : render_vertices) {
        v.id = 1;
    }

    // Initialize vertex/index buffers.
    vertices_buffer = lava::buffer::make();
    vertices_buffer->create_mapped(
        device, render_vertices.data(),
        render_vertices.capacity() * sizeof(lava::vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    indices_buffer = lava::buffer::make();
    indices_buffer->create_mapped(
        device, render_indices.data(), render_indices.capacity(),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    lava::descriptor::ptr bindless_descriptor;
    lava::descriptor::pool::ptr descriptor_pool;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    // descriptor sets must be made to transfer the shapes' world matrix
    // and the camera's view matrix to the physical device
    bindless_descriptor = lava::descriptor::make();
    bindless_descriptor->add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     VK_SHADER_STAGE_VERTEX_BIT);
    if (!bindless_descriptor->create(device)) {
        //        return false;
    }

    descriptor_pool = lava::descriptor::pool::make();
    if (!descriptor_pool->create(device,
                                 {
                                     {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
    })) {
        //        return false;
    }

    descriptor_set = bindless_descriptor->allocate(descriptor_pool->get());

    VkWriteDescriptorSet const write_bindless_descriptor_set{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = bindless_buffer.get_descriptor_info(),
    };

    device->vkUpdateDescriptorSets({
        write_bindless_descriptor_set,
    });

    mypipeline color_pipeline = make_color_pipeline(
        descriptor_set, bindless_descriptor, color_render_pass);
    mypipeline composite_pipeline =
        make_composite_pipeline(composite_render_pass);
    //
    // push this render color_render_pass to the pipeline
    color_render_pass->add_front(color_pipeline.pipeline, 0);
    composite_render_pass->add_front(composite_pipeline.pipeline, 0);

    block.add_command([&](VkCommandBuffer cmd_buf) {
        std::memcpy(bindless_buffer.get_mapped_data(), std::data(bindless_data),
                    std::size(bindless_data) * sizeof(gpu_entity));

        color_render_pass->process(cmd_buf, block.get_current_frame());
    });

    frame.add_run([&](lava::id::ref run_id) {
        input.handle_events();

        if (window.close_request()) {
            return frame.shut_down();
        }

        if (window.resize_request()) {
            return window.handle_resize();
        }

        if (window.iconified()) {
            frame.set_wait_for_events(true);
            return lava::run_continue;
        } else {
            if (frame.waiting_for_events()) {
                frame.set_wait_for_events(false);
            }
        }

        lava::optional_index current_frame = renderer.begin_frame();
        if (!current_frame.has_value()) {
            return lava::run_continue;
        }

        if (!block.process(*current_frame)) {
            return lava::run_abort;
        }

        return renderer.end_frame(block.collect_buffers());
    });

    frame.add_run_end([&]() {
        entity_id_image->destroy();

        vertices_buffer->destroy();
        indices_buffer->destroy();

        bindless_descriptor->free(descriptor_set, descriptor_pool->get());
        descriptor_pool->destroy();
        bindless_descriptor->destroy();
        color_pipeline.destroy();

        block.destroy();
        // color_shading.destroy();

        color_render_pass->destroy();
        renderer.destroy();
        final_render_target->destroy();
    });

    return frame.run();
}
