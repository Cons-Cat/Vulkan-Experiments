#include <liblava/app/app.hpp>
#include <liblava/resource/mesh.hpp>
#include <types.hpp>

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

lava::render_pass::ptr init_color_pass() {
    lava::render_pass::ptr pass = lava::render_pass::make(device);

    lava::attachment::ptr color_attachment =
        lava::attachment::make(VK_FORMAT_B8G8R8A8_UNORM);
    color_attachment->set_op(VK_ATTACHMENT_LOAD_OP_CLEAR,
                             VK_ATTACHMENT_STORE_OP_STORE);
    color_attachment->set_stencil_op(VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                     VK_ATTACHMENT_STORE_OP_DONT_CARE);
    color_attachment->set_layouts(VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    auto depth_format =
        lava::find_supported_depth_format(device->get_vk_physical_device());
    lava::attachment::ptr depth_attachment =
        lava::attachment::make(depth_format.value());
    depth_attachment->set_op(VK_ATTACHMENT_LOAD_OP_CLEAR,
                             VK_ATTACHMENT_STORE_OP_STORE);
    depth_attachment->set_stencil_op(VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                     VK_ATTACHMENT_STORE_OP_DONT_CARE);
    depth_attachment->set_layouts(
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    lava::attachment::ptr id_attachment =
        lava::attachment::make(VK_FORMAT_R32_UINT);
    id_attachment->set_op(VK_ATTACHMENT_LOAD_OP_CLEAR,
                          VK_ATTACHMENT_STORE_OP_STORE);
    id_attachment->set_stencil_op(VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE);
    id_attachment->set_layouts(VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    pass->add(color_attachment);
    pass->add(depth_attachment);
    pass->add(id_attachment);

    auto subpass = lava::subpass::make(VK_PIPELINE_BIND_POINT_GRAPHICS);
    subpass->add_color_attachment(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    subpass->set_depth_stencil_attachment(
        1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    subpass->add_color_attachment(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
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

    pass->create(
        {
    },
        {{}, {game_width, game_height}});
    pass->set_clears_count(3);
    pass->set_clear_color();

    return pass;
}

lava::render_pass::ptr init_composite_pass(
    lava::render_target::ptr& target, lava::attachment::ptr& color_attachment,
    lava::attachment::ptr& depth_attachment) {
    lava::render_pass::ptr pass = lava::render_pass::make(device);

    pass->add(color_attachment);
    pass->add(depth_attachment);

    auto subpass = lava::subpass::make(VK_PIPELINE_BIND_POINT_GRAPHICS);
    subpass->add_color_attachment(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
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

    pass->create({}, {{}, target->get_size()});
    pass->set_clears_count(2);
    pass->set_clear_color();

    return pass;
}
