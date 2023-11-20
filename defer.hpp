#pragma once

#include <algorithm>

namespace std {
template <std::size_t S, typename... Ts>
struct aligned_union {
    static constexpr std::size_t alignment_value = std::max({alignof(Ts)...});

    struct type {
        alignas(alignment_value) char bytes[std::max({S, sizeof(Ts)...})];
    };
};

template <typename T>
using unaligned_storage [[gnu::aligned(alignof(T))]] = std::byte[sizeof(T)];
}  // namespace std

namespace detail {

template <typename F>
class scoped_callback {
    F callback;

  public:
    template <typename T>
    scoped_callback(T&& f) : callback(f) {
    }

    ~scoped_callback() {
        callback();
    }
};

inline constinit struct {
    template <typename F>
    scoped_callback<F> operator<<(F&& callback) {
        return scoped_callback<F>(callback);
    }
} deferrer;

}  // namespace detail

#define defer auto _ = detail::deferrer << [&]  // NOLINT
