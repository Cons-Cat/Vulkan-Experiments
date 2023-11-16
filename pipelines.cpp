#include "pipelines.hpp"

#include <liblava/block/descriptor.hpp>
#include <liblava/resource/mesh.hpp>
#include <liblava/resource/primitive.hpp>

#include <filesystem>

#include "types.hpp"

void add_vertex_shader_to(lava::render_pipeline::ptr& pipeline,
                          std::filesystem::path shader_path);

void add_fragment_shader_to(lava::render_pipeline::ptr& pipeline,
                            std::filesystem::path shader_path);

mypipeline make_color_pipeline(lava::device_p& device,
                               VkDescriptorSet& descriptor_set,
                               lava::descriptor::ptr& descriptor,
                               lava::render_pass::ptr& render_pass) {
    mypipeline out;

    out.pipeline = lava::render_pipeline::make(device);
    out.pipeline->add_color_blend_attachment();  // Color

    VkPipelineColorBlendAttachmentState id_attachment{
        .blendEnable = false,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT,
    };
    out.pipeline->add_color_blend_attachment(id_attachment);  // Entity ID

    out.pipeline->set_depth_test_and_write();
    out.pipeline->set_depth_compare_op(VK_COMPARE_OP_LESS_OR_EQUAL);

    out.pipeline->set_vertex_input_binding({
        .binding = 0u,
        .stride = sizeof(vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    });

    // only send position and color to shaders for this demo
    out.pipeline->set_vertex_input_attributes({
        {0, 0,    VK_FORMAT_R32G32B32_SFLOAT,  0}, // pos
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 12}, // color
        {2, 0,       VK_FORMAT_R32G32_SFLOAT, 28}, // uv
        {3, 0,    VK_FORMAT_R32G32B32_SFLOAT, 36}, // normal
        {4, 0,            VK_FORMAT_R32_UINT, 48}, // entity id
    });

    add_vertex_shader_to(out.pipeline, "/home/conscat/game/vertex.spirv");
    add_fragment_shader_to(out.pipeline, "/home/conscat/game/fragment.spirv");

    out.layout = lava::pipeline_layout::make();

    out.layout->add_push_constant_range(
        {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(cameras)});
    out.layout->add(descriptor);

    out.pipeline->set_layout(out.layout);

    if (!out.layout->create(device)) {
        //        return false;
    }

    if (!out.pipeline->create(render_pass->get())) {
        // return false;
    }

    out.pipeline->on_process = [&](VkCommandBuffer cmd_buf) {
        device->call().vkCmdPushConstants(cmd_buf, out.layout->get(),
                                          VK_SHADER_STAGE_VERTEX_BIT, 0,
                                          sizeof(cameras), std::data(viewproj));

        out.layout->bind(cmd_buf, descriptor_set);

        std::array<VkDeviceSize, 1> const buffer_offsets = {0};
        std::array<VkBuffer, 1> const buffers = {vertices_buffer->get()};

        vkCmdBindVertexBuffers(cmd_buf, 0, buffers.size(), buffers.data(),
                               buffer_offsets.data());

        vkCmdBindIndexBuffer(cmd_buf, indices_buffer->get(), 0,
                             VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd_buf, render_indices.size(), 1, 0, 0, 0);
    };

    return out;
}
