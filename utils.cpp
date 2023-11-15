#include <liblava/app/app.hpp>
#include <liblava/resource/mesh.hpp>

void init_pipeline(lava::render_pipeline::ptr& pipeline, lava::app& app) {
    pipeline = lava::render_pipeline::make(app.device, app.pipeline_cache);
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
