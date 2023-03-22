#include "camera.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(glm::vec3 positionVector, glm::vec3 worldUpVector, float FOV, float aspectRatio, float sensetivity)
{
    this->positionVector = positionVector;
    this->worldUpVector = worldUpVector;
    this->FOV = FOV;
    this->aspectRatio = aspectRatio;
    this->sensetivity = sensetivity;

    yaw = 90.0f;
    pitch = 0.0f;
    
    updateVectors();
}

glm::mat4 Camera::getViewMatrix()
{
    return glm::lookAt(positionVector, positionVector + frontVector, upVector);
}

void Camera::move(CAMERA_MOVEMENT direction, float deltaTime)
{
    float velocity = 3.0f * deltaTime;
    
    if (direction == FORWARD) {
        positionVector += frontVector * velocity;
    }
    else if (direction == BACKWARD) {
        positionVector -= frontVector * velocity;
    }
    else if (direction == RIGHT) {
        positionVector += rightVector * velocity;
    }
    else if (direction == LEFT) {
        positionVector -= rightVector * velocity;
    }
}

void Camera::rotateByMouse(float xOffset, float yOffset)
{
    yaw += xOffset * sensetivity;
    pitch += yOffset * sensetivity;

    if (pitch > 89.0) {
        pitch = 89.0;
    }
    if (pitch < -89.0) {
        pitch = -89.0;
    }

    updateVectors();
}

void Camera::updateVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    frontVector = glm::normalize(front);

    rightVector = glm::cross(frontVector, worldUpVector);
    rightVector = glm::normalize(rightVector);

    upVector = glm::cross(rightVector, frontVector);
    upVector = glm::normalize(upVector);
}
