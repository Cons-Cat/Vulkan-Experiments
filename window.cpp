#include "window.hpp"

// NOLINTNEXTLINE I can't control this API.
void my_window::OnResizeEvent(uint16_t width, uint16_t height) {
}

void my_window::OnKeyEvent(eAction action, eKeycode keycode) {
    if (action == eDOWN) {
        switch (keycode) {
            case KEY_Escape:
                Close();
                break;
        }
    }
}
