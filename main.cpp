// Disable these warnings for Vookoo.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <vku/vku.hpp>

#include <WSIWindow.h>
#pragma GCC diagnostic pop

#include <vulkan/vulkan.hpp>

#include <VkBootstrap.h>
#include <cstddef>
#include <fstream>
#include <iostream>

#include "defer.hpp"

// Vulkan confuses the leak sanitizer.
extern "C" auto __asan_default_options() -> char const* {  // NOLINT
    return "detect_leaks=0";
}

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

inline constinit auto& vulk = VULKAN_HPP_DEFAULT_DISPATCHER;

inline constexpr uint32_t max_frames_in_flight = 3;
inline constexpr uint32_t game_width = 480;
inline constexpr uint32_t game_height = 320;
inline constexpr auto depth_format = vk::Format::eD32Sfloat;

// TODO: Remove this.
// inline std::array<float, 4> g_bindless_data = {1, 1, 0, 1};

inline vkb::PhysicalDevice g_physical_device;
// This is optional to defer initialization:
inline std::optional<vkb::SwapchainBuilder> swapchain_builder;

inline vk::Queue g_graphics_queue;
inline uint32_t g_graphics_queue_index;
inline vk::Queue g_present_queue;
inline uint32_t g_present_queue_index;

inline vkb::Swapchain g_swapchain;
inline std::vector<VkImage> g_swapchain_images{};
inline std::vector<VkImageView> g_swapchain_views{};
inline vku::ColorAttachmentImage g_color_image;
inline vku::ColorAttachmentImage g_normal_image;
inline vku::ColorAttachmentImage g_id_image;
inline vku::DepthStencilImage g_depth_image;

struct alignas(16) vertex {
    // NOLINTNEXTLINE
    constexpr vertex(float x, float y, float z = 0.f, float w = 1.f)
        : x(x), y(y), z(z), w(w) {
    }

    float x;  // NOLINT
    float y;  // NOLINT
    float z;  // NOLINT
    float w;  // NOLINT
};

using index_type = unsigned int;

struct mesh {
    std::vector<vertex> vertices;
    std::vector<index_type> indices;
};

class buffer_storage {
  public:
    static constexpr unsigned char vertices_offset = 32;
    static constexpr unsigned char member_stride = 4;
    using member_type = unsigned int;

    constexpr buffer_storage() : m_data(2'048z) {
        reset();
        //  The vector is already zero-initialized here.
        //  Ensure that vector pointer is properly aligned for pushing vertices.
        std::byte* p_destination = m_data.data() + m_data.size();
        assert((reinterpret_cast<std::uintptr_t>(p_destination) &
                static_cast<std::uintptr_t>(alignof(vertex) - 1u)) == 0u);
    }

    auto data() -> std::byte* {
        return m_data.data();
    }

    [[nodiscard]]
    auto size() const -> std::size_t {
        return m_data.capacity();
    }

    void reset() {
        // `m_data`'s size member must be reset, but this does not reallocate.
        m_data.resize(vertices_offset);
        m_indices.clear();

        // Zero out the prologue data, which is safe and well-defined because
        // `member_type` and `std::byte` are trivial integers.
        std::memset(m_data.data(), '\0', vertices_offset);
    }

    template <typename T>
    void set_at(T&& value, std::size_t byte_offset) {
        new (m_data.data() + byte_offset) std::decay_t<T>(fwd(value));
    }

    template <typename T>
    [[nodiscard]]
    auto get_at(std::size_t byte_offset) const -> T {
        return *__builtin_bit_cast(T*, m_data.data() + byte_offset);
    }

    void set_vertex_count(member_type count) {
        set_at(count, 0);
    }

    [[nodiscard]]
    auto get_vertex_count() const -> member_type {
        return get_at<member_type>(0);
    }

    void set_index_count(member_type count) {
        set_at(count, member_stride * 1z);
    }

    [[nodiscard]]
    auto get_index_count() const -> member_type {
        return get_at<member_type>(member_stride * 1z);
    }

