#pragma once

// Disable these warnings for Vookoo.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <vku/vku.hpp>
#pragma GCC diagnostic pop

#include <cassert>
#include <vector>

#include "defer.hpp"

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
    static constexpr unsigned char vertices_offset = 64;
    static constexpr unsigned char member_stride = 4;
    using member_type = unsigned int;

    buffer_storage() : m_data(2'048z) {
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
