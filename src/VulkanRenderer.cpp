#include "VulkanRenderer.h"

// std
#include <stdexcept>
#include <iostream>
#include <limits>
#include <algorithm>
#include <set>
#include <chrono>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Utils.hpp"
#include "Shader.h"
#include "Model.h"

constexpr auto MSSA_SAMPLES = VK_SAMPLE_COUNT_4_BIT;

VulkanRenderer::VulkanRenderer(bool enableValidationLayers)
{
	this->enableValidationLayers = enableValidationLayers;
}

void VulkanRenderer::start(int windowWidth, int windowHeight, const char* windowTitle)
{
	initWindow(windowWidth, windowHeight, windowTitle);
	camera = new Camera(
		glm::vec3(0.0f, 0.0f, -5.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		45.0f,
		windowHeight / float(windowWidth),
		0.1f
	);
	initVulkan();
	loop();
	cleanup();
}

void VulkanRenderer::initVulkan()
{
	createInstance();
	setupDebugMessenger();
	createSurface();
	choosePhysicalDevice();
	createLogicalDevice();
	createSwapchain();
	createSwapchainImageViews();
	createRenderPass();
	createColorResources();
	createDepthResources();
	createSwapchainFramebuffers();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createCommandPool();
	createCommandBuffers();

	light = new Light(
		device,
		device.physicalDevice,
		glm::vec3(1.0f, 1.0f, -3.0f),
		glm::vec3(1.0f)
	);

	createModel("models/head.obj", "textures/head.tga");
	createMVPBuffer();
	createDescriptorPool();
	createDescriptorSet();
	createSyncTools();
}

void VulkanRenderer::initWindow(int windowWidth, int windowHeight, const char* windowTitle)
{
	glfwInit();

	// GLFW was initialy created for OpenGL. So we should say GLFW that we don't want to use API
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(windowWidth, windowHeight, windowTitle, nullptr, nullptr);

	if (!window) {
		throw std::runtime_error("ERROR: cannot create Window.");
	}

	glfwSetWindowUserPointer(window, this);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, mouseCallback);
}

void VulkanRenderer::loop()
{
	auto startTime = std::chrono::high_resolution_clock::now();
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		
		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		startTime = std::chrono::high_resolution_clock::now();
		processInput(deltaTime);

		draw();
	}

	// Wait while device do nothing ( queues are empty, etc ).
	// After that we can end loop and proceed to destroing created objects.
	vkDeviceWaitIdle(device);
}

void VulkanRenderer::processInput(float deltaTime)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);
	}
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		camera->move(Camera::FORWARD, deltaTime);
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		camera->move(Camera::BACKWARD, deltaTime);
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		camera->move(Camera::RIGHT, deltaTime);
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		camera->move(Camera::LEFT, deltaTime);
	}
}

void VulkanRenderer::mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
	VulkanRenderer* obj = (VulkanRenderer*)glfwGetWindowUserPointer(window);

	static bool firstMouse = true;
	static float lastX;
	static float lastY;

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;
	lastX = xpos;
	lastY = ypos;

	obj->camera->rotateByMouse(xoffset, yoffset);
}

void VulkanRenderer::createInstance()
{
	/*
		Instance is a connection point between an app and the vulkan library.
		All states that specify an application are stored in Instance.

		We have to specify information about our app, layers and extensions that we want to be used by instance.

		Layers are additional steps that addad to the call chain for Vulkan commands.
		Thay are helpful for logging, validation, etc.

		Extension defines new commands, structure, etc.
	*/
	if (enableValidationLayers && !checkValidationLayerSupport()) {
		throw std::runtime_error("ERROR: validation layers requested, but not supported.");
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
	
	// If validation is enable than we add validation layer to instace
	// and create Debug Messenger
	VkDebugUtilsMessengerCreateInfoEXT messegerInfo = {};
	if (enableValidationLayers) {
		instanceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		instanceInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(messegerInfo);
		instanceInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&messegerInfo;
	}
	else {
		instanceInfo.enabledLayerCount = 0;
		instanceInfo.ppEnabledLayerNames = VK_NULL_HANDLE;
	}

	// get extensions required to work with GLFW and for validation
	std::vector<const char*> extensions = getRequiredExtensions();
	instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instanceInfo.ppEnabledExtensionNames = extensions.data();

	result = vkCreateInstance(&instanceInfo, nullptr,&instance);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Vulkan Instance.");
	}
}