    void set_index_offset(member_type count) {
        set_at(count, member_stride * 2z);
    }

    [[nodiscard]]
    auto get_index_offset() const -> member_type {
        return get_at<member_type>(member_stride * 2z);
    }

    void set_material_count(member_type count) {
        set_at(count, member_stride * 3z);
    }

    [[nodiscard]]
    auto get_material_count() const -> member_type {
        return get_at<member_type>(member_stride * 3z);
    }

    void set_instance_count(member_type count) {
        set_at(count, member_stride * 4z);
    }

    [[nodiscard]]
    auto get_instance_count() const -> member_type {
        return get_at<member_type>(member_stride * 4z);
    }

    void set_texture_count(member_type count) {
        set_at(count, member_stride * 5z);
    }

    void push_mesh(mesh const& mesh) {
        // This assumes the vector pointer is properly aligned, which is ensured
        // by `buffer_storage`'s constructor.
        std::byte* p_destination = m_data.data() + m_data.size();

        // This assumes that no indices have been pushed yet. That means
        // `push_mesh` can only be called in a sequence following the
        // `buffer_storage` constructor or `.reset()`.
        assert(get_index_count() == 0);

        // Reserve storage in `m_data` for `mesh`.
        m_data.resize(m_data.size() + (mesh.vertices.size() * sizeof(vertex)));

        // Bit-copy the mesh into `m_data`.
        std::memcpy(p_destination, mesh.vertices.data(),
                    mesh.vertices.size() * sizeof(vertex));
        add_vertex_count(static_cast<member_type>(mesh.vertices.size()));

        // Copy the mesh's indices into `m_indices` to be concatenated onto
        // `m_data` in the future with `.push_indices()`.
        m_indices.insert(m_indices.end(), mesh.indices.begin(),
                         mesh.indices.end());
    }

    void push_indices() {
        set_index_count(static_cast<member_type>(m_indices.size()));
        set_index_offset(static_cast<member_type>(m_data.size()));

        // Bit-copy the indices into `m_data`.
        std::byte* p_destination = m_data.data() + m_data.size();
        // m_data.resize(m_data.size() + (m_indices.size() *
        // sizeof(index_type)));

        std::memcpy(p_destination, m_indices.data(),
                    m_indices.size() * sizeof(index_type));
    }

  private:
    void add_vertex_count(member_type count) {
        set_vertex_count(get_vertex_count() + count);
    }

    std::vector<std::byte> m_data;
    std::vector<index_type> m_indices;
};

vku::GenericBuffer g_buffer;

inline VkCommandPool g_command_pool;
inline std::vector<vk::CommandBuffer> g_command_buffers;

inline std::vector<VkSemaphore> g_available_semaphores;
inline std::vector<VkSemaphore> g_finished_semaphore;
inline std::vector<VkFence> g_in_flight_fences;
inline std::vector<VkFence> g_image_in_flight;

inline vk::DescriptorSet g_descriptor_set;
inline vk::DescriptorSetLayout g_descriptor_layout;
inline vk::PipelineLayout g_pipeline_layout;

auto make_device(vkb::Instance instance, vk::SurfaceKHR surface) -> vk::Device {
    vkb::PhysicalDeviceSelector physical_device_selector(instance);
    physical_device_selector
        .add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)
        .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
        .add_required_extension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);

    auto maybe_physical_device =
        physical_device_selector.set_surface(surface).select();
    if (!maybe_physical_device) {
        std::cout << maybe_physical_device.error().message() << '\n';
        std::quick_exit(1);
    }
    g_physical_device = *maybe_physical_device;
    std::cout << g_physical_device.name << '\n';

    vk::PhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature(
        vk::True);
    vk::PhysicalDeviceBufferDeviceAddressFeaturesKHR device_address_feature(
        vk::True, vk::True, vk::True, &dynamic_rendering_feature);
    vk::PhysicalDeviceShaderObjectFeaturesEXT shader_object_feature(
        vk::True, &device_address_feature);

    vkb::DeviceBuilder device_builder{g_physical_device};
    device_builder.add_pNext(&shader_object_feature);
    auto maybe_device = device_builder.build();
    if (!maybe_device) {
        std::cout << maybe_device.error().message() << '\n';
    }
    auto device = *maybe_device;

    // Initialize queues.
    g_graphics_queue = *device.get_queue(vkb::QueueType::graphics);
    g_graphics_queue_index = *device.get_queue_index(vkb::QueueType::graphics);

    g_present_queue = *device.get_queue(vkb::QueueType::present);
    g_present_queue_index = *device.get_queue_index(vkb::QueueType::present);

    swapchain_builder = vkb::SwapchainBuilder{device};

    return device.device;
}

