#include "bindless.hpp"

void buffer_storage::reset() {
    // `m_data`'s size member must be reset, but this does not reallocate.
    m_data.resize(vertices_offset);
    m_indices.clear();

    // Zero out the prologue data, which is safe and well-defined because
    // `member_type` and `std::byte` are trivial integers.
    // Do not wipe camera data or anything beyond.
    std::memset(m_data.data(), '\0', cameras_offset);
}

void buffer_storage::push_mesh(mesh const& mesh) {
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
    m_indices.insert(m_indices.end(), mesh.indices.begin(), mesh.indices.end());
}

void buffer_storage::push_indices() {
    // This assumes that no instances have been pushed yet.
    assert(get_instance_count() == 0);

    set_index_count(static_cast<member_type>(m_indices.size()));
    set_index_offset(static_cast<member_type>(m_data.size()));

    // Bit-copy the indices into `m_data`.
    std::byte* p_destination = m_data.data() + m_data.size();

    // Reserve storage in `m_data` for indices.
    m_data.resize(m_data.size() + (m_indices.size() * sizeof(index_type)));

    std::memcpy(p_destination, m_indices.data(),
                m_indices.size() * sizeof(index_type));

    // Place instance immediately after indices.
    set_instance_offset(static_cast<member_type>(m_data.size()));
}

void buffer_storage::push_instances(std::vector<instance> const& instances) {
    increment_instance_count();
    std::byte* p_destination = m_data.data() + m_data.size();

    vk::DrawIndexedIndirectCommand command;
    command.setFirstIndex(instances[0].index_offset)
        .setFirstInstance(0)
        .setIndexCount(instances[0].index_count)
        .setInstanceCount(static_cast<unsigned>(instances.size()))
        .setVertexOffset(0);

    // Reserve storage in `m_data` for this instance.
    m_data.resize(m_data.size() + sizeof(command));

    // TODO: Ensure this copy is vectorized. It could be one or two SIMD stores.
    // Bit-copy the instances command into `m_data`.
    __builtin_memcpy_inline(p_destination, &command, sizeof(command));
}
