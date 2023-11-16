#include <liblava/lava.hpp>  // IWYU pragma: keep

#include "pipelines.hpp"
#include "types.hpp"

void add_shader_to(lava::render_pipeline::ptr& pipeline,
                   std::filesystem::path shader_path,
                   VkShaderStageFlagBits stage);

void init_render_pass(lava::render_pass::ptr&, lava::image::ptr&,
                      lava::render_target::ptr&);

using namespace lava;

// Reserve two screen-sized textures.
inline std::vector<std::byte> textures_data(game_width* game_height * 2);

auto main(int argc, char* argv[]) -> int {
    frame frame({argc, argv});
    if (!frame.ready()) {
        return error::not_ready;
    }

    lava::window window("game");
    if (!window.create()) {
        return error::create_failed;
    }
    window.set_resizable(false);
    window.set_size(game_width, game_height);

    input input;
    window.assign(&input);

    constexpr float speed = 0.1;
    input.key.listeners.add([&](key_event::ref event) -> bool {
        if (event.pressed(key::left)) {
            bindless_data[0].world_position.x -= speed;
        }
        if (event.pressed(key::right)) {
            bindless_data[0].world_position.x += speed;
        }
        if (event.pressed(key::down)) {
            bindless_data[0].world_position.z += speed;
        }
        if (event.pressed(key::up)) {
            bindless_data[0].world_position.z -= speed;
        }

        if (event.pressed(key::escape)) {
            return frame.shut_down();
        }

        return input_done;
    });

    device_p device = frame.platform.create_device(0);
    if (!device) {
        return error::create_failed;
    }

    render_target::ptr render_target = create_target(&window, device);
    if (!render_target) {
        return error::create_failed;
    }

    forward_shading shading;
    if (!shading.create(render_target)) {
        return error::create_failed;
    }

    render_pass::ptr render_pass = shading.get_pass();

    ui32 frame_count = render_target->get_frame_count();

    block block;
    if (!block.create(device, frame_count, device->graphics_queue().family)) {
        return error::create_failed;
    }

    renderer renderer;
    if (!renderer.create(render_target->get_swapchain())) {
        return error::create_failed;
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
    if (!bindless_buffer.create_mapped(
            device, std::data(bindless_data),
            std::size(bindless_data) * sizeof(gpu_entity),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) {
        // return error::create_failed;
    }

    // lava::image::ptr image_buffer = lava::image::make({});

    // lava::buffer textures_buffer;
    // if (!textures_buffer.create_mapped(device, std::data(textures_data),
    //                                    std::size(textures_data),
    //                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) {
    //     // return error::create_failed;
    // }

    // Put a cube into the `render_vertices`.
    mesh::ptr mesh = create_mesh(device, mesh_type::cube);
    render_vertices.clear();
    render_vertices.insert(std::begin(render_vertices),
                           std::begin(mesh->get_vertices()),
                           std::end(mesh->get_vertices()));

    render_indices.clear();
    render_indices.insert(std::begin(render_indices),
                          std::begin(mesh->get_indices()),
                          std::end(mesh->get_indices()));

    // Initialize vertex/index buffers.
    vertices_buffer = lava::buffer::make();
    vertices_buffer->create(device, render_vertices.data(),
                            render_vertices.capacity() * sizeof(lava::vertex),
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, false,
                            VMA_MEMORY_USAGE_CPU_TO_GPU);

    indices_buffer = lava::buffer::make();
    indices_buffer->create(
        device, render_indices.data(), render_indices.capacity(),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, false, VMA_MEMORY_USAGE_CPU_TO_GPU);

    descriptor::ptr bindless_descriptor;
    descriptor::pool::ptr descriptor_pool;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    // descriptor sets must be made to transfer the shapes' world matrix
    // and the camera's view matrix to the physical device
    bindless_descriptor = descriptor::make();
    bindless_descriptor->add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     VK_SHADER_STAGE_VERTEX_BIT);
    if (!bindless_descriptor->create(device)) {
        //        return false;
    }

    descriptor_pool = descriptor::pool::make();
    if (!descriptor_pool->create(device,
                                 {
                                     {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
    })) {
        //        return false;
    }

    descriptor_set = bindless_descriptor->allocate(descriptor_pool->get());

    render_pass->set_clear_color({random(1.f), random(1.f), random(1.f)});

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

    mypipeline first_pipeline = make_first_pipeline(
        device, descriptor_set, bindless_descriptor, render_pass);
    // push this render pass to the pipeline
    render_pass->add_front(first_pipeline.pipeline);

    block.add_command([&](VkCommandBuffer cmd_buf) {
        std::memcpy(bindless_buffer.get_mapped_data(), std::data(bindless_data),
                    std::size(bindless_data) * sizeof(gpu_entity));

        render_pass->process(cmd_buf, block.get_current_frame());
    });

    window.on_resize = [&](ui32 new_width, ui32 new_height) {
        // return render_target->resize({480, 320});
        return render_target->resize({new_width, new_height});
    };

    frame.add_run([&](id::ref run_id) {
        input.handle_events();

        if (window.close_request()) {
            return frame.shut_down();
        }

        if (window.resize_request()) {
            return window.handle_resize();
        }

        if (window.iconified()) {
            frame.set_wait_for_events(true);
            return run_continue;
        } else {
            if (frame.waiting_for_events()) {
                frame.set_wait_for_events(false);
            }
        }

        optional_index current_frame = renderer.begin_frame();
        if (!current_frame.has_value()) {
            return run_continue;
        }

        if (!block.process(*current_frame)) {
            return run_abort;
        }

        return renderer.end_frame(block.collect_buffers());
    });

    frame.add_run_end([&]() {
        vertices_buffer->destroy();
        indices_buffer->destroy();

        bindless_descriptor->free(descriptor_set, descriptor_pool->get());
        descriptor_pool->destroy();
        bindless_descriptor->destroy();
        first_pipeline.destroy();

        block.destroy();
        shading.destroy();

        renderer.destroy();
        render_target->destroy();
    });

    return frame.run();
}