inline vk::Device device;

void create_swapchain() {
    assert(swapchain_builder);

    // Initialize swapchain.
    auto maybe_swapchain =
        swapchain_builder->set_old_swapchain(g_swapchain).build();
    if (!maybe_swapchain) {
        std::cout << maybe_swapchain.error().message() << ' '
                  << maybe_swapchain.vk_result() << '\n';
    }

    // Destroy the old swapchain if it exists, and create a new one.
    // vkb::destroy_swapchain(g_swapchain);
    g_swapchain = *maybe_swapchain;
    g_swapchain_images = *g_swapchain.get_images();
    g_swapchain_views = *g_swapchain.get_image_views();
}

auto read_file(std::filesystem::path const& file_name) -> vector<char> {
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
    std::vector<vk::ShaderEXT> objects;  // NOLINT

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
                .setSetLayouts(g_descriptor_layout);

        VkShaderEXT p_shader;

        vulk.vkCreateShadersEXT(device, 1, &info, nullptr, &p_shader);
        objects.emplace_back(p_shader);
    }

    void add_vertex_shader(std::filesystem::path const& shader_path) {
        add_shader(shader_path, vk::ShaderStageFlagBits::eVertex,
                   vk::ShaderStageFlagBits::eFragment);
    }

    void add_fragment_shader(std::filesystem::path const& shader_path) {
        add_shader(shader_path, vk::ShaderStageFlagBits::eFragment);
    }

    void bind_vertex(vk::CommandBuffer cmd, uint32_t index) {
        auto frag_bit = vk::ShaderStageFlagBits::eVertex;
        cmd.bindShadersEXT(1, &frag_bit, &objects[index]);
    }

    void bind_fragment(vk::CommandBuffer cmd, uint32_t index) {
        auto frag_bit = vk::ShaderStageFlagBits::eFragment;
        cmd.bindShadersEXT(1, &frag_bit, &objects[index]);
    }

    void destroy() {
        for (auto& shader : objects) {
            vulk.vkDestroyShaderEXT(device, shader, nullptr);
        }
    }
} shader_objects;

void create_command_pool() {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = g_graphics_queue_index;

    vulk.vkCreateCommandPool(device, &pool_info, nullptr, &g_command_pool);
}

void create_command_buffers() {
    vk::CommandBufferAllocateInfo info{
        g_command_pool, vk::CommandBufferLevel::ePrimary, max_frames_in_flight};
    g_command_buffers = device.allocateCommandBuffers(info);
}

void create_sync_objects() {
    g_available_semaphores.resize(max_frames_in_flight);
    g_finished_semaphore.resize(max_frames_in_flight);
    g_in_flight_fences.resize(max_frames_in_flight);
    g_image_in_flight.resize(g_swapchain.image_count, VK_NULL_HANDLE);

    // TODO: Use hpp bindings for this.

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < max_frames_in_flight; i++) {
        vulk.vkCreateSemaphore(device, &semaphore_info, nullptr,
                               &g_available_semaphores[i]);

        vulk.vkCreateSemaphore(device, &semaphore_info, nullptr,
                               &g_finished_semaphore[i]);

        vulk.vkCreateFence(device, &fence_info, nullptr,
                           &g_in_flight_fences[i]);
    }
}

void recreate_swapchain();

