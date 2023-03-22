#pragma once

// std
#include <array>

#include "volk.h"
#include <glm/glm.hpp>

class Light
{
public:

	struct Properties
	{
		alignas(0)	glm::vec3 position;
		alignas(16) glm::vec3 color;
	};
	
	Light(VkDevice device, VkPhysicalDevice physicalDevice, glm::vec3 position, glm::vec3 color);
	~Light();

	// Getters
	glm::vec3 getPostion() { return properties.position; }
	glm::vec3 getColor() { return properties.color; }
	VkBuffer getBuffer() { return buffer; }

private:

	Properties properties;
	
	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;

	void createBuffer();
};

