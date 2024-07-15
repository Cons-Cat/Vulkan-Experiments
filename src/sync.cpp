#include "sync.hpp"

#include "vulkan_flow.hpp"

// void raster_steps::record_and_submit(
//     std::span<void (*)(vk::CommandBuffer&)> record_callbacks) {
//     for (std::size_t i = 0; i < record_callbacks.size(); ++i) {
//         vk::CommandBufferBeginInfo begin_info;
//         m_cmds[i].begin(begin_info);
//         set_all_render_state(m_cmds[i]);
//         record_callbacks[i](m_cmds[i]);
//         m_cmds.end();

//         m_steps[i].sema.signal_submit(m_steps[i].queue, m_cmds[i], 1);
//     }
// }
