#include <liblava/app/app.hpp>
#include <liblava/resource/mesh.hpp>

void init_pipeline(lava::render_pipeline::ptr& pipeline,
                   lava::device_p& device) {
    pipeline = lava::render_pipeline::make(device);
    pipeline->add_color_blend_attachment();
    pipeline->set_depth_test_and_write();
    pipeline->set_depth_compare_op(VK_COMPARE_OP_LESS_OR_EQUAL);

    pipeline->set_vertex_input_binding(
        {0, sizeof(lava::vertex), VK_VERTEX_INPUT_RATE_VERTEX});

    // only send position and color to shaders for this demo
    pipeline->set_vertex_input_attributes({
        {0, 0,    VK_FORMAT_R32G32B32_SFLOAT, offsetof(lava::vertex, position)},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(lava::vertex,    color)},
        {2, 0,       VK_FORMAT_R32G32_SFLOAT, offsetof(lava::vertex,       uv)},
        {3, 0,    VK_FORMAT_R32G32B32_SFLOAT, offsetof(lava::vertex,   normal)},
    });
}

void add_shader_to(lava::render_pipeline::ptr& pipeline,
                   std::filesystem::path shader_path,
                   VkShaderStageFlagBits stage) {
    std::vector<char> shader_source;
    lava::read_file(shader_source, shader_path);
    if (!pipeline->add_shader_stage(
            lava::cdata(shader_source.data(), shader_source.size()), stage)) {
        // return false;
        // TODO: Error handling is necessary here.
    }
}

void add_vertex_shader_to(lava::render_pipeline::ptr& pipeline,
                          std::filesystem::path shader_path) {
    add_shader_to(pipeline, shader_path, VK_SHADER_STAGE_VERTEX_BIT);
}

void add_fragment_shader_to(lava::render_pipeline::ptr& pipeline,
                            std::filesystem::path shader_path) {
    add_shader_to(pipeline, shader_path, VK_SHADER_STAGE_FRAGMENT_BIT);
}

void init_render_pass(lava::render_pass::ptr& pass,
                      lava::image::ptr& depth_stencil,
                      lava::render_target::ptr& target) {
    auto depth_format = lava::find_supported_depth_format(
        target->get_device()->get_vk_physical_device());
    if (!depth_format.has_value()) {
        // return false;
    }

    pass = lava::render_pass::make(target->get_device());
    {
        auto color_attachment = lava::attachment::make(target->get_format());
        color_attachment->set_op(VK_ATTACHMENT_LOAD_OP_CLEAR,
                                 VK_ATTACHMENT_STORE_OP_STORE);
        color_attachment->set_stencil_op(VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                         VK_ATTACHMENT_STORE_OP_DONT_CARE);
        color_attachment->set_layouts(VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        pass->add(color_attachment);

        auto depth_attachment = lava::attachment::make(*depth_format);
        depth_attachment->set_op(VK_ATTACHMENT_LOAD_OP_CLEAR,
                                 VK_ATTACHMENT_STORE_OP_DONT_CARE);
        depth_attachment->set_stencil_op(VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                         VK_ATTACHMENT_STORE_OP_DONT_CARE);
        depth_attachment->set_layouts(
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        pass->add(depth_attachment);

        auto subpass = lava::subpass::make(VK_PIPELINE_BIND_POINT_GRAPHICS);
        subpass->set_color_attachment(0,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        subpass->set_depth_stencil_attachment(
            1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        pass->add(subpass);

        auto first_subpass_dependency =
            lava::subpass_dependency::make(VK_SUBPASS_EXTERNAL, 0);
        first_subpass_dependency->set_stage_mask(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        first_subpass_dependency->set_access_mask(
            VK_ACCESS_MEMORY_READ_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        pass->add(first_subpass_dependency);

        auto second_subpass_dependency =
            lava::subpass_dependency::make(0, VK_SUBPASS_EXTERNAL);
        second_subpass_dependency->set_stage_mask(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        second_subpass_dependency->set_access_mask(
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        pass->add(second_subpass_dependency);
    }

    depth_stencil = lava::image::make(*depth_format);
    if (!depth_stencil) {
        // return false;
    }

    depth_stencil->set_usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    depth_stencil->set_layout(VK_IMAGE_LAYOUT_UNDEFINED);
    depth_stencil->set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT |
                                   VK_IMAGE_ASPECT_STENCIL_BIT);
    depth_stencil->set_component();

    target->on_create_attachments = [&]() -> lava::VkAttachments {
        lava::VkAttachments result;

        if (!depth_stencil->create(target->get_device(), target->get_size())) {
            return {};
        }

        for (auto& backbuffer : target->get_backbuffers()) {
            lava::VkImageViews attachments;

            attachments.push_back(backbuffer->get_view());
            attachments.push_back(depth_stencil->get_view());

            result.push_back(attachments);
        }

        return result;
    };

    // target->on_destroy_attachments = [&]() {
    //     depth_stencil->destroy();
    // };

    if (!pass->create(target->on_create_attachments(),
                      {{}, target->get_size()})) {
        // return false;
    }

    target->add_callback(&pass->get_target_callback());

    pass->set_clear_color();
}
