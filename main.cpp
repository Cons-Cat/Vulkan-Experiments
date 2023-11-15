#include <liblava/lava.hpp>  // IWYU pragma: keep

#include "types.hpp"

void init_pipeline(lava::render_pipeline::ptr& pipeline, lava::app& app);

void add_shader_to(lava::render_pipeline::ptr& pipeline,
                   std::filesystem::path shader_path,
                   VkShaderStageFlagBits stage);

void add_vertex_shader_to(lava::render_pipeline::ptr& pipeline,
                          std::filesystem::path shader_path);

void add_fragment_shader_to(lava::render_pipeline::ptr& pipeline,
                            std::filesystem::path shader_path);

using namespace lava;

auto main(int argc, char* argv[]) -> int {
    engine app("game", {argc, argv});

    if (!app.setup()) {
        return error::not_ready;
    }

    // initialize camera
    glm::vec3 pos = {0, -4, 4};
    auto view = glm::lookAt(pos, {0, 0, 0}, {0, 0, 1});
    auto proj =
        glm::perspective(glm::radians(45.f),
                         static_cast<float>(app.target->get_size().x) /
                             static_cast<float>(app.target->get_size().y),
                         0.1f, 10.0f);

    glm::mat4 cameras[2]{view, proj};

    gpu_entity bindless_data[1] = {
        {{0, 0, 0}, {1, 0, 0, 1}}
    };

    // all shapes will share the same rotation value
    buffer bindless_buffer;
    if (!bindless_buffer.create_mapped(
            app.device, bindless_data,
            std::size(bindless_data) * sizeof(gpu_entity),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) {
        return error::create_failed;
    }

    mesh::ptr mesh = create_mesh(app.device, mesh_type::cube);

    descriptor::ptr descriptor;
    descriptor::pool::ptr descriptor_pool;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    render_pipeline::ptr pipeline;
    pipeline_layout::ptr layout;

    // Add key events before Vulkan events.
    constexpr float speed = 0.1;
    app.input.key.listeners.add([&](key_event::ref event) -> bool {
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
            return app.shut_down();
        }

        return input_done;
    });

    app.on_create = [&]() {
        init_pipeline(pipeline, app);

        add_vertex_shader_to(pipeline, "/home/conscat/game/vertex.spirv");
        add_fragment_shader_to(pipeline, "/home/conscat/game/fragment.spirv");

        // descriptor sets must be made to transfer the shapes' world matrix
        // and the camera's view matrix to the physical device
        descriptor = descriptor::make();
        descriptor->add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                VK_SHADER_STAGE_VERTEX_BIT);
        if (!descriptor->create(app.device)) {
            return false;
        }

        descriptor_pool = descriptor::pool::make();
        if (!descriptor_pool->create(
                app.device, {
                                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
        })) {
            return false;
        }

        layout = pipeline_layout::make();

        layout->add_push_constant_range(
            {VK_SHADER_STAGE_VERTEX_BIT, 0,
             std::size(cameras) * sizeof(glm::mat4x4)});
        layout->add(descriptor);

        if (!layout->create(app.device)) {
            return false;
        }

        pipeline->set_layout(layout);

        descriptor_set = descriptor->allocate(descriptor_pool->get());
        VkWriteDescriptorSet const bindless_descriptor{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = bindless_buffer.get_descriptor_info(),
        };
        app.device->vkUpdateDescriptorSets({
            bindless_descriptor,
        });

        render_pass::ptr render_pass = app.shading.get_pass();

        if (!pipeline->create(render_pass->get())) {
            return false;
        }

        // push this render pass to the pipeline
        render_pass->add_front(pipeline);

        pipeline->on_process = [&](VkCommandBuffer cmd_buf) {
            app.device->call().vkCmdPushConstants(
                cmd_buf, layout->get(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                std::size(cameras) * sizeof(glm::mat4x4), std::data(cameras));

            layout->bind(cmd_buf, descriptor_set);

            mesh->bind_draw(cmd_buf);
            return true;
        };

        return true;
    };

    app.on_destroy = [&]() {
        descriptor->free(descriptor_set, descriptor_pool->get());

        descriptor_pool->destroy();
        descriptor->destroy();

        pipeline->destroy();
        layout->destroy();
    };

    app.on_update = [&](delta dt) -> bool {
        std::memcpy(bindless_buffer.get_mapped_data(), std::data(bindless_data),
                    std::size(bindless_data) * sizeof(gpu_entity));

        return run_continue;
    };

    app.add_run_end([&]() {
        mesh->destroy();
    });

    return app.run();
}
