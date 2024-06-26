#include "bindless.hpp"

#include <vulkan/vulkan.hpp>

#include "light.hpp"

void buffer_storage::reset() {
    // `m_data`'s size member must be reset, but this does not reallocate.
    m_data.resize(vertices_offset);
    m_indices.clear();
    m_instance_properties.clear();
    m_counts.clear();
    m_instance_count = 0;

    // Zero out the prologue data, which is safe and well-defined because
    // `member_type` and `std::byte` are trivial integers.
    // Do not wipe camera data or anything beyond.
    std::memset(m_data.data(), '\0', cameras_offset);
}

void buffer_storage::push_mesh(mesh const& mesh) {
    // Remember the offsets for this mesh.
    m_counts.emplace_back(
        // Vertex offset:
        get_vertex_count(),
        // Index offset:
        m_indices.size());

    // This assumes the vector pointer is properly aligned, which is ensured
    // by `buffer_storage`'s constructor.
    std::byte* p_destination = m_data.data() + m_data.size();

    // This assumes that no indices have been pushed yet. That means
    // `push_mesh` can only be called in a sequence following the
    // `buffer_storage` constructor or `.reset()`.
    assert(get_index_count() == 0);

    // Reserve storage in `m_data` for `mesh`.
    m_data.resize(m_data.size() + (mesh.m_vertices.size() * sizeof(vertex)));

    // Bit-copy the mesh into `m_data`.
    std::memcpy(p_destination, mesh.m_vertices.data(),
                mesh.m_vertices.size() * sizeof(vertex));
    add_vertex_count(static_cast<member_type>(mesh.m_vertices.size()));

    // Copy the mesh's indices into `m_indices` to be concatenated onto
    // `m_data` in the future with `.push_indices()`.
    m_indices.insert(m_indices.end(), mesh.m_indices.begin(),
                     mesh.m_indices.end());
}

void buffer_storage::push_indices() {
    // This assumes that no instances have been pushed yet.
    assert(get_instance_commands_count() == 0);

    set_index_count(static_cast<member_type>(m_indices.size()));
    set_index_offset(static_cast<member_type>(m_data.size()));

    // Bit-copy the indices into `m_data`.
    std::byte* p_destination = m_data.data() + m_data.size();

    // Reserve storage in `m_data` for indices.
    m_data.resize(m_data.size() + (m_indices.size() * sizeof(index_type)));

    std::memcpy(p_destination, m_indices.data(),
                m_indices.size() * sizeof(index_type));

    // Place instance immediately after indices.
    set_instance_commands_offset(static_cast<member_type>(m_data.size()));
}

void buffer_storage::push_instances_of(
    std::size_t mesh_index, std::span<mesh_instance const> const instances) {
    increment_instance_command_count();
    std::byte* p_destination = m_data.data() + m_data.size();

    unsigned instance_index_count = instances.front().index_count;
    int instance_index_offset = instances.front().index_offset;
    // unsigned instance_first_id = instances.front().id;

    // All indices must be the same across these instances, because they are
    // pushed as a single command.
    for (auto&& i : instances) {
        assert(i.index_count == instance_index_count);
        assert(i.index_offset == instance_index_offset);
        // assert(i.id == instance_first_id);
    }

    vk::DrawIndexedIndirectCommand command{};
    command
        // Instances:
        .setFirstInstance(m_instance_count)
        .setInstanceCount(static_cast<unsigned>(instances.size()))
        // Vertices:
        .setVertexOffset(m_counts[mesh_index].vertex_offset)
        // Indices:
        .setFirstIndex(static_cast<unsigned>(m_counts[mesh_index].index_offset +
                                             instance_index_offset))
        .setIndexCount(instance_index_count);
    m_instance_count += instances.size();

    // Reserve storage in `m_data` for these instances.
    m_data.resize(m_data.size() + sizeof(command));
    std::memcpy(p_destination, &command, sizeof(command));

    // Copy the instance's properties into `m_instance_properties` to be
    // concatenated onto `m_data` in the future with `.push_properties()`.
    for (auto&& i : instances) {
        // Generate a new instance ID if one is not specified.
        if (i.id == 0) {
            ++g_next_instance_id;
        }
        unsigned id = (i.id == 0) ? g_next_instance_id : i.id;
        m_instance_properties.emplace_back(i.position, i.rotation, i.scaling,
                                           i.color_blend, id);
    }
}

void buffer_storage::push_properties() {
    std::byte* p_destination = m_data.data() + m_data.size();

    // TODO: Align this without looping.
    // This must be aligned, because it stores matrices.
    while (!is_aligned(p_destination, alignof(property))) {
        m_data.resize(m_data.size() + 4);
        p_destination += 4;
    }

    set_properties_offset(static_cast<member_type>(m_data.size()));

    // Reserve storage in `m_data` for command properties.
    assert(m_data.size() + (m_instance_properties.size() * sizeof(property)) <
           m_data.capacity());
    m_data.resize(m_data.size() +
                  (m_instance_properties.size() * sizeof(property)));

    // Bit-copy the properties into `m_data`.
    std::memcpy(p_destination, m_instance_properties.data(),
                m_instance_properties.size() * sizeof(property));

    // Push light sources.
    set_lights_count(g_lights.size());
    set_lights_offset(static_cast<member_type>(m_data.size()));
    p_destination = m_data.data() + m_data.size();
    // Reserve storage in `m_data` for command properties.
    m_data.resize(m_data.size() + (g_lights.size() * sizeof(light_t::light)));
    // Bit-copy the lights into `m_data`.
    std::memcpy(p_destination, g_lights.lights.data(),
                g_lights.size() * sizeof(light_t::light));
}
