#include "Device.h"

Device::Device()
{

}

Device::Device(VkPhysicalDevice physicalDevice)
{
	initDevice(physicalDevice);
}

void Device::initDevice(VkPhysicalDevice physicalDevice)
{
	this->physicalDevice = physicalDevice;

	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

	VkDeviceCreateInfo deviceInfo = {};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.queueCreateInfoCount = 0;
	deviceInfo.pQueueCreateInfos = nullptr;
	deviceInfo.enabledExtensionCount = 0;
	deviceInfo.ppEnabledExtensionNames = nullptr;
	deviceInfo.pEnabledFeatures = nullptr;
}
