#include <liblava/lava.hpp>  // IWYU pragma: keep

#include "types.hpp"

void init_pipeline(lava::render_pipeline::ptr& pipeline,
                   lava::device_p& device);

void add_shader_to(lava::render_pipeline::ptr& pipeline,
                   std::filesystem::path shader_path,
                   VkShaderStageFlagBits stage);

void add_vertex_shader_to(lava::render_pipeline::ptr& pipeline,
                          std::filesystem::path shader_path);

void add_fragment_shader_to(lava::render_pipeline::ptr& pipeline,
                            std::filesystem::path shader_path);

void init_render_pass(lava::render_pass::ptr&, lava::image::ptr&,
                      lava::render_target::ptr&);

using namespace lava;

inline gpu_entity bindless_data[1] = {
    {{0, 0, 0}, {1, 0, 0, 1}}
};

inline constexpr ui16 game_width = 480;
inline constexpr ui16 game_height = 320;

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

    device_p device = frame.platform.create_device();
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
    auto view = glm::lookAt(pos, {0, 0, 0}, {0, 0, 1});
    auto proj =
        glm::perspective(glm::radians(45.f),
                         static_cast<float>(render_target->get_size().x) /
                             static_cast<float>(render_target->get_size().y),
                         0.1f, 10.0f);

    glm::mat4 cameras[2]{view, proj};

    // all shapes will share the same rotation value
    buffer bindless_buffer;
    if (!bindless_buffer.create_mapped(
            device, bindless_data,
            std::size(bindless_data) * sizeof(gpu_entity),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) {
        return error::create_failed;
    }

    mesh::ptr mesh = create_mesh(device, mesh_type::cube);

    descriptor::ptr descriptor;
    descriptor::pool::ptr descriptor_pool;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    render_pipeline::ptr render_pipeline;
    pipeline_layout::ptr layout;

    init_pipeline(render_pipeline, device);

    add_vertex_shader_to(render_pipeline, "/home/conscat/game/vertex.spirv");
    add_fragment_shader_to(render_pipeline,
                           "/home/conscat/game/fragment.spirv");

    // descriptor sets must be made to transfer the shapes' world matrix
    // and the camera's view matrix to the physical device
    descriptor = descriptor::make();
    descriptor->add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            VK_SHADER_STAGE_VERTEX_BIT);
    if (!descriptor->create(device)) {
        //        return false;
    }

    descriptor_pool = descriptor::pool::make();
    if (!descriptor_pool->create(device,
                                 {
                                     {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
    })) {
        //        return false;
    }

    layout = pipeline_layout::make();

    layout->add_push_constant_range({VK_SHADER_STAGE_VERTEX_BIT, 0,
                                     std::size(cameras) * sizeof(glm::mat4x4)});
    layout->add(descriptor);

    if (!layout->create(device)) {
        //        return false;
    }

    render_pipeline->set_layout(layout);
    render_pass->add_front(render_pipeline);

    descriptor_set = descriptor->allocate(descriptor_pool->get());

    render_pass->set_clear_color({random(1.f), random(1.f), random(1.f)});

    VkWriteDescriptorSet const bindless_descriptor{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = bindless_buffer.get_descriptor_info(),
    };

    device->vkUpdateDescriptorSets({
        bindless_descriptor,
    });

    if (!render_pipeline->create(render_pass->get())) {
        // return false;
    }

    // push this render pass to the pipeline
    render_pass->add_front(render_pipeline);

    render_pipeline->on_process = [&](VkCommandBuffer cmd_buf) {
        device->call().vkCmdPushConstants(
            cmd_buf, layout->get(), VK_SHADER_STAGE_VERTEX_BIT, 0,
            std::size(cameras) * sizeof(glm::mat4x4), std::data(cameras));

        layout->bind(cmd_buf, descriptor_set);

        mesh->bind_draw(cmd_buf);
    };

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
        descriptor->free(descriptor_set, descriptor_pool->get());
        descriptor_pool->destroy();
        descriptor->destroy();
        render_pipeline->destroy();
        layout->destroy();

        block.destroy();
        shading.destroy();

        renderer.destroy();
        render_target->destroy();

        mesh->destroy();
    });

    return frame.run();
}
