#pragma once

#include <vulkan/vulkan.h>

class Device
{
public:

	Device();
	Device(VkPhysicalDevice physicalDevice);

	void initDevice(VkPhysicalDevice physicalDevice);
	
	operator VkDevice() const { return logicalDevice; }

	// Getters
	VkPhysicalDevice getPhysicaDevice() { return physicalDevice; }
	VkDevice getLogicalDevice() { return logicalDevice; }
	VkPhysicalDeviceProperties getProperties() { return properties; }
private:

	VkPhysicalDevice physicalDevice;
	VkDevice logicalDevice;
	VkPhysicalDeviceProperties properties;
};

