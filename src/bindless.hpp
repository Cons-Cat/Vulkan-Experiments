#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include <cassert>
#include <span>
#include <vector>

#include "defer.hpp"
#include "glm/fwd.hpp"

struct vertex {
    constexpr vertex() = default;

    // NOLINTNEXTLINE
    constexpr vertex(float x, float y, float z = 0.f, float w = 1.f)
        : position(x, y, z, w) {
    }

    alignas(16) glm::vec4 position;
    alignas(16) glm::vec3 normal;
};

using index_type = unsigned int;

struct mesh {
    constexpr mesh(std::vector<vertex>&& verts,
                   std::vector<index_type>&& inds) {
        assert((inds.size() % 3) == 0);
        m_vertices = std::move(verts);
        m_indices = std::move(inds);

        for (std::size_t i = 0; i < m_indices.size(); i += 3) {
            glm::vec3 a = m_vertices[m_indices[i]].position;
            glm::vec3 b = m_vertices[m_indices[i + 1]].position;
            glm::vec3 c = m_vertices[m_indices[i + 2]].position;
            glm::vec3 ab = b - a;
            glm::vec3 ac = c - a;

            glm::vec3 normal = glm::cross(ab, ac);
            m_vertices[m_indices[i]].normal = normal;
            m_vertices[m_indices[i + 1]].normal = normal;
            m_vertices[m_indices[i + 2]].normal = normal;
        }
    }

    std::vector<vertex> m_vertices;
    std::vector<index_type> m_indices;
};

struct mesh_instance {
    alignas(16) glm::vec3 position;
    alignas(16) glm::fquat rotation;
    alignas(16) glm::vec3 scaling = glm::vec3(1);
    alignas(16) glm::vec4 color_blend = glm::vec4(1);
    unsigned id = 0;
    signed int index_offset;
    index_type index_count;
};

template <typename T>
inline auto is_aligned(T* p_data, std::uintptr_t alignment) -> bool {
    return (reinterpret_cast<std::uintptr_t>(p_data) & alignment - 1u) == 0u;
}

class buffer_storage {
  public:
    // This matches `buffer_storage` in `shaders.slang`:
    static constexpr unsigned int cameras_offset = 64;
    static constexpr unsigned int vertices_offset = 256;
    static_assert(vertices_offset >= cameras_offset + sizeof(glm::mat4x4) * 2);

    static constexpr unsigned int member_stride = 4;
    using member_type = unsigned int;

    buffer_storage() : m_data(2'048z * 8) {
        reset();

        //  The vector is already zero-initialized here.
        //  Ensure that vector pointer is properly aligned for pushing vertices.
        std::byte* p_destination = m_data.data() + m_data.size();
        assert(is_aligned(p_destination, alignof(vertex)));
    }

    auto data() -> std::byte* {
        return m_data.data();
    }

    [[nodiscard]]
    auto capacity() const -> std::size_t {
        return m_data.capacity();
    }

    void reset();

    template <typename T>
    void set_at(T&& value, std::size_t byte_offset) {
        new (m_data.data() + byte_offset) std::decay_t<T>(fwd(value));
    }

    template <typename T>
    [[nodiscard]]
    auto get_at(std::size_t byte_offset) const -> T const& {
        return *__builtin_bit_cast(T*, m_data.data() + byte_offset);
    }

    void set_vertex_count(member_type count) {
        set_at(count, 0);
    }

    [[nodiscard]]
    auto get_vertex_count() const -> member_type const& {
        return get_at<member_type>(0);
    }

    [[nodiscard]]
    auto get_vertex_data() const -> vertex const* {
        return reinterpret_cast<vertex const*>(m_data.data() +
                                               get_vertex_count());
    }

    [[nodiscard, gnu::noinline, gnu::used]]
    auto get_vertex(uint index) const -> vertex {
        return get_at<vertex>(vertices_offset + (index * sizeof(vertex)));
    }

    [[nodiscard]]
    auto get_index(uint index) const -> uint {
        return get_at<uint>(get_index_offset() + (index * sizeof(uint)));
    }

    // [[nodiscard]]
    // auto get_light(uint index) const -> light {
    //     return get_at<light>(get_lights_offset() + (index * sizeof(light)));
    // }

    //

    void set_index_count(member_type count) {
        set_at(count, member_stride);
    }