void render_and_present() {
    static uint32_t frame = 0;

    constexpr auto timeout = std::numeric_limits<uint64_t>::max();

    // Wait for host to signal the fence for this swapchain frame.
    vulk.vkWaitForFences(device, 1, &g_in_flight_fences[frame], vk::True,
                         timeout);

    // Get a swapchain index that is currently presentable.
    uint32_t image_index;
    auto error = static_cast<vk::Result>(vulk.vkAcquireNextImageKHR(
        device, g_swapchain.swapchain, timeout, g_available_semaphores[frame],
        nullptr, &image_index));

    if (error == vk::Result::eErrorOutOfDateKHR) {
        recreate_swapchain();
        return;
    }

    if (g_image_in_flight[image_index] != VK_NULL_HANDLE) {
        vulk.vkWaitForFences(device, 1, &g_image_in_flight[image_index],
                             vk::True, timeout);
    }

    g_image_in_flight[image_index] = g_in_flight_fences[frame];

    std::array<vk::Semaphore, 1> wait_semaphores = {
        g_available_semaphores[frame],
    };
    std::array<vk::Semaphore, 1> signal_semaphores = {
        g_finished_semaphore[frame],
    };
    std::array<vk::PipelineStageFlags, 1> wait_stages = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
    };

    // Submit commands to the graphics queue.
    vk::SubmitInfo submit_info;
    submit_info.setWaitSemaphores(wait_semaphores)
        .setWaitDstStageMask(wait_stages)
        .setCommandBufferCount(1)
        .setPCommandBuffers(&g_command_buffers[image_index])
        .setSignalSemaphores(signal_semaphores);

    vulk.vkResetFences(device, 1, &g_in_flight_fences[frame]);

    vulk.vkQueueSubmit(g_graphics_queue, 1, (VkSubmitInfo*)&submit_info,
                       g_in_flight_fences[frame]);

    // After rendering to the swapchain frame completes, present it to the
    // surface.
    std::array present_indices = {image_index};
    std::array<vk::SwapchainKHR, 1> swapchains = {g_swapchain.swapchain};
    vk::PresentInfoKHR present_info;
    present_info.setImageIndices(present_indices)
        .setWaitSemaphores(signal_semaphores)
        .setSwapchains(swapchains);

    error = (vk::Result)vulk.vkQueuePresentKHR(
        g_present_queue, (VkPresentInfoKHR*)&present_info);
    if (error == vk::Result::eErrorOutOfDateKHR) {
        recreate_swapchain();
        return;
    }

    frame = (frame + 1) % max_frames_in_flight;
}

void set_all_render_state(vk::CommandBuffer cmd) {
    cmd.setLineWidth(1.0);
    cmd.setCullMode(vk::CullModeFlagBits::eNone);
    cmd.setPolygonModeEXT(vk::PolygonMode::eFill);
    vk::ColorBlendEquationEXT color_blend_equations[4]{};
    cmd.setColorBlendEquationEXT(4, color_blend_equations);
    cmd.setRasterizerDiscardEnable(vk::False);
    cmd.setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);

    VkSampleMask sample_mask = 0x1;
    cmd.setSampleMaskEXT(vk::SampleCountFlagBits::e1, &sample_mask);
    cmd.setAlphaToCoverageEnableEXT(vk::False);

    cmd.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
    cmd.setPrimitiveRestartEnable(vk::False);

    cmd.setVertexInputEXT(0, nullptr, 0, nullptr);

    cmd.setDepthClampEnableEXT(vk::False);

    cmd.setDepthBiasEnable(vk::False);
    cmd.setDepthTestEnable(vk::True);
    cmd.setDepthWriteEnable(vk::True);
    cmd.setDepthBoundsTestEnable(vk::False);

    cmd.setFrontFace(vk::FrontFace::eClockwise);
    cmd.setDepthCompareOp(vk::CompareOp::eLessOrEqual);

    cmd.setStencilTestEnable(vk::False);

    cmd.setLogicOpEnableEXT(vk::False);

    cmd.setColorBlendEnableEXT(0, vk::False);
    cmd.setColorBlendEnableEXT(1, vk::False);
    cmd.setColorBlendEnableEXT(2, vk::False);

    constexpr vk::Flags color_write_mask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    cmd.setColorWriteMaskEXT(0, 1, &color_write_mask);

    constexpr vk::Flags normal_write_mask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    cmd.setColorWriteMaskEXT(1, 1, &normal_write_mask);

    constexpr vk::Flags id_write_mask = vk::ColorComponentFlagBits::eR;
    cmd.setColorWriteMaskEXT(2, 1, &id_write_mask);

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, g_pipeline_layout,
                           0, g_descriptor_set, nullptr);
}

