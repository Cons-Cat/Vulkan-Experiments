#include <liblava/lava.hpp>  // IWYU pragma: keep

using namespace lava;

auto main(int argc, char* argv[]) -> int {
    engine app("game", {argc, argv});

    if (!app.setup()) {
        return error::not_ready;
    }

    // initialize camera
    app.camera.position = v3(0.f, -2.f, 4.f);
    app.camera.rotation = v3(-25.f, 0.f, 0.f);  // degrees

    // mat4 world_matrix = glm::identity<mat4>();
    std::byte bindless_data[4];

    // all shapes will share the same rotation value
    buffer bindless_buffer;
    if (!bindless_buffer.create_mapped(app.device, bindless_data,
                                       sizeof(bindless_data),
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) {
        return error::create_failed;
    }

    mesh::ptr triangle = create_mesh(app.device, mesh_type::triangle);

    descriptor::ptr descriptor;
    descriptor::pool::ptr descriptor_pool;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    render_pipeline::ptr pipeline;
    pipeline_layout::ptr layout;

    app.on_create = [&]() {
        pipeline = render_pipeline::make(app.device, app.pipeline_cache);
        pipeline->add_color_blend_attachment();
        pipeline->set_depth_test_and_write();
        pipeline->set_depth_compare_op(VK_COMPARE_OP_LESS_OR_EQUAL);

        // all shapes use the same simple shaders
        std::vector<char> shader_source;
        read_file(shader_source, "/home/conscat/game/vertex.spirv");
        if (!pipeline->add_shader_stage(
                cdata(shader_source.data(), shader_source.size()),
                VK_SHADER_STAGE_VERTEX_BIT)) {
            return false;
        }

        read_file(shader_source, "/home/conscat/game/fragment.spirv");
        if (!pipeline->add_shader_stage(
                cdata(shader_source.data(), shader_source.size()),
                VK_SHADER_STAGE_FRAGMENT_BIT)) {
            return false;
        }

        pipeline->set_vertex_input_binding(
            {0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX});

        // only send position and color to shaders for this demo
        pipeline->set_vertex_input_attributes({
            {0, 0,    VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, position)},
            {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex,    color)},
            {2, 0,       VK_FORMAT_R32G32_SFLOAT, offsetof(vertex,       uv)},
            {3, 0,    VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex,   normal)},
        });

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

        glm::mat4 cameras[2]{app.camera.get_view(),
                             app.camera.get_projection()};

        layout = pipeline_layout::make();

        layout->add_push_constant_range(
            {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(cameras)});
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
            app.device->call().vkCmdPushConstants(cmd_buf, layout->get(),
                                                  VK_SHADER_STAGE_VERTEX_BIT, 0,
                                                  sizeof(cameras), cameras);

            layout->bind(cmd_buf, descriptor_set);
            return true;
        };

        return true;
    };

    app.on_destroy = [&]() {
        // descriptor->free(descriptor_set, descriptor_pool->get());

        // descriptor_pool->destroy();
        // descriptor->destroy();

        // pipeline->destroy();
        // layout->destroy();
    };

    app.on_update = [&](delta dt) {
        // rotation_vector += v3{0, 1.f, 0} * dt;
        // memcpy(as_ptr(rotation_buffer.get_mapped_data()),
        //&rotation_vector, sizeof(rotation_vector));

        if (app.camera.activated()) {
            app.camera.update_view(to_dt(app.run_time.delta),
                                   app.input.get_mouse_position());
        }

        return run_continue;
    };

    app.add_run_end([&]() {
        triangle->destroy();
    });

    return app.run();
}
