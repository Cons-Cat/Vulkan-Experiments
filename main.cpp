#include <vulkan/vulkan.hpp>

#include <VkBootstrap.h>
#include <WSIWindow.h>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "defer.hpp"

inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
inline constexpr uint32_t game_width = 480;
inline constexpr uint32_t game_height = 320;

struct my_window final : public WSIWindow {
    // Override virtual functions.
    void OnResizeEvent(uint16_t width, uint16_t height) {
    }
};

auto depthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

struct Init {
    vkb::Instance instance;
    vkb::PhysicalDevice gpu;
    vkb::InstanceDispatchTable inst_disp;
    VkSurfaceKHR surface;
    vkb::Device device;
    vkb::DispatchTable disp;
    vkb::Swapchain swapchain;
};

struct RenderData {
    VkQueue graphics_queue;
    VkQueue present_queue;

    std::vector<VkImage> swapchain_color_images;
    std::vector<VkImageView> swapchain_color_image_views;
    std::vector<VkImage> swapchain_depth_images;
    std::vector<VkImageView> swapchain_depth_image_views;
    std::vector<VkDeviceMemory> depth_memories;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;

    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> command_buffers;

    std::vector<VkSemaphore> available_semaphores;
    std::vector<VkSemaphore> finished_semaphore;
    std::vector<VkFence> in_flight_fences;
    std::vector<VkFence> image_in_flight;
    size_t current_frame = 0;
};

int device_initialization(Init& init) {
    init.inst_disp = init.instance.make_table();

    vkb::PhysicalDeviceSelector phys_device_selector(init.instance);
    phys_device_selector
        .add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)
        .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
        .add_required_extension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME)
        .add_required_extension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);

    auto phys_device_ret =
        phys_device_selector.set_surface(init.surface).select();
    if (!phys_device_ret) {
        std::cout << phys_device_ret.error().message() << "\n";
        return -1;
    }
    init.gpu = phys_device_ret.value();

    vk::PhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature(true);
    vk::PhysicalDeviceBufferDeviceAddressFeaturesKHR device_address_feature(
        true, true, true, &dynamic_rendering_feature);

    vkb::DeviceBuilder device_builder{init.gpu};
    device_builder.add_pNext(&device_address_feature);
    auto device_ret = device_builder.build();
    if (!device_ret) {
        std::cout << device_ret.error().message() << "\n";
        return -1;
    }
    init.device = device_ret.value();

    init.disp = init.device.make_table();
    // init.disp.fp_vkDestroyShaderEXT = init.disp.fun

    return 0;
}

int create_swapchain(Init& init) {
    vkb::SwapchainBuilder swapchain_builder{init.device};
    auto swap_ret = swapchain_builder.set_old_swapchain(init.swapchain).build();
    if (!swap_ret) {
        std::cout << swap_ret.error().message() << " " << swap_ret.vk_result()
                  << "\n";
        return -1;
    }
    vkb::destroy_swapchain(init.swapchain);
    init.swapchain = swap_ret.value();
    return 0;
}

int get_queues(Init& init, RenderData& data) {
    auto gq = init.device.get_queue(vkb::QueueType::graphics);
    if (!gq.has_value()) {
        std::cout << "failed to get graphics queue: " << gq.error().message()
                  << "\n";
        return -1;
    }
    data.graphics_queue = gq.value();

    auto pq = init.device.get_queue(vkb::QueueType::present);
    if (!pq.has_value()) {
        std::cout << "failed to get present queue: " << pq.error().message()
                  << "\n";
        return -1;
    }
    data.present_queue = pq.value();
    return 0;
}

std::vector<char> readFile(std::string const& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

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

uint32_t findMemoryType(VkPhysicalDevice& gpu, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(gpu, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void createImage(Init& init, uint32_t width, uint32_t height, VkFormat format,
                 VkImageTiling tiling, VkImageUsageFlags usage,
                 VkMemoryPropertyFlags properties, VkImage& image,
                 VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(init.device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(init.device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        init.gpu.physical_device, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(init.device, &allocInfo, nullptr, &imageMemory) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(init.device, image, imageMemory, 0);
}

VkImageView createImageView(Init& init, VkImage& image, VkFormat format,
                            VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(init.device, &viewInfo, nullptr, &imageView) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }

    return imageView;
}

int create_swapchain_images(Init& init, RenderData& data) {
    data.swapchain_color_images = init.swapchain.get_images().value();
    data.swapchain_color_image_views = init.swapchain.get_image_views().value();
    data.swapchain_depth_images.resize(data.swapchain_color_images.size());
    data.swapchain_depth_image_views.resize(
        data.swapchain_color_image_views.size());
    data.depth_memories.resize(data.swapchain_color_images.size());

    // TODO: Memory leaks of depth memory
    for (int i = 0; i < data.swapchain_depth_images.size(); ++i) {
        createImage(init, init.swapchain.extent.width,
                    init.swapchain.extent.height, depthFormat,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    data.swapchain_depth_images[i], data.depth_memories[i]);
        data.swapchain_depth_image_views[i] =
            createImageView(init, data.swapchain_depth_images[i], depthFormat,
                            VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    return 0;
}

int create_command_pool(Init& init, RenderData& data) {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex =
        init.device.get_queue_index(vkb::QueueType::graphics).value();

    if (init.disp.createCommandPool(&pool_info, nullptr, &data.command_pool) !=
        VK_SUCCESS) {
        std::cout << "failed to create command pool\n";
        return -1;  // failed to create command pool
    }
    return 0;
}

int create_command_buffers(Init& init, RenderData& data) {
    data.command_buffers.resize(3);  // TODO: Do not hard code this!

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = data.command_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)data.command_buffers.size();

    if (init.disp.allocateCommandBuffers(
            &allocInfo, data.command_buffers.data()) != VK_SUCCESS) {
        return -1;  // failed to allocate command buffers;
    }

    auto vert_code =
        readFile("/home/conscat/game/vk-bootstrap/build/example/vert.spv");
    auto frag_code =
        readFile("/home/conscat/game/vk-bootstrap/build/example/frag.spv");

    std::array<VkShaderEXT, 2> shaders;

    VkShaderCreateInfoEXT vertex_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .nextStage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .codeType = VkShaderCodeTypeEXT::VK_SHADER_CODE_TYPE_SPIRV_EXT,
        .codeSize = vert_code.size(),
        .pCode = vert_code.data(),
        .pName = "main",
    };

    VkShaderCreateInfoEXT fragment_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .codeType = VkShaderCodeTypeEXT::VK_SHADER_CODE_TYPE_SPIRV_EXT,
        .codeSize = frag_code.size(),
        .pCode = frag_code.data(),
        .pName = "main",
    };

    std::array infos = {vertex_info, fragment_info};

    init.disp.createShadersEXT(infos.size(), infos.data(), nullptr,
                               shaders.data());

    for (size_t i = 0; i < data.command_buffers.size(); i++) {
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (init.disp.beginCommandBuffer(data.command_buffers[i],
                                         &begin_info) != VK_SUCCESS) {
            return -1;  // failed to begin recording command buffer
        }

        VkClearValue clearColor{{{1.0f, 0.0f, 1.0f, 1.0f}}};

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)init.swapchain.extent.width;

        viewport.height = (float)init.swapchain.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = init.swapchain.extent;

        init.disp.cmdSetViewport(data.command_buffers[i], 0, 1, &viewport);
        init.disp.cmdSetScissor(data.command_buffers[i], 0, 1, &scissor);

        VkRenderingAttachmentInfoKHR const color_attachment_info{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .imageView = data.swapchain_color_image_views[i],
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clearColor,
        };

        VkRenderingAttachmentInfoKHR const depth_attachment_info{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .imageView = data.swapchain_depth_image_views[i],
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clearColor,
        };

        VkRect2D render_area = {
            .offset = {0, 0},
            .extent = init.swapchain.extent,
        };

        VkRenderingInfoKHR const render_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
            .renderArea = render_area,
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment_info,
            .pDepthAttachment = &depth_attachment_info,
        };

        VkImageMemoryBarrier const image_render_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = data.swapchain_color_images[i],
            .subresourceRange = {
                                 .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1,
                                 }
        };

        init.disp.cmdPipelineBarrier(
            data.command_buffers[i],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,              // srcStageMask
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // dstStageMask
            0, 0, nullptr, 0, nullptr,
            1,                     // imageMemoryBarrierCount
            &image_render_barrier  // pImageMemoryBarriers
        );

        init.disp.cmdBeginRendering(data.command_buffers[i], &render_info);

        init.disp.cmdSetCullMode(data.command_buffers[i], VK_CULL_MODE_NONE);
        init.disp.cmdSetDepthWriteEnable(data.command_buffers[i], VK_FALSE);

        auto vert_bit = VK_SHADER_STAGE_VERTEX_BIT;
        auto frag_bit = VK_SHADER_STAGE_FRAGMENT_BIT;
        init.disp.cmdBindShadersEXT(data.command_buffers[i], 1, &vert_bit,
                                    &shaders[0]);
        init.disp.cmdBindShadersEXT(data.command_buffers[i], 1, &frag_bit,
                                    &shaders[1]);

        init.disp.cmdDraw(data.command_buffers[i], 3, 1, 0, 0);

        init.disp.cmdEndRendering(data.command_buffers[i]);

        VkImageMemoryBarrier const image_present_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = data.swapchain_color_images[i],
            .subresourceRange = {
                                 .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1,
                                 }
        };

        init.disp.cmdPipelineBarrier(
            data.command_buffers[i],
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,           // dstStageMask
            0, 0, nullptr, 0, nullptr,
            1,                      // imageMemoryBarrierCount
            &image_present_barrier  // pImageMemoryBarriers
        );

        if (init.disp.endCommandBuffer(data.command_buffers[i]) != VK_SUCCESS) {
            std::cout << "failed to record command buffer\n";
            return -1;  // failed to record command buffer!
        }
    }
    return 0;
}

int create_sync_objects(Init& init, RenderData& data) {
    data.available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    data.finished_semaphore.resize(MAX_FRAMES_IN_FLIGHT);
    data.in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
    data.image_in_flight.resize(init.swapchain.image_count, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (init.disp.createSemaphore(&semaphore_info, nullptr,
                                      &data.available_semaphores[i]) !=
                VK_SUCCESS ||
            init.disp.createSemaphore(&semaphore_info, nullptr,
                                      &data.finished_semaphore[i]) !=
                VK_SUCCESS ||
            init.disp.createFence(&fence_info, nullptr,
                                  &data.in_flight_fences[i]) != VK_SUCCESS) {
            std::cout << "failed to create sync objects\n";
            return -1;  // failed to create synchronization objects for a frame
        }
    }
    return 0;
}

int recreate_swapchain(Init& init, RenderData& data) {
    init.disp.deviceWaitIdle();

    init.disp.destroyCommandPool(data.command_pool, nullptr);

    init.swapchain.destroy_image_views(data.swapchain_color_image_views);

    if (0 != create_swapchain(init)) {
        return -1;
    }
    if (0 != create_command_pool(init, data)) {
        return -1;
    }
    if (0 != create_command_buffers(init, data)) {
        return -1;
    }
    return 0;
}

int draw_frame(Init& init, RenderData& data) {
    init.disp.waitForFences(1, &data.in_flight_fences[data.current_frame],
                            VK_TRUE, UINT64_MAX);

    uint32_t image_index = 0;
    VkResult result = init.disp.acquireNextImageKHR(
        init.swapchain, UINT64_MAX,
        data.available_semaphores[data.current_frame], VK_NULL_HANDLE,
        &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return recreate_swapchain(init, data);
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cout << "failed to acquire swapchain image. Error " << result
                  << "\n";
        return -1;
    }

    if (data.image_in_flight[image_index] != VK_NULL_HANDLE) {
        init.disp.waitForFences(1, &data.image_in_flight[image_index], VK_TRUE,
                                UINT64_MAX);
    }
    data.image_in_flight[image_index] =
        data.in_flight_fences[data.current_frame];

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {
        data.available_semaphores[data.current_frame]};
    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = wait_semaphores;
    submitInfo.pWaitDstStageMask = wait_stages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &data.command_buffers[image_index];

    VkSemaphore signal_semaphores[] = {
        data.finished_semaphore[data.current_frame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signal_semaphores;

    init.disp.resetFences(1, &data.in_flight_fences[data.current_frame]);

    if (init.disp.queueSubmit(data.graphics_queue, 1, &submitInfo,
                              data.in_flight_fences[data.current_frame]) !=
        VK_SUCCESS) {
        std::cout << "failed to submit draw command buffer\n";
        return -1;  //"failed to submit draw command buffer
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapChains[] = {init.swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapChains;

    present_info.pImageIndices = &image_index;

    result = init.disp.queuePresentKHR(data.present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return recreate_swapchain(init, data);
    } else if (result != VK_SUCCESS) {
        std::cout << "failed to present swapchain image\n";
        return -1;
    }

    data.current_frame = (data.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    return 0;
}

void cleanup(Init& init, RenderData& data) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        init.disp.destroySemaphore(data.finished_semaphore[i], nullptr);
        init.disp.destroySemaphore(data.available_semaphores[i], nullptr);
        init.disp.destroyFence(data.in_flight_fences[i], nullptr);
    }

    init.disp.destroyCommandPool(data.command_pool, nullptr);

    init.swapchain.destroy_image_views(data.swapchain_color_image_views);
    for (auto& i : data.swapchain_depth_images) {
        init.disp.destroyImage(i, nullptr);
    }
    for (auto& i : data.swapchain_depth_image_views) {
        init.disp.destroyImageView(i, nullptr);
    }
    for (auto& i : data.depth_memories) {
        init.disp.freeMemory(i, nullptr);
    }

    vkb::destroy_swapchain(init.swapchain);
    vkb::destroy_device(init.device);
}

int main() {
    Init init;
    RenderData render_data;

    vkb::InstanceBuilder instance_builder;
    auto maybe_instance = instance_builder
                              .use_default_debug_messenger()
                              //.request_validation_layers()
                              .require_api_version(1, 3, 0)
                              .build();
    if (!maybe_instance) {
        std::cout << maybe_instance.error().message() << "\n";
        return -1;
    }
    init.instance = maybe_instance.value();

    my_window win;
    win.SetTitle("");
    win.SetWinSize(game_width, game_height);
    init.surface = static_cast<VkSurfaceKHR>(win.GetSurface(init.instance));

    if (0 != device_initialization(init)) {
        return -1;
    }
    if (0 != create_swapchain(init)) {
        return -1;
    }
    if (0 != get_queues(init, render_data)) {
        return -1;
    }
    if (0 != create_swapchain_images(init, render_data)) {
        return -1;
    }
    if (0 != create_command_pool(init, render_data)) {
        return -1;
    }
    if (0 != create_command_buffers(init, render_data)) {
        return -1;
    }
    if (0 != create_sync_objects(init, render_data)) {
        return -1;
    }

    while (win.ProcessEvents()) {
        int res = draw_frame(init, render_data);
        if (res != 0) {
            std::cout << "failed to draw frame \n";
            return -1;
        }

        if (win.GetKeyState(eKeycode::KEY_Escape)) {
            win.Close();
        }
    }
    init.disp.deviceWaitIdle();

    cleanup(init, render_data);
    return 0;
}
