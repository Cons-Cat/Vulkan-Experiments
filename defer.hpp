#pragma once

#include <algorithm>
#include <filesystem>

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

#ifdef __linux__
#include <climits>
#include <string>
#include <unistd.h>

inline auto getexepath() -> std::filesystem::path {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return std::string(result, (count > 0) ? count : 0);
}
#elif _WIN32
#include <string>
#include <windows.h>

inline auto getexepath() -> std::filesystem::path {
    char result[MAX_PATH];
    return std::string(result, GetModuleFileName(NULL, result, MAX_PATH));
}
#endif

namespace detail {

template <typename F>
class scoped_callback {
    F callback;

  public:
    template <typename T>
    scoped_callback(T&& f) : callback(f) {  // NOLINT
    }

    ~scoped_callback() {
        callback();
    }
};

inline constinit struct {
    template <typename F>
    auto operator<<(F&& callback) -> scoped_callback<F> {
        return scoped_callback<F>(callback);
    }
} deferrer;

}  // namespace detail

#define defer auto _ = detail::deferrer << [&]  // NOLINT
