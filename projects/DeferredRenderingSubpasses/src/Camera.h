#pragma once

#define GLM_FORCE_RADIANCE
#include <glm/glm.hpp>

class Camera
{
public:

	enum CAMERA_MOVEMENT {
		FORWARD,
		BACKWARD,
		RIGHT,
		LEFT
	};

	Camera(
		glm::vec3 positionVector,
		glm::vec3 worldUpVector,
		float FOV,
		float aspectRatio,
		float sensetivity = 0.1f
	);

	float getFOV() { return FOV; }
	float getAspectRatio() { return aspectRatio; }
	glm::vec3 getPosition() { return positionVector; }
	
	glm::mat4 getViewMatrix();
	void move(CAMERA_MOVEMENT direction, float deltaTime);
	void rotateByMouse(float xOffset, float yOffset);

private:

	glm::vec3 positionVector;
	glm::vec3 frontVector;
	glm::vec3 rightVector;
	glm::vec3 upVector;
	glm::vec3 worldUpVector;

	float FOV;
	float yaw;
	float pitch;
	float aspectRatio;
	float sensetivity;

	void updateVectors();
};

