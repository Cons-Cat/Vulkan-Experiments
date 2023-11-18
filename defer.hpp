#pragma once

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

#define defer auto _ = detail::deferrer << [&]
