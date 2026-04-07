#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

#define Z_NEAR 0.001f
#define Z_FAR  100.0f

void Camera::update()
{
    float cx = radius * std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    float cy = radius * std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    float cz = radius * std::sin(glm::radians(pitch));
    position_ = target + glm::vec3(cx, cy, cz);

    view_ = glm::lookAt(position_, target, up);
    proj_ = glm::perspective(glm::radians(fovy), aspect_, Z_NEAR, Z_FAR);
    viewProj_ = proj_ * view_;

    glm::vec3 forward = glm::normalize(target - position_);
    right_ = glm::normalize(glm::cross(forward, up));
    camUp_ = glm::normalize(glm::cross(right_, forward));
}

void Camera::onMouseButton(int button, int action, float xpos, float ypos)
{
    const int PRESS = 1;
    if (button == 0) {
        isDragging_ = (action == PRESS);
        if (action == PRESS) { lastMouseX_ = xpos; lastMouseY_ = ypos; }
    }
    if (button == 2) {
        isPanning_ = (action == PRESS);
        if (action == PRESS) { lastMouseX_ = xpos; lastMouseY_ = ypos; }
    }
}

void Camera::onMouseMove(float xpos, float ypos)
{
    float dx = xpos - lastMouseX_;
    float dy = lastMouseY_ - ypos;
    lastMouseX_ = xpos;
    lastMouseY_ = ypos;

    if (isDragging_) {
        const float sensitivity = 0.5f;
        yaw += dx * sensitivity;
        pitch += dy * sensitivity;
        pitch = std::clamp(pitch, -89.0f, 89.0f);
    }

    if (isPanning_) {
        glm::vec3 forward;
        forward.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        forward.y = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        forward.z = std::sin(glm::radians(pitch));
        forward = glm::normalize(forward);

        glm::vec3 right = glm::normalize(glm::cross(forward, up));
        glm::vec3 panUp = glm::normalize(glm::cross(right, forward));

        float panSpeed = radius * 0.002f;
        target += (right * dx + panUp * dy) * panSpeed;
    }
}

void Camera::onScroll(float yoffset)
{
    radius -= yoffset * 0.2f;
    radius = std::clamp(radius, 0.5f, 20.0f);
}
