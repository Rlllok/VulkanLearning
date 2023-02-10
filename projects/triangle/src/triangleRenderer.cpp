#include "triangleRenderer.h"

#include "errorLog.h"
#include "validation.h"

TriangleRenderer::TriangleRenderer(bool isDebug)
{
    this->isDebug = isDebug;
}

void TriangleRenderer::start(uint32_t windowWidth, uint32_t windowHeight, std::string windowTitle)
{
    window = new Window(windowWidth, windowHeight, windowTitle);

	initVulkan();
	window->runLoop();
}

TriangleRenderer::~TriangleRenderer()
{
    delete window;
	delete device;
	delete swapchain;

	destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

	vkDestroyInstance(instance, nullptr);
}

void TriangleRenderer::initVulkan()
{
    createInstance();
	createDebugMessenger(instance, &debugMessenger);
	createDevice();
}

void TriangleRenderer::createInstance()
{
    if (isDebug && !checkValidationLayerSupport()) {
		error::log("validation layers requested, but not supported.");
	}

	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pApplicationName = "VulkanLearning";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.pEngineName = "NoEngine";
	applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo instanceInfo = {};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &applicationInfo;
	
	VkDebugUtilsMessengerCreateInfoEXT messegerInfo = {};
	if (isDebug) {
		instanceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		instanceInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(messegerInfo);
		instanceInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&messegerInfo;
	}
	else {
		instanceInfo.enabledLayerCount = 0;
		instanceInfo.ppEnabledLayerNames = VK_NULL_HANDLE;
	}

	std::vector<const char*> extensions = window->getRequiredExtensions();
    if (isDebug) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instanceInfo.ppEnabledExtensionNames = extensions.data();

	VkResult result = vkCreateInstance(&instanceInfo, nullptr,&instance);
	if (result != VK_SUCCESS) {
		error::log("cannot create Vulkan Instance.");
	}
}

void TriangleRenderer::createDevice()
{
	uint32_t physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

	VkPhysicalDevice physicalDevice;

	for (const auto& currentDevice : physicalDevices) {
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(currentDevice, &properties);

		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			physicalDevice = currentDevice;
			break;
		}
	}

	if(physicalDevice == VK_NULL_HANDLE)
	{
		physicalDevice = physicalDevices[0];
	}

	VkPhysicalDeviceFeatures enabledFeatures = {};

	std::vector<const char*> enabledExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	device = new Device(physicalDevice, enabledFeatures, enabledExtensions, VK_QUEUE_GRAPHICS_BIT);
}

void TriangleRenderer::createSwapchain()
{
	swapchain = new SwapChain(instance, device);
	swapchain->initSurface(window);
	swapchain->initSwapchain();
}

void TriangleRenderer::createRenderPass()
{
}
