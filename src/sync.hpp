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

        queue.submit(submit_info);
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

        queue.submit(submit_info);
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