void VulkanRenderer::setupDebugMessenger()
{
	/*
		By default Vulkan have no debug information. It is optimization decision.
		But we can on such features by adding validation layers to out instance.
		Also we can modify what validation layers prints. To do this, we can create custom
		Debug Messenger and setup settings that we want.
	*/
	if (!enableValidationLayers) { return; }

	VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {};
	populateDebugMessengerCreateInfo(messengerInfo);

	result = createDebugUtilsMessengerEXT(instance, &messengerInfo, nullptr, &debugMessenger);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Debug Messenger.");
	}
}

void VulkanRenderer::createLogicalDevice()
{
	/*
		Logical Device is an interface to the Physical Device.
	
		There we describe what queues our device will use, device level extensions and features.
		( Features like geometry shader or tesselation shader )
	*/

	std::set<uint32_t> uniqueQueueIndecis = {
		queues.graphicsQueueIndex.value(),
		queues.presentQueueIndex.value(),
	};

	std::vector<VkDeviceQueueCreateInfo> queueInfos;

	float queuePriority = 1.0f;
	for (const auto& queueIndex : uniqueQueueIndecis) {
		VkDeviceQueueCreateInfo queueInfo = {};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = queueIndex;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &queuePriority;
		queueInfos.push_back(queueInfo);
	}

	VkPhysicalDeviceFeatures deviceFeature = {};
	deviceFeature.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo deviceInfo = {};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
	deviceInfo.pQueueCreateInfos = queueInfos.data();
	deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceInfo.pEnabledFeatures = &deviceFeature;

	result = vkCreateDevice(device.physicalDevice, &deviceInfo, nullptr, &device.logicalDevice);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Logical Device.");
	}

	vkGetDeviceQueue(device, queues.graphicsQueueIndex.value(), 0, &queues.graphicsQueue);
	vkGetDeviceQueue(device, queues.presentQueueIndex.value(), 0, &queues.presentQueue);
}

void VulkanRenderer::createSurface()
{
	/*
		The Surface is an object that represents where we want to represent rendered image.

		We can create surface using commands like vkCreateWin32SurfaceKHR or  vkCreateAndroidSurfaceKHR.
		But GLFW can handle it by itself. glfwCreateWindowSurface creates surface that represent window.
	*/
	result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Surface.");
	}
}

void VulkanRenderer::createSwapchain()
{
	/*
		Swapchain define how images that we present to surface are handeled.
		It is like a array of images that are waiting to be presented to the screnn.
		And swapchain parameters change how such images are trited.

		In parameters of swapchain we define format of images, surface to present on,
		present mode, etc.
	*/
	SwapchainSupportDetails swapchainSupportDetails = getSwapchainSupportDetails(device.physicalDevice);

	VkExtent2D extent = chooseSwapchainExtent(swapchainSupportDetails.capabilities);
	VkSurfaceFormatKHR surfaceFormat = chooseSwapchainSurfaceFormat(swapchainSupportDetails.surfaceFormats);
	//VkPresentModeKHR presentMode = chooseSwapchainPresentMode(swapchainSupportDetails.presentModes);
	
	// Changed to VK_PRESENT_MODE_FIFO_KHR, because it has no input latency. q
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

	VkSwapchainCreateInfoKHR swapchainInfo = {};
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = surface;

	uint32_t imageCount = swapchainSupportDetails.capabilities.minImageCount + 1;
	if (swapchainSupportDetails.capabilities.maxImageCount > 0
		&& imageCount < swapchainSupportDetails.capabilities.maxImageCount) {

		imageCount = swapchainSupportDetails.capabilities.maxImageCount;
	}
	swapchainInfo.minImageCount = imageCount;

	swapchainInfo.imageFormat = surfaceFormat.format;
	swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainInfo.imageExtent = extent;
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t queueIndices[] = { queues.graphicsQueueIndex.value() , queues.presentQueueIndex.value() };

	if (queues.graphicsQueueIndex != queues.presentQueueIndex) {
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainInfo.queueFamilyIndexCount = 2;
		swapchainInfo.pQueueFamilyIndices = queueIndices;
	}
	else {
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainInfo.queueFamilyIndexCount = 0;
		swapchainInfo.pQueueFamilyIndices = nullptr;
	}

	swapchainInfo.preTransform = swapchainSupportDetails.capabilities.currentTransform;
	swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.presentMode = presentMode;
	swapchainInfo.clipped = VK_TRUE;
	swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

	result = vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Swapchain.");
	}

	uint32_t swapchainImageCount;
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);

	swapchainImages.resize(swapchainImageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

	swapchainImageFormat = surfaceFormat.format;
	swapchainExtent = extent;
}

void VulkanRenderer::createSwapchainImageViews()
{
	/*
		Image View is an discription of an Image. Image view contain
		information about Image, such as type (1D, 2D, 3D),
		format of the Image, aspect (Color or Depth), etc

		So we have to create Image View for every Swapchain`s Image.
	*/
	swapchainImageViews.resize(swapchainImages.size());

	for (size_t i = 0; i < swapchainImageViews.size(); i++) {
		VkImageViewCreateInfo imageViewInfo = {};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = swapchainImages[i];
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = swapchainImageFormat;
		imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;

		result = vkCreateImageView(device, &imageViewInfo, nullptr, &swapchainImageViews[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: cannot create Swapchain Image Views.");
		}
	}
}

void VulkanRenderer::createColorResources()
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = swapchainImageFormat;
	imageInfo.extent.height = swapchainExtent.height;
	imageInfo.extent.width = swapchainExtent.width;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = MSSA_SAMPLES;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	result = vkCreateImage(device, &imageInfo, nullptr, &colorImage);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Color Image.");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetImageMemoryRequirements(device, colorImage, &memRequirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memRequirements.size;
	allocateInfo.memoryTypeIndex = findMemoryType(
		device.physicalDevice,
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	result = vkAllocateMemory(device, &allocateInfo, nullptr, &colorImageMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate Color Image Memory.");
	}

	vkBindImageMemory(device, colorImage, colorImageMemory, 0);

	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.image = colorImage;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = swapchainImageFormat;
	imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewInfo.subresourceRange.layerCount = 1;
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.baseMipLevel = 0;

	result = vkCreateImageView(device, &imageViewInfo, nullptr, &colorImageView);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Color Image View.");
	}
}

void VulkanRenderer::createDepthResources()
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_D32_SFLOAT;
	imageInfo.extent.width = swapchainExtent.width;
	imageInfo.extent.height = swapchainExtent.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = MSSA_SAMPLES;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	result = vkCreateImage(device, &imageInfo, nullptr, &depthImage);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Depth Image.");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetImageMemoryRequirements(device, depthImage, &memRequirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memRequirements.size;
	allocateInfo.memoryTypeIndex = findMemoryType(
		device.physicalDevice,
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	result = vkAllocateMemory(device, &allocateInfo, nullptr, &depthImageMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate Depth Image Memory.");
	}

	vkBindImageMemory(device, depthImage, depthImageMemory, 0);

	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.image = depthImage;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = VK_FORMAT_D32_SFLOAT;
	imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	imageViewInfo.subresourceRange.layerCount = 1;
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.baseMipLevel = 0;

	result = vkCreateImageView(device, &imageViewInfo, nullptr, &depthImageView);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Depth Image View.");
	}
}

void VulkanRenderer::createRenderPass()
{
	/*
		Render Pass is where rendering commands are executed.
		
		In Render Pass object we describe attachments used by this render pass,
		the way attachments are used and for what they are used.

		Render Pass can have multiple subpasses. 
		Subpass is an way to get access to the result of previous rendering.
		The limitation is that subpass can only access one pixel per time.
	*/
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapchainImageFormat;
	colorAttachment.samples = MSSA_SAMPLES;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = VK_FORMAT_D32_SFLOAT;
	depthAttachment.samples = MSSA_SAMPLES;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription colorAttachmentResolve = {};
	colorAttachmentResolve.format = swapchainImageFormat;
	colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
	colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorResolveReference = {};
	colorResolveReference.attachment = 2;
	colorResolveReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pResolveAttachments = &colorResolveReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;

	VkSubpassDependency subpassDependency = {};
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency.dstSubpass = 0;
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	subpassDependency.srcAccessMask = 0;
	subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &subpassDependency;

	result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Render Pass.");
	}
}

void VulkanRenderer::createSwapchainFramebuffers()
{
	/*
		Framebuffer represent memory of attachment used in Render Pass.
	*/
	swapchainFramebuffer.resize(swapchainImageViews.size());

	for (size_t i = 0; i < swapchainFramebuffer.size(); i++) {
		std::array<VkImageView, 3> attachments = {
			colorImageView,
			depthImageView,
			swapchainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapchainExtent.width;
		framebufferInfo.height = swapchainExtent.height;
		framebufferInfo.layers = 1;

		result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffer[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: cannot create Swapchain Framebuffer.");
		}
	}
}

void VulkanRenderer::createGraphicsPipeline()
{
	/*
		Graphics Pipeline is an list of stages required to render image.

		There we describe different stages like shaders, restarization, depth, etc.
	*/

	// SHADERS
	Shader vertShader(device, "shaders/triangle_vert.spv");
	Shader fragShader(device, "shaders/triangle_frag.spv");

	VkPipelineShaderStageCreateInfo vertStageInfo = {};
	vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStageInfo.module = vertShader.getShaderModule();
	vertStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragStageInfo = {};
	fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStageInfo.module = fragShader.getShaderModule();
	fragStageInfo.pName = "main";

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {
		vertStageInfo,
		fragStageInfo,
	};

	// VERTEX INPUT STATE
	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	// INPUT ASSEMBLY STATE
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

	// VIEWPORT STATE
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = static_cast<float>(swapchainExtent.width);
	viewport.height = static_cast<float>(swapchainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.extent = swapchainExtent;
	scissor.offset.x = 0;
	scissor.offset.y = 0;

	VkPipelineViewportStateCreateInfo viewportInfo = {};
	viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportInfo.viewportCount = 1;
	viewportInfo.pViewports = &viewport;
	viewportInfo.scissorCount = 1;
	viewportInfo.pScissors = &scissor;

	// RASTERIZATION STATE
	VkPipelineRasterizationStateCreateInfo rasterizationInfo = {};
	rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationInfo.depthClampEnable = VK_FALSE;
	rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationInfo.depthBiasEnable = VK_FALSE;
	rasterizationInfo.lineWidth = 1.0f;

	// MULTISAMPLE STATE
	VkPipelineMultisampleStateCreateInfo multisampleInfo = {};
	multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleInfo.sampleShadingEnable = VK_FALSE;
	multisampleInfo.rasterizationSamples = MSSA_SAMPLES;

	// DEPTH STENCIL STATE
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};
	depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilInfo.depthTestEnable = VK_TRUE;
	depthStencilInfo.depthWriteEnable = VK_TRUE;
	depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilInfo.stencilTestEnable = VK_FALSE;

	// COLOR BLEND STATE
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendInfo = {};
	colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendInfo.logicOpEnable = VK_FALSE;
	colorBlendInfo.attachmentCount = 1;
	colorBlendInfo.pAttachments = &colorBlendAttachment;

	// PIPELINE LAYOUT
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Pipeline Layout.");
	}

	// GRAPHICS PIPELINE
	VkGraphicsPipelineCreateInfo graphicsPipelineInfo = {};
	graphicsPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	graphicsPipelineInfo.pStages = shaderStages.data();
	graphicsPipelineInfo.pVertexInputState = &vertexInputInfo;
	graphicsPipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
	graphicsPipelineInfo.pTessellationState = nullptr;
	graphicsPipelineInfo.pViewportState = &viewportInfo;
	graphicsPipelineInfo.pRasterizationState = &rasterizationInfo;
	graphicsPipelineInfo.pMultisampleState = &multisampleInfo;
	graphicsPipelineInfo.pDepthStencilState = &depthStencilInfo;
	graphicsPipelineInfo.pColorBlendState = &colorBlendInfo;
	graphicsPipelineInfo.pDynamicState = nullptr;
	graphicsPipelineInfo.layout = pipelineLayout;
	graphicsPipelineInfo.subpass = 0;
	graphicsPipelineInfo.renderPass = renderPass;
	graphicsPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineInfo.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Graphics Pipeline.");
	}
}

void VulkanRenderer::createCommandPool()
{
	/*
		Command Buffer's memory allocated from Command Pool.
	*/
	VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = queues.graphicsQueueIndex.value();

	result = vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool);
	if (result = VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Command Buffer");
	}
}

void VulkanRenderer::createCommandBuffers()
{
	/*
		Command Buffer are object where command are recorded.

		Command Buffer can be submited to an Queue to execut recorded commands.
	*/
	VkCommandBufferAllocateInfo commandBufferInfo = {};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferInfo.commandPool = commandPool;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferInfo.commandBufferCount = 1;

	result = vkAllocateCommandBuffers(device, &commandBufferInfo, &commandBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate CommandBuffer.");
	}
}

void VulkanRenderer::recordCommandBuffer(uint32_t imageIndex)
{
	/*
		There command are recorded to Command Buffer. Such as begining of render pass
		(required if we want render something), pipeline binding, draw commands, etc.
	*/
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBeginInfo.flags = 0;
	bufferBeginInfo.pInheritanceInfo = nullptr;

	result = vkBeginCommandBuffer(commandBuffer, &bufferBeginInfo);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot begin Command Buffer recording.");
	}

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = swapchainFramebuffer[imageIndex];
	renderPassBeginInfo.renderArea.extent = swapchainExtent;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;

	VkClearValue colorClear = {};
	colorClear.color = { 0.01f, 0.01f, 0.01f, 1.0f };

	VkClearValue depthClear = {};
	depthClear.depthStencil = { 1.0, 0 };

	std::array<VkClearValue, 2> clearValues = { colorClear, depthClear };

	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		VkBuffer vertexBuffers[] = {model->getVertexBuffer()};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

		VkBuffer indexBuffer = model->getIndexBuffer();
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0,
			1,
			&descriptorSet,
			0,
			nullptr
		);

		uint32_t indexCount = model->getIndexCount();
		vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

	vkCmdEndRenderPass(commandBuffer);

	result = vkEndCommandBuffer(commandBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot end Command Buffer recording.");
	}
}

void VulkanRenderer::createSyncTools()
{
	/*
		Vulkan have synchronization tools such as Semaphore and Fence.

		Semaphore is an tool used to synchronize GPU - GPU operations.
		Controls access resources across queues.

		Fence is an tool used to syncronize CPU and GPU.
		The way to communicate to the host (CPU).
	*/
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Image Available Semaphore.");
	}

	result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Render Finished Semaphore.");
	}

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	result = vkCreateFence(device, &fenceInfo, nullptr, &bufferFence);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Buffer Fence.");
	}
}

void VulkanRenderer::createMVPBuffer()
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.size = sizeof(MVP);
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	result = vkCreateBuffer(device, &bufferInfo, nullptr, &mvpBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create MVP buffer.");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetBufferMemoryRequirements(device, mvpBuffer, &memRequirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memRequirements.size;
	allocateInfo.memoryTypeIndex = findMemoryType(
		device.physicalDevice,
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	result = vkAllocateMemory(device, &allocateInfo, nullptr, &mvpBufferMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate MVP Buffer Memory.");
	}

	vkBindBufferMemory(device, mvpBuffer, mvpBufferMemory, 0);

	vkMapMemory(device, mvpBufferMemory, 0, sizeof(MVP), 0, &mvpBufferMapped);
}

void VulkanRenderer::updateMVPBuffer()
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
	
	mvp.model = glm::mat4(1.0f);
	mvp.model = glm::rotate(mvp.model, deltaTime * glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	mvp.view = camera->getViewMatrix();
	mvp.projection = glm::perspective(glm::radians(camera->getFOV()), swapchainExtent.width / (float)swapchainExtent.height, 0.1f, 10.f);
	mvp.projection[1][1] *= -1;

	memcpy(mvpBufferMapped, &mvp, sizeof(MVP));
}

void VulkanRenderer::createDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding mvpLayoutBinding = {};
	mvpLayoutBinding.binding = 0;
	mvpLayoutBinding.descriptorCount = 1;
	mvpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	mvpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	mvpLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding textureLayoutBinding = {};
	textureLayoutBinding.binding = 1;
	textureLayoutBinding.descriptorCount = 1;
	textureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	textureLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding lightLayoutBinding = {};
	lightLayoutBinding.binding = 2;
	lightLayoutBinding.descriptorCount = 1;
	lightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	lightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lightLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 3> bindings = { mvpLayoutBinding, textureLayoutBinding, lightLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Descriptor Set Layout.");
	}
}

void VulkanRenderer::createDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 3> poolSizes = {};
	poolSizes[0].descriptorCount = 1;
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[2].descriptorCount = 1;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.maxSets = 1;
	descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descriptorPoolInfo.pPoolSizes = poolSizes.data();

	result = vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Descriptor Pool.");
	}
}

void VulkanRenderer::createDescriptorSet()
{
	VkDescriptorSetAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.descriptorSetCount = 1;
	allocateInfo.pSetLayouts = &descriptorSetLayout;

	result = vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate Descriptor Set.");
	}

	VkDescriptorBufferInfo descriptorBufferInfo = {};
	descriptorBufferInfo.buffer = mvpBuffer;
	descriptorBufferInfo.offset = 0;
	descriptorBufferInfo.range = sizeof(MVP);

	VkDescriptorImageInfo descriptorImageInfo = {};
	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptorImageInfo.imageView = texture->getImageView();
	descriptorImageInfo.sampler = texture->getSampler();

	VkDescriptorBufferInfo lightDescriptorInfo = {};
	lightDescriptorInfo.buffer = light->getBuffer();
	lightDescriptorInfo.offset = 0;
	lightDescriptorInfo.range = sizeof(Light::Properties);

	std::array<VkWriteDescriptorSet, 3> writeSets = {};

	writeSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSets[0].dstSet = descriptorSet;
	writeSets[0].dstBinding = 0;
	writeSets[0].dstArrayElement = 0;
	writeSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeSets[0].descriptorCount = 1;
	writeSets[0].pBufferInfo = &descriptorBufferInfo;

	writeSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSets[1].dstSet = descriptorSet;
	writeSets[1].dstBinding = 1;
	writeSets[1].dstArrayElement = 0;
	writeSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSets[1].descriptorCount = 1;
	writeSets[1].pImageInfo = &descriptorImageInfo;

	writeSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSets[2].dstSet = descriptorSet;
	writeSets[2].dstBinding = 2;
	writeSets[2].dstArrayElement = 0;
	writeSets[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeSets[2].descriptorCount = 1;
	writeSets[2].pBufferInfo = &lightDescriptorInfo;

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

void VulkanRenderer::choosePhysicalDevice()
{
	/*
	Physical devices represent GPUs installed in the system.
	We can obtain properties of GPU and choose what device we want to use.
	*/
	uint32_t physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

	for (const auto& physicalDevice : physicalDevices) {
		if (isPhysicalDeviceSuitable(physicalDevice)) {
			device.physicalDevice = physicalDevice;
			break;
		}
	}

	if (device.physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("ERROR: cannot find suitable Physical Device.");
	}
}

void VulkanRenderer::cleanup()
{
	/*
		We have to destroy every object that we created using Vulkan (free memory).
	*/
	delete light;
	delete camera;
	delete texture;
	delete model;

	vkDestroyImageView(device, depthImageView, nullptr);
	vkFreeMemory(device, depthImageMemory, nullptr);
	vkDestroyImage(device, depthImage, nullptr);

	vkDestroyImageView(device, colorImageView, nullptr);
	vkFreeMemory(device, colorImageMemory, nullptr);
	vkDestroyImage(device, colorImage, nullptr);

	vkFreeMemory(device, mvpBufferMemory, nullptr);
	vkDestroyBuffer(device, mvpBuffer, nullptr);

	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

	vkDestroyFence(device, bufferFence, nullptr);
	vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

	vkDestroyCommandPool(device, commandPool, nullptr);

	vkDestroyPipeline(device, graphicsPipeline, nullptr);

	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

	for (const auto& framebuffer : swapchainFramebuffer) {
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}

	vkDestroyRenderPass(device, renderPass, nullptr);

	for (const auto& imageView : swapchainImageViews) {
		vkDestroyImageView(device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);

	vkDestroySurfaceKHR(instance, surface, nullptr);

	vkDestroyDevice(device, nullptr);

	if (enableValidationLayers) {
		destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}

	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);

	glfwTerminate();
}

void VulkanRenderer::draw()
{
	/*
		This is where rendering and presentation is going.

		Fence is used to be sure that Command Buffer submited to Queue is executed.

		Semaphores is used to be sure that image is rendered and can be presented to an surface.
		Or image is available to start rendering.
	*/

	// waits while fence will be in state Singaled
	vkWaitForFences(device, 1, &bufferFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
	// reset Fence to Unsignaled state
	vkResetFences(device, 1, &bufferFence);

	uint32_t imageIndex;
	// Acquire next image and signals that image is available (change imageAvailableSemaphore).
	vkAcquireNextImageKHR(device, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

	updateMVPBuffer();

	vkResetCommandBuffer(commandBuffer, 0);
	recordCommandBuffer(imageIndex);

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

	// Fence become Signaled
	result = vkQueueSubmit(queues.graphicsQueue, 1, &submitInfo, bufferFence);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot submit Graphics Queue.");
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(queues.presentQueue, &presentInfo);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot Present Image.");
	}
}

void VulkanRenderer::createModel(std::string modelPath, std::string texturePath)
{
	model = new Model(modelPath, texturePath, device, device.physicalDevice);
	texture = new Texture(texturePath, device, device.physicalDevice, commandPool, queues.graphicsQueueIndex.value(), queues.graphicsQueue);
}

std::vector<const char*> VulkanRenderer::getRequiredExtensions()
{
	uint32_t extensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + extensionCount);

	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

bool VulkanRenderer::isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	
	findQueueFamilyIndeces(physicalDevice);

	bool extensionSupported = isDeviceSupportExtensions(physicalDevice);

	bool swapchainAdequate = false;
	if (extensionSupported) {
		SwapchainSupportDetails swapchainDetails = getSwapchainSupportDetails(physicalDevice);

		swapchainAdequate = !swapchainDetails.surfaceFormats.empty() && !swapchainDetails.presentModes.empty();
	}

	return properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
		&& queues.isComplete()
		&& extensionSupported
		&& swapchainAdequate;
}

void VulkanRenderer::findQueueFamilyIndeces(VkPhysicalDevice physicalDevice)
{
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

	for (int i = 0; i < queueFamilyProperties.size(); i++) {
		if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			queues.graphicsQueueIndex = i;
		}

		VkBool32 presentSupported = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupported);

		if (presentSupported) {
			queues.presentQueueIndex = i;
		}

		if (queues.isComplete()) {
			break;
		}
	}
}

bool VulkanRenderer::checkValidationLayerSupport()
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> layers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

	for (const char* layerName : validationLayers) {
		bool isLayerFound = false;

		for (const auto& layerProperties : layers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				isLayerFound = true;
				break;
			}
		}

		if (!isLayerFound) {
			return false;
		}
	}

	return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallackData, void* pUserData)
{
	std::cerr << "VL >> " << pCallackData->pMessage << std::endl;

	return VK_FALSE;
}

void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& messengerInfo)
{
	messengerInfo = {};
	messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
	messengerInfo.pfnUserCallback = debugCallback;
	messengerInfo.pUserData = nullptr;
}

bool VulkanRenderer::isDeviceSupportExtensions(VkPhysicalDevice physicalDevice)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	for (const auto& requiredExtensionName : deviceExtensions) {
		bool isExtensionSupported = false;

		for (const auto& extension : availableExtensions) {
			if (strcmp(requiredExtensionName, extension.extensionName) == 0) {
				isExtensionSupported = true;
				break;
			}
		}

		if (!isExtensionSupported) {
			return false;
		}
	}

	return true;
}

SwapchainSupportDetails VulkanRenderer::getSwapchainSupportDetails(VkPhysicalDevice physicalDevice)
{
	SwapchainSupportDetails supportDetails;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &supportDetails.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		supportDetails.surfaceFormats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, supportDetails.surfaceFormats.data());
	}

	uint32_t modeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr);

	if (modeCount != 0) {
		supportDetails.presentModes.resize(formatCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr);
	}

	return supportDetails;
}

VkSurfaceFormatKHR VulkanRenderer::chooseSwapchainSurfaceFormat(const std::vector<VkSurfaceFormatKHR> availableFormats)
{
	for (const auto& format : availableFormats) {
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
			return format;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR VulkanRenderer::chooseSwapchainPresentMode(const std::vector<VkPresentModeKHR> availableModes)
{
	for (const auto& mode : availableModes) {
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return mode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilites)
{
	if (capabilites.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilites.currentExtent;
	}
	else {
		int width;
		int height;

		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D extent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height),
		};

		extent.width = std::clamp(extent.width, capabilites.minImageExtent.width, capabilites.maxImageExtent.width);
		extent.height = std::clamp(extent.height, capabilites.minImageExtent.height, capabilites.maxImageExtent.width);

		return extent;
	}
}

VkResult VulkanRenderer::createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMesseneger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMesseneger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanRenderer::destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}