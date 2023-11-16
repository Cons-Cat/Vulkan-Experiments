#include <liblava/lava.hpp>  // IWYU pragma: keep

#include "pipelines.hpp"
#include "types.hpp"

void add_shader_to(lava::render_pipeline::ptr& pipeline,
                   std::filesystem::path shader_path,
                   VkShaderStageFlagBits stage);

void init_render_pass(lava::render_pass::ptr&, lava::image::ptr&,
                      lava::render_target::ptr&);

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

    lava::device_p device = frame.platform.create_device(0);
    if (!device) {
        /// return error::create_failed;
    }

    lava::render_target::ptr render_target = create_target(&window, device);
    if (!render_target) {
        // return error::create_failed;
    }

    lava::forward_shading color_shading;

    // The render pass already has color and depth attachments.
    lava::attachment::ptr id_attachment =
        lava::attachment::make(VK_FORMAT_R32_UINT);
    id_attachment->set_op(VK_ATTACHMENT_LOAD_OP_CLEAR,
                          VK_ATTACHMENT_STORE_OP_STORE);
    id_attachment->set_stencil_op(VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE);
    id_attachment->set_layouts(VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    color_shading.extra_color_attachments.push_back(id_attachment);

    lava::image::ptr image = lava::image::make(VK_FORMAT_R32_UINT);
    image->set_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    image->create(device, {game_width, game_height});
    color_shading.extra_image_views.push_back(image);

    if (!color_shading.create(render_target)) {
        // return error::create_failed;
    }

    lava::render_pass::ptr render_pass = color_shading.get_pass();

    lava::ui32 frame_count = render_target->get_frame_count();

    lava::block block;
    if (!block.create(device, frame_count, device->graphics_queue().family)) {
        // return error::create_failed;
    }

    lava::renderer renderer;
    if (!renderer.create(render_target->get_swapchain())) {
        // return error::create_failed;
    }

    // initialize camera
    glm::vec3 pos = {0, -4, 4};
    viewproj[0] = glm::lookAt(pos, {0, 0, 0}, {0, 0, 1});
    viewproj[1] =
        glm::perspective(glm::radians(45.f),
                         static_cast<float>(render_target->get_size().x) /
                             static_cast<float>(render_target->get_size().y),
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

    // Push data for outlines too.
    render_vertices.insert(std::end(render_vertices),
                           std::begin(mesh->get_vertices()),
                           std::end(mesh->get_vertices()));

    render_indices.insert(std::end(render_indices),
                          std::begin(mesh->get_indices()),
                          std::end(mesh->get_indices()));

    for (auto& v : mesh->get_vertices()) {
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

    render_pass->set_clear_color({0, 0, 0});

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
        device, descriptor_set, bindless_descriptor, render_pass);
    // push this render pass to the pipeline
    render_pass->add_front(color_pipeline.pipeline);

    block.add_command([&](VkCommandBuffer cmd_buf) {
        std::memcpy(bindless_buffer.get_mapped_data(), std::data(bindless_data),
                    std::size(bindless_data) * sizeof(gpu_entity));

        render_pass->process(cmd_buf, block.get_current_frame());
    });

    window.on_resize = [&](uint32_t new_width, uint32_t new_height) {
        return render_target->resize({new_width, new_height});
    };

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
        image->destroy();

        vertices_buffer->destroy();
        indices_buffer->destroy();

        bindless_descriptor->free(descriptor_set, descriptor_pool->get());
        descriptor_pool->destroy();
        bindless_descriptor->destroy();
        color_pipeline.destroy();

        block.destroy();
        color_shading.destroy();

        renderer.destroy();
        render_target->destroy();
    });

    return frame.run();
}