void record_rendering(std::size_t const frame) {
    vk::CommandBuffer& cmd = g_command_buffers[frame];
    vk::CommandBufferBeginInfo begin_info;
    cmd.begin(begin_info);

    vk::ClearColorValue clear_color = {1.f, 0.f, 1.f, 0.f};
    vk::ClearColorValue black_clear_color = {0, 0, 0, 1};
    vk::ClearColorValue depth_clear_color = {1.f, 0.f, 1.f, 1.f};

    vk::Viewport viewport;
    viewport.setWidth(game_width)
        .setHeight(game_height)
        .setX(0)
        .setY(0)
        .setMinDepth(0.f)
        .setMaxDepth(1.f);
    vk::Rect2D scissor;
    scissor.setOffset({0, 0}).setExtent(g_swapchain.extent);
    vk::Rect2D render_area;
    render_area.setOffset({0, 0}).setExtent(g_swapchain.extent);

    cmd.setViewportWithCount(1, &viewport);
    cmd.setScissorWithCount(1, &scissor);

    vk::RenderingAttachmentInfoKHR color_attachment_info;
    color_attachment_info.setClearValue(clear_color)
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(g_color_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfoKHR normal_attachment_info;
    normal_attachment_info.setClearValue(black_clear_color)
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(g_normal_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingAttachmentInfoKHR id_attachment_info;
    id_attachment_info.setClearValue(black_clear_color)
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(g_id_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setResolveMode(vk::ResolveModeFlagBits::eNone);

    std::array attachments = {color_attachment_info, normal_attachment_info,
                              id_attachment_info};

    vk::RenderingAttachmentInfoKHR depth_attachment_info;
    depth_attachment_info.setClearValue(depth_clear_color)
        .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
        .setImageView(g_depth_image.imageView())
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setResolveMode(vk::ResolveModeFlagBits::eNone);

    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(render_area)
        .setLayerCount(1)
        .setColorAttachments(attachments)
        .setPDepthAttachment(&depth_attachment_info);

    g_color_image.setLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    g_normal_image.setLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    g_id_image.setLayout(cmd, vk::ImageLayout::eColorAttachmentOptimal);
    g_depth_image.setLayout(cmd, vk::ImageLayout::eDepthAttachmentOptimal,
                            vk::ImageAspectFlagBits::eDepth);

    cmd.beginRendering(rendering_info);

    set_all_render_state(cmd);

    // Draw world.
    shader_objects.bind_vertex(cmd, 0);
    shader_objects.bind_fragment(cmd, 1);

    cmd.draw(3, 1, 0, 0);

    cmd.endRendering();

    // Post processing.
    g_color_image.setLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
    g_normal_image.setLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);
    g_id_image.setLayout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal);

    cmd.setDepthTestEnable(vk::False);
    cmd.setDepthWriteEnable(vk::False);

    // Transition swapchain image layout to color write.
    VkImageMemoryBarrier const render_memory_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = g_swapchain_images[frame],
        .subresourceRange = {
                             .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1,
                             }
    };
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,              // srcStageMask
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // dstStageMask
        0, 0, nullptr, 0, nullptr,
        1,                      // imageMemoryBarrierCount
        &render_memory_barrier  // pImageMemoryBarriers
    );

    vk::RenderingAttachmentInfoKHR swapchain_attachment_info;
    swapchain_attachment_info
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImageView(g_swapchain_views[frame])
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    rendering_info.setColorAttachments(swapchain_attachment_info)
        .setPDepthAttachment(nullptr);

    cmd.beginRendering(rendering_info);

    shader_objects.bind_vertex(cmd, 2);
    shader_objects.bind_fragment(cmd, 3);

    cmd.draw(3, 1, 0, 0);

    cmd.endRendering();

    // Transition swapchain image layout to present.
    VkImageMemoryBarrier const present_memory_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = g_swapchain_images[frame],
        .subresourceRange = {
                             .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1,
                             }
    };
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,           // dstStageMask
        0, 0, nullptr, 0, nullptr,
        1,                       // imageMemoryBarrierCount
        &present_memory_barrier  // pImageMemoryBarriers
    );

    cmd.end();
}

