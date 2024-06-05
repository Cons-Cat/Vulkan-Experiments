#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <WSIWindow.h>
#pragma GCC diagnostic pop

struct my_window final : public WSIWindow {
    // Override virtual functions.
    // NOLINTNEXTLINE I can't control this API.
    void OnResizeEvent(uint16_t width, uint16_t height) final;

    void OnKeyEvent(eAction action, eKeycode keycode) final;
};
