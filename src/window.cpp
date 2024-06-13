#include "window.hpp"

#include "camera.hpp"

// NOLINTNEXTLINE I can't control this API.
void my_window::OnResizeEvent(uint16_t width, uint16_t height) {
}

constexpr float speed = 0.5f;

void my_window::OnKeyEvent(eAction action, eKeycode keycode) {
    if (action == eDOWN) {
        switch (keycode) {
            case KEY_Up:
                g_camera.position.z -= speed;
                break;
            case KEY_Down:
                g_camera.position.z += speed;
                break;
            case KEY_Left:
                g_camera.position.x -= speed;
                break;
            case KEY_Right:
                g_camera.position.x += speed;
                break;
            case KEY_PageUp:
                g_camera.position.y += speed;
                break;
            case KEY_PageDown:
                g_camera.position.y -= speed;
                break;
            case KEY_S:
                g_camera.yaw -= speed / 4;
                break;
            case KEY_D:
                g_camera.yaw += speed / 4;
                break;
            case KEY_Escape:
                Close();
                break;
        }
    }
}
