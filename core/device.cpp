#include "device.h"
#include "errorLog.h"
#include "validation.h"
#include "window.h"

#include <vector>

Device::Device(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, VkQueueFlags queueTypes)
{
	this->physicalDevice = physicalDevice;
	this->enabledFeatures = enabledFeatures;
	this->enabledExtensions = enabledExtensions;

	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
	
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	if (queueFamilyCount == 0)
	{
		error::log("Physical Device \"" + std::string(properties.deviceName) + "\" has no Queue Families.");
	}
	queueFamilyProperties.resize(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

	createLogicalDevice(queueTypes);
}

Device::~Device()
{
    clean();
}

void Device::init()
{
}

void Device::clean()
{
	vkDestroyDevice(logicalDevice, nullptr);
}

void Device::createLogicalDevice(VkQueueFlags queueTypes)
{
	std::vector<VkDeviceQueueCreateInfo> queueInfos;

	const float queuePriority = 1.0f;

	if ((queueTypes & VK_QUEUE_GRAPHICS_BIT) == queueTypes) {
		queueFamilyIndecies.graphicsFamily = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);

		VkDeviceQueueCreateInfo queueInfo = {};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = queueFamilyIndecies.graphicsFamily;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &queuePriority;
		queueInfos.push_back(queueInfo);
	}
	else {
		queueFamilyIndecies.graphicsFamily = 0;
	}

	VkDeviceCreateInfo deviceInfo = {};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
	deviceInfo.pQueueCreateInfos = queueInfos.data();
	deviceInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
	deviceInfo.ppEnabledExtensionNames = enabledExtensions.data();
	deviceInfo.pEnabledFeatures = &enabledFeatures;

	VkResult result = vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &logicalDevice);
	if (result != VK_SUCCESS) {
		error::log("cannot create Logical Device.");
	}
}

uint32_t Device::getQueueFamilyIndex(VkQueueFlags queueFlags)
{
	for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
		if ((queueFamilyProperties[i].queueFlags & queueFlags) == queueFlags) {
			return i;
		}
	}

    error::log("could not find Queue Family Index.");
	return 0;
}