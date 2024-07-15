#pragma once

#include <vulkan/vulkan.hpp>

#include "globals.hpp"

struct timeline_semaphore {
    // Hello!
    timeline_semaphore(std::size_t frame_width) : m_frame_width(frame_width) {
        vk::SemaphoreTypeCreateInfo type_info;
        type_info.setSemaphoreType(vk::SemaphoreType::eTimeline)
            .setInitialValue(0);

        vk::SemaphoreCreateInfo sema_info;
        sema_info.setPNext(&type_info);

        m_sema = g_device.createSemaphore(sema_info);
    }

    // Timeline semaphores can only be updated monotonically, so the base index
    // must be incremented each frame.
    void reset() {
        m_frame_idx += m_frame_width;
    }

    void signal_submit(vk::Queue& queue, vk::CommandBuffer& cmd,
                       std::size_t signal_idx) {
        std::size_t real_signal_idx = m_frame_width * m_frame_idx + signal_idx;

        vk::TimelineSemaphoreSubmitInfo sema_info;
        sema_info.setSignalSemaphoreValues(real_signal_idx);

        vk::SubmitInfo submit_info;
        submit_info.setPNext(&sema_info)
            .setSignalSemaphores(m_sema)
            .setCommandBuffers(cmd);
    }

    void wait_submit(vk::Queue& queue, vk::CommandBuffer& cmd,
                     std::size_t wait_idx) {
        std::size_t real_wait_idx = m_frame_width * m_frame_idx + wait_idx;

        vk::TimelineSemaphoreSubmitInfo sema_info;
        sema_info.setWaitSemaphoreValues(real_wait_idx);

        vk::SubmitInfo submit_info;
        submit_info.setPNext(&sema_info)
            .setWaitSemaphores(m_sema)
            .setCommandBuffers(cmd);
    }

    void signal_wait_submit(vk::Queue& queue, vk::CommandBuffer& cmd,
                            std::size_t signal_idx, std::size_t wait_idx) {
        std::size_t real_signal_idx = m_frame_width * m_frame_idx + signal_idx;
        std::size_t real_wait_idx = m_frame_width * m_frame_idx + wait_idx;

        vk::TimelineSemaphoreSubmitInfo sema_info;
        sema_info.setSignalSemaphoreValues(real_signal_idx);
        sema_info.setWaitSemaphoreValues(real_wait_idx);

        vk::SubmitInfo submit_info;
        submit_info.setPNext(&sema_info)
            .setSignalSemaphores(m_sema)
            .setWaitSemaphores(m_sema)
            .setCommandBuffers(cmd);
    }

    void signal(std::size_t signal_idx) const {
        std::size_t real_signal_idx = m_frame_width * m_frame_idx + signal_idx;

        vk::SemaphoreSignalInfo info;
        info.setSemaphore(m_sema).setValue(real_signal_idx);

        g_device.signalSemaphore(info);
    }

    void wait(std::size_t wait_idx) const {
        std::size_t real_wait_idx = m_frame_width * m_frame_idx + wait_idx;

        vk::SemaphoreWaitInfo info;
        info.setSemaphores(m_sema).setValues(real_wait_idx);

        // TODO: Handle timeout error.
        auto _ = g_device.waitSemaphores(
            info, std::numeric_limits<std::size_t>::max());
    }

    vk::Semaphore m_sema;
    std::size_t const m_frame_width;
    std::size_t m_frame_idx = 0;
};

/*
struct raster_steps {
    struct step {
        vk::Queue queue;
        std::size_t frag_shader_idx;
        timeline_semaphore sema;
    };

    raster_steps(std::span<std::size_t> frag_shaders, std::size_t frame)
        : m_frame(frame) {
        std::size_t size = frag_shaders.size();
        m_steps.reserve(size);
        m_cmds.resize(size);
        m_semas.resize(size);

        vk::CommandBufferAllocateInfo cmd_info{g_command_pool,
                                               vk::CommandBufferLevel::ePrimary,
                                               static_cast<uint32_t>(size)};
        m_cmds = g_device.allocateCommandBuffers(cmd_info);

        for (std::size_t i = 0; i < size; ++i) {
            m_steps.push_back({
                .queue = g_graphics_queues[2 + (i * frame)],
                .frag_shader_idx = frag_shaders[i],
                .sema = {2},
            });

            // Shallow copy semaphores into a vector that can be waited on.
            m_semas[i] = m_steps[i].sema.m_sema;
        }
    }

    void record_and_submit(
        std::span<void (*)(vk::CommandBuffer&)> record_callbacks);

    std::size_t const m_frame;
    std::vector<step> m_steps;
    std::vector<vk::Semaphore> m_semas;
    std::vector<vk::CommandBuffer> m_cmds;
};

inline std::array<raster_steps, max_frames_in_flight> g_raster_steps;
*/
