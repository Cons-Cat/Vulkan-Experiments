#pragma once

#include <filesystem>

#ifdef __linux__
#include <string>
#include <unistd.h>

inline auto getexepath() -> std::filesystem::path {
    char result[PATH_MAX];
    size_t count =
        static_cast<std::size_t>(readlink("/proc/self/exe", result, PATH_MAX));
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
    F m_callback;

  public:
    template <typename T>
    scoped_callback(T&& f) : m_callback(f) {  // NOLINT
    }

    ~scoped_callback() {
        m_callback();
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

#define fwd(x) ::std::forward<decltype(x)>(x)
