#pragma once

// Disable these warnings for Vookoo.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <vku/vku.hpp>
#pragma GCC diagnostic pop

#include <glm/mat4x4.hpp>

#include <cassert>
#include <vector>

#include "defer.hpp"

struct alignas(16) vertex {
    constexpr vertex() = default;

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

    buffer_storage() : m_data(2'048z) {
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

    void set_instance_count(member_type count) {
        set_at(count, member_stride * 4z);
    }

    [[nodiscard]]
    auto get_instance_count() const -> member_type const& {
        return get_at<member_type>(member_stride * 4z);
    }

    void set_texture_count(member_type count) {
        set_at(count, member_stride * 5z);
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

  private:
    void add_vertex_count(member_type count) {
        set_vertex_count(get_vertex_count() + count);
    }

    std::vector<std::byte> m_data;
    std::vector<index_type> m_indices;
};

inline vku::GenericBuffer g_buffer;

// Bindless storage buffer.
inline buffer_storage g_bindless_data;
