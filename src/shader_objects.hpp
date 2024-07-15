#pragma once

#include <fstream>

#include "globals.hpp"

inline auto read_file(std::filesystem::path const& file_name)
    -> std::vector<char> {
    std::ifstream file(file_name, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(file_size));

    file.close();

    return buffer;
}

struct shader_objects_t {
    std::vector<vk::ShaderEXT> objects;

    void add_shader(
        std::filesystem::path const& shader_path,
        vk::ShaderStageFlagBits shader_stage,
        vk::ShaderStageFlagBits next_stage = vk::ShaderStageFlagBits{0}) {
        std::vector<char> spirv_source = read_file(shader_path);

        VkShaderCreateInfoEXT info =
            vk::ShaderCreateInfoEXT{}
                .setStage(shader_stage)
                .setNextStage(next_stage)
                .setCodeType(vk::ShaderCodeTypeEXT::eSpirv)
                .setPName("main")
                .setCodeSize(spirv_source.size())
                .setPCode(spirv_source.data())
                .setSetLayouts(g_descriptor_layout)
                .setPushConstantRanges(g_push_constants);

        VkShaderEXT p_shader;

        vulk.vkCreateShadersEXT(g_device, 1, &info, nullptr, &p_shader);
        objects.emplace_back(p_shader);
    }

    void add_compute_shader(std::filesystem::path const& shader_path) {
        add_shader(shader_path, vk::ShaderStageFlagBits::eCompute);
    }

    void add_vertex_shader(std::filesystem::path const& shader_path) {
        add_shader(shader_path, vk::ShaderStageFlagBits::eVertex,
                   vk::ShaderStageFlagBits::eFragment);
    }

    void add_fragment_shader(std::filesystem::path const& shader_path) {
        add_shader(shader_path, vk::ShaderStageFlagBits::eFragment);
    }

    void bind_compute(vk::CommandBuffer cmd, std::uint32_t index) {
        auto compute_bit = vk::ShaderStageFlagBits::eCompute;
        cmd.bindShadersEXT(1, &compute_bit, &objects[index]);
    }

    void bind_vertex(vk::CommandBuffer cmd, std::uint32_t index) {
        auto vertex_bit = vk::ShaderStageFlagBits::eVertex;
        cmd.bindShadersEXT(1, &vertex_bit, &objects[index]);
    }

    void bind_fragment(vk::CommandBuffer cmd, std::uint32_t index) {
        auto fragment_bit = vk::ShaderStageFlagBits::eFragment;
        cmd.bindShadersEXT(1, &fragment_bit, &objects[index]);
    }

    void destroy() {
        for (auto& shader : objects) {
            vulk.vkDestroyShaderEXT(g_device, shader, nullptr);
        }
    }
};

inline constinit shader_objects_t shader_objects;