void recreate_swapchain() {
    device.waitIdle();
    device.destroyCommandPool(g_command_pool);
    g_swapchain.destroy_image_views(g_swapchain_views);

    create_swapchain();
    create_command_pool();
    create_command_buffers();

    for (uint32_t i = 0; i < g_command_buffers.size(); ++i) {
        record_rendering(i);
    }
}

struct my_window final : public WSIWindow {
    // Override virtual functions.
    // NOLINTNEXTLINE I can't control this API.
    void OnResizeEvent(uint16_t width, uint16_t height) final {
    }

    void OnKeyEvent(eAction action, eKeycode keycode) final {
        if (action == eDOWN) {
            switch (keycode) {
                case KEY_Escape:
                    Close();
                    break;
            }
        }
    }
};

auto main() -> int {
    vk::DynamicLoader vkloader;
    vulk.init();

    vkb::InstanceBuilder instance_builder;
    vkb::Result maybe_instance = instance_builder.request_validation_layers()
                                     .require_api_version(1, 3, 0)
                                     .use_default_debug_messenger()
                                     .build();
    if (!maybe_instance) {
        std::cout << maybe_instance.error().message() << '\n';
        std::quick_exit(1);
    }
    vkb::Instance instance = *maybe_instance;

    vulk.init(vk::Instance{instance.instance});

    my_window win;
    win.SetTitle("");
    win.SetWinSize(game_width, game_height);
    vk::SurfaceKHR surface =
        static_cast<VkSurfaceKHR>(win.GetSurface(instance));

    // Initialize global device.
    device = make_device(instance, surface);
    vulk.init(device);

    create_swapchain();
    defer {
        vkb::destroy_swapchain(g_swapchain);
    };

    create_command_pool();
    defer {
        device.destroyCommandPool(g_command_pool);
    };

    g_color_image = vku::ColorAttachmentImage(
        device, g_physical_device.memory_properties, game_width, game_height,
        vk::Format::eR32G32B32A32Sfloat);

    g_normal_image = vku::ColorAttachmentImage(
        device, g_physical_device.memory_properties, game_width, game_height,
        vk::Format::eR32G32B32A32Sfloat);

    g_id_image = vku::ColorAttachmentImage(
        device, g_physical_device.memory_properties, game_width, game_height,
        vk::Format::eR32Uint);

    g_depth_image =
        vku::DepthStencilImage(device, g_physical_device.memory_properties,
                               game_width, game_height, depth_format);

    vku::SamplerMaker sampler_maker;
    vk::Sampler nearest_sampler = sampler_maker.create(device);

    create_sync_objects();
    defer {
        for (auto& semaphore : g_finished_semaphore) {
            device.destroySemaphore(semaphore);
        }
        for (auto& semaphore : g_available_semaphores) {
            device.destroySemaphore(semaphore);
        }
        for (auto& fence : g_in_flight_fences) {
            device.destroyFence(fence);
        }
    };

    create_command_buffers();

    vku::DescriptorSetLayoutMaker dslm;
    g_descriptor_layout =
        dslm.buffer(0, vk::DescriptorType::eStorageBuffer,
                    vk::ShaderStageFlagBits::eAllGraphics, 1)
            .image(1, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 1)
            .image(2, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 1)
            .image(3, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 1)
            .createUnique(device)
            .release();

    vk::PipelineLayoutCreateInfo pipeline_info;
    pipeline_info.setSetLayouts(g_descriptor_layout)
        .setFlags(vk::PipelineLayoutCreateFlags());
    g_pipeline_layout = device.createPipelineLayout(pipeline_info);

    std::vector<vk::DescriptorPoolSize> pool_sizes;
    pool_sizes.emplace_back(vk::DescriptorType::eStorageBuffer, 2);
    pool_sizes.emplace_back(vk::DescriptorType::eCombinedImageSampler, 3);

    // Create an arbitrary number of descriptors in a pool.
    // Allow the descriptors to be freed, possibly not optimal behaviour.
    vk::DescriptorPoolCreateInfo descriptor_pool_info;
    descriptor_pool_info
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setPoolSizes(pool_sizes)
        .setMaxSets(1);
    vk::DescriptorPool descriptor_pool =
        device.createDescriptorPool(descriptor_pool_info);
    defer {
        device.destroyDescriptorPool(descriptor_pool);
    };

    // Bindless storage buffer.
    buffer_storage bindless_data;

    // TODO: `g_buffer` is hard coded to 2 kibibytes, which might be a problem
    // later.
    g_buffer = vku::GenericBuffer(device, g_physical_device.memory_properties,
                                  vk::BufferUsageFlagBits::eStorageBuffer |
                                      vk::BufferUsageFlagBits::eTransferDst,
                                  bindless_data.size());

    // g_buffer.upload(device, g_physical_device.memory_properties,
    // g_command_pool,
    //                 g_graphics_queue, bindless_data.data());

    // g_buffer.upload(device, g_physical_device.memory_properties,
    // g_command_pool,
    //                 g_graphics_queue, bindless_data.data());

    vku::DescriptorSetMaker dsm;
    dsm.layout(g_descriptor_layout);
    g_descriptor_set = dsm.create(device, descriptor_pool).front();

    vku::DescriptorSetUpdater dsu;
    dsu.beginDescriptorSet(g_descriptor_set)
        .beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer)
        .buffer(g_buffer.buffer(), 0, vk::WholeSize)

        .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
        .image(nearest_sampler, g_color_image.imageView(),
               vk::ImageLayout::eShaderReadOnlyOptimal)

        .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
        .image(nearest_sampler, g_normal_image.imageView(),
               vk::ImageLayout::eShaderReadOnlyOptimal)

        .beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler)
        .image(nearest_sampler, g_id_image.imageView(),
               vk::ImageLayout::eShaderReadOnlyOptimal)
        .update(device);
    assert(dsu.ok());

    // Compile and link shaders.
    shader_objects.add_vertex_shader(getexepath().parent_path() /
                                     "../vertex.spv");
    shader_objects.add_fragment_shader(getexepath().parent_path() /
                                       "../fragment.spv");

    shader_objects.add_vertex_shader(getexepath().parent_path() /
                                     "../composite_vertex.spv");
    shader_objects.add_fragment_shader(getexepath().parent_path() /
                                       "../composite_fragment.spv");
    defer {
        shader_objects.destroy();
    };

    mesh triangle;
    triangle.vertices = {
        {  0.f, -0.5f},
        { 0.8f,  0.5f},
        {-0.5f,  0.8f}
    };

    // Add a triangle to be rendered.
    bindless_data.push_mesh(triangle);

    // Update the vertex buffer memory.
    g_buffer.upload(device, g_physical_device.memory_properties, g_command_pool,
                    g_graphics_queue, bindless_data.data(),
                    bindless_data.size());

    for (std::size_t i = 0; i < g_command_buffers.size(); ++i) {
        record_rendering(i);
    }

    // Game loop.
    while (win.ProcessEvents()) {
        render_and_present();
    }

    device.waitIdle();
}