    [[nodiscard]]
    auto get_index_count() const -> member_type const& {
        return get_at<member_type>(member_stride);
    }

    void set_index_offset(member_type count) {
        set_at(count, member_stride * 2z);
    }

    [[nodiscard]]
    auto get_index_offset() const -> member_type const& {
        return get_at<member_type>(member_stride * 2z);
    }

    void set_material_count(member_type count) {
        set_at(count, member_stride * 3z);
    }

    [[nodiscard]]
    auto get_material_count() const -> member_type const& {
        return get_at<member_type>(member_stride * 3z);
    }

    void set_instance_commands_count(member_type count) {
        set_at(count, member_stride * 4z);
    }

    [[nodiscard]]
    auto get_instance_commands_count() const -> member_type const& {
        return get_at<member_type>(member_stride * 4z);
    }

    void set_instance_commands_offset(member_type offset) {
        set_at(offset, member_stride * 5z);
    }

    [[nodiscard]]
    auto get_instance_commands_offset() const -> member_type const& {
        return get_at<member_type>(member_stride * 5z);
    }

    void set_properties_offset(member_type offset) {
        set_at(offset, member_stride * 6z);
    }

    [[nodiscard]]
    auto get_properties_offset() const -> member_type const& {
        return get_at<member_type>(member_stride * 6z);
    }

    void set_lights_count(member_type count) {
        set_at(count, member_stride * 7z);
    }

    [[nodiscard]]
    auto get_lights_count() const -> member_type const& {
        return get_at<member_type>(member_stride * 7z);
    }

    void set_lights_offset(member_type offset) {
        set_at(offset, member_stride * 8z);
    }

    [[nodiscard]]
    auto get_lights_offset() const -> member_type const& {
        return get_at<member_type>(member_stride * 8z);
    }

    void set_textures_count(member_type count) {
        set_at(count, member_stride * 9z);
    }

    [[nodiscard]]
    auto get_textures_count() const -> member_type const& {
        return get_at<member_type>(member_stride * 9z);
    }

    void set_mirrors_count(member_type count) {
        set_at(count, member_stride * 10z);
    }

    [[nodiscard]]
    auto get_mirrors_count() const -> member_type const& {
        return get_at<member_type>(member_stride * 10z);
    }

    [[nodiscard]]
    auto get_mirrors_offset() const -> member_type {
        // The first 4 textures are hard-coded into the renderer, add the number
        // of mesh textures, and that is the beginning of the mirror textures.
        return 4 + get_textures_count();
    }

    void set_camera_position(glm::vec3 position) {
        set_at(position, cameras_offset - 16);
    }

    [[nodiscard]]
    auto get_camera_position() const -> glm::vec3 const& {
        return get_at<glm::vec3>(cameras_offset - 16);
    }

    // Camera getters/setters.
    void set_view_matrix(glm::mat4x4 matrix) {
        set_at(matrix, cameras_offset);
    }

    [[nodiscard]]
    auto get_view_matrix() const -> glm::mat4x4 const& {
        return get_at<glm::mat4x4>(cameras_offset);
    }

    void set_proj_matrix(glm::mat4x4 matrix) {
        set_at(matrix, cameras_offset + sizeof(glm::mat4x4));
    }

    [[nodiscard]]
    auto get_proj_matrix() const -> glm::mat4x4 const& {
        return get_at<glm::mat4x4>(cameras_offset + sizeof(glm::mat4x4));
    }

    void push_mesh(mesh const& mesh);

    void push_indices();

    void push_instances_of(std::size_t mesh_index,
                           std::span<mesh_instance const> instance);

    void push_properties();

    struct property {
        alignas(16) glm::vec3 position;
        alignas(16) glm::fquat rotation;
        alignas(16) glm::vec3 scaling;
        alignas(16) glm::vec4 color_blend;
        std::uint32_t id;
    };

  private:
    void add_vertex_count(member_type count) {
        set_vertex_count(get_vertex_count() + count);
    }

    void increment_instance_command_count() {
        set_instance_commands_count(get_instance_commands_count() + 1);
    }

    std::vector<std::byte> m_data;
    std::vector<index_type> m_indices;

    std::vector<property> m_instance_properties;

    unsigned m_instance_count;

    struct mesh_count {
        int vertex_offset;
        int index_offset;
    };

    std::vector<mesh_count> m_counts;
};

// Bindless storage buffer.
inline buffer_storage g_bindless_data;
