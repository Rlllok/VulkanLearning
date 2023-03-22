#include "baseRenderer.h"

#include "errorLog.h"
#include "validation.h"

#include <volk.h>

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
BaseRenderer::BaseRenderer(struct android_app *app, bool isDebug) {
	this->app = app;
	this->isDebug = isDebug;
}
#else
BaseRenderer::BaseRenderer(bool isDebug)
{
	this->isDebug = isDebug;
}
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void BaseRenderer::initWindow(ANativeWindow* window)
{
	this->window = window;
}
#else
void BaseRenderer::initWindow(uint32_t width, uint32_t height, std::string title)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

    if (!window) {
        error::log("cannot create GLFW window.");
    }
}
#endif

BaseRenderer::~BaseRenderer()
{
	// for (auto* framebuffer : swapchainFramebuffers) {
	// 	vkDestroyFramebuffer(device->getLogicalDevice(), framebuffer, nullptr);
	// }

	vkDestroyRenderPass(device->getLogicalDevice(), renderPass, nullptr);

	delete swapchain;

	delete device;

	destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	
	vkDestroyInstance(instance, nullptr);
}

void BaseRenderer::initVulkan()
{
    VkResult result = volkInitialize();
	if (result != VK_SUCCESS) {
		error::log("cannot init volk.");
	}

	createInstance();
	if (isDebug) {
		createDebugMessenger(instance, &debugMessenger);
	}
	createDevice();
}

void BaseRenderer::prepareRenderer()
{
	createSwapchain();
	createRenderPass();
	// createSwapchainFramebuffers();
}

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void BaseRenderer::startRenderLoop(struct android_app *app)
{
	int events;
	android_poll_source *pSource;
	do {
		if (ALooper_pollAll(0, nullptr, &events, (void **) &pSource) >= 0) {
			if (pSource) {
				pSource->process(app, pSource);
			}
		}

		if (setupCompleted()) {
			draw();
		}
	} while (!app->destroyRequested);
}
#else
void BaseRenderer::startRenderLoop()
{
	while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
		draw();
    }

	vkDeviceWaitIdle(device->getLogicalDevice());
}
#endif

bool BaseRenderer::setupCompleted()
{
    return isSetupCompleted;
}

void BaseRenderer::createInstance()
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

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	std::vector<const char*> extensions;
	extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#else
    uint32_t extensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + extensionCount);
#endif
    if (isDebug) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instanceInfo.ppEnabledExtensionNames = extensions.data();

	VkResult result = vkCreateInstance(&instanceInfo, nullptr,&instance);
	if (result != VK_SUCCESS) {
		error::log("cannot create Vulkan Instance.");
	}

	volkLoadInstance(instance);
}

void BaseRenderer::createDevice()
{
    uint32_t physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

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

void BaseRenderer::createSwapchain()
{
    swapchain = new SwapChain(instance, device);
	swapchain->initSurface(window);
	swapchain->initSwapchain();
}

void BaseRenderer::createSwapchainFramebuffers()
{
    swapchainFramebuffers.resize(swapchain->getImageCount());

	std::vector<VkImageView> attachments = swapchain->getImageViews();

	for (int i = 0; i < swapchainFramebuffers.size(); i++) {
		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &attachments[i];

		VkExtent2D extent = swapchain->getExtent();
		framebufferInfo.width = extent.width;
		framebufferInfo.height = extent.height;
		framebufferInfo.layers = 1;

		VkResult result = vkCreateFramebuffer(device->getLogicalDevice(), &framebufferInfo, nullptr, &swapchainFramebuffers[i]);
		if (result != VK_SUCCESS) {
			error::log("cannot create Swapchain Framebuffer.");
		}
	}
}

void BaseRenderer::createRenderPass()
{
    VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapchain->getImageFormat();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorAttachmentRef;
	subpassDescription.pResolveAttachments = nullptr;
	subpassDescription.pDepthStencilAttachment = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = 0;
	renderPassInfo.pDependencies = nullptr;

	VkResult result = vkCreateRenderPass(device->getLogicalDevice(), &renderPassInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS) {
		error::log("cannot create RenderPass.");
	}
}
