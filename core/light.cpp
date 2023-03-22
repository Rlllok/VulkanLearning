#include "light.h"

// std
#include <stdexcept>

#include "utils.hpp"

Light::Light(VkDevice device, VkPhysicalDevice physicalDevice, glm::vec3 position, glm::vec3 color)
{
	this->device = device;
	this->physicalDevice = physicalDevice;
	this->properties.position = position;
	this->properties.color = color;

	createBuffer();
}

Light::~Light()
{
	vkFreeMemory(device, bufferMemory, nullptr);
	vkDestroyBuffer(device, buffer, nullptr);
}

void Light::createBuffer()
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(Properties);
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Light Buffer.");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memRequirements.size;
	allocateInfo.memoryTypeIndex = findMemoryType(
		physicalDevice,
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	result = vkAllocateMemory(device, &allocateInfo, nullptr, &bufferMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate Light Buffer Memory.");
	}

	vkBindBufferMemory(device, buffer, bufferMemory, 0);

	void* data;
	vkMapMemory(device, bufferMemory, 0, bufferInfo.size, 0, &data);
		memcpy(data, &properties, sizeof(Properties));
	vkUnmapMemory(device, bufferMemory);
}
