#include "triangleRenderer.h"

#include "errorLog.h"
#include "validation.h"
#include "shader.h"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#include <array>

TriangleRenderer::TriangleRenderer(bool isDebug)
{
    this->isDebug = isDebug;
}

void TriangleRenderer::initWindow(uint32_t windowWidth, uint32_t windowHeight, std::string windowTitle)
{
	glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(windowWidth, windowHeight, windowTitle.c_str(), nullptr, nullptr);

    if (!window) {
        error::log("cannot create GLFW window.");
    }
}

void TriangleRenderer::draw()
{
	static uint32_t currentFrame = 0;
	vkWaitForFences(device->getLogicalDevice(), 1, &bufferFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(device->getLogicalDevice(), 1, &bufferFences[currentFrame]);

	uint32_t imageIndex;
	swapchain->acquireNextImage(imageAvailableSemaphores[currentFrame], &imageIndex);

	vkResetCommandBuffer(cmdBuffers[currentFrame], 0);
	recordCmdBuffer(cmdBuffers[currentFrame], imageIndex);

	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffers[currentFrame];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];

	VkResult result = vkQueueSubmit(device->getGraphicsQueue(), 1, &submitInfo, bufferFences[currentFrame]);
	if (result != VK_SUCCESS) {
		error::log("cannot submit Graphics Queue.");
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
	presentInfo.swapchainCount = 1;
	VkSwapchainKHR usedSpawchain = *(swapchain);
	presentInfo.pSwapchains = &usedSpawchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(device->getGraphicsQueue(), &presentInfo);
	if (result != VK_SUCCESS) {
		error::log("cannot Present Image.");
	}

	currentFrame = (currentFrame + 1) % swapchain->getImageCount();
}

void TriangleRenderer::startLoop()
{
	while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
		draw();
    }

	vkDeviceWaitIdle(device->getLogicalDevice());
}

TriangleRenderer::~TriangleRenderer()
{
	for (uint32_t i = 0; i < swapchain->getImageCount(); i++) {
		vkDestroyFence(device->getLogicalDevice(), bufferFences[i], nullptr);
		vkDestroySemaphore(device->getLogicalDevice(), imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(device->getLogicalDevice(), renderFinishedSemaphores[i], nullptr);
	}

	vkDestroyCommandPool(device->getLogicalDevice(), cmdPool, nullptr);

	vkDestroyPipelineLayout(device->getLogicalDevice(), graphicsPipelineLayout, nullptr);

	vkDestroyPipeline(device->getLogicalDevice(), graphicsPipeline, nullptr);

	for (const auto& framebuffer : swapchainFramebuffers) {
		vkDestroyFramebuffer(device->getLogicalDevice(), framebuffer, nullptr);
	}

	vkDestroyRenderPass(device->getLogicalDevice(), renderPass, nullptr);

	destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

	delete swapchain;
	delete device;

	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
    glfwTerminate();
}

void TriangleRenderer::initVulkan()
{
	volkInitialize();

    createInstance();
	createDebugMessenger(instance, &debugMessenger);
	createDevice();
	createSwapchain();
	createRenderPass();
	createSwapchainFramebuffers();
	createGraphicsPipeline();
	createCmdPool();
	createCmdBuffers();
	createSyncTools();
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

	std::vector<const char*> extensions = getRequiredExtensions();
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

void TriangleRenderer::createSwapchainFramebuffers()
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

void TriangleRenderer::createGraphicsPipeline()
{
	// STAGES
	Shader vertShader(device->getLogicalDevice(), "shaders/triangle_vert.spv");
	Shader fragShader(device->getLogicalDevice(), "shaders/triangle_frag.spv");

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

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
		vertStageInfo,
		fragStageInfo
	};
	
	// VERTEX INPUT STATE
	VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {};
	vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateInfo.pVertexAttributeDescriptions = nullptr;

	// INPUT ASSEMLY STATE
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo = {};
	inputAssemblyStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;

	// VIEWPORT STATE
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.height = swapchain->getExtent().height;
	viewport.width = swapchain->getExtent().width;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = swapchain->getExtent();

	VkPipelineViewportStateCreateInfo viewportStateInfo = {};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports = &viewport;
	viewportStateInfo.scissorCount = 1;
	viewportStateInfo.pScissors = &scissor;

	// RASTERIZATION STATE
	VkPipelineRasterizationStateCreateInfo rasterStateInfo = {};
	rasterStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterStateInfo.depthClampEnable = VK_FALSE;
	rasterStateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterStateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterStateInfo.depthBiasEnable = VK_FALSE;
	rasterStateInfo.lineWidth = 1.0f;

	// MULTISAMPLE STATE
	VkPipelineMultisampleStateCreateInfo multisampleStateInfo = {};
	multisampleStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateInfo.alphaToOneEnable = VK_FALSE;

	// DEPTH STENCIL STATE
	VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo = {};
	depthStencilStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateInfo.stencilTestEnable = VK_FALSE;

	// COLOR BLEND STATE
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = {};
	colorBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateInfo.attachmentCount = 1;
	colorBlendStateInfo.pAttachments = &colorBlendAttachment;

	// PIPELINE LAYOUT
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pSetLayouts = nullptr;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	VkResult result = vkCreatePipelineLayout(device->getLogicalDevice(), &pipelineLayoutInfo, nullptr, &graphicsPipelineLayout);
	if (result != VK_SUCCESS) {
		error::log("cannot create Graphics Pipeline Layout.");
	}

	// GRAPHICS PIPELINE
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputStateInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyStateInfo;
	pipelineInfo.pTessellationState = nullptr;
	pipelineInfo.pViewportState = &viewportStateInfo;
	pipelineInfo.pRasterizationState = &rasterStateInfo;
	pipelineInfo.pMultisampleState = &multisampleStateInfo;
	pipelineInfo.pDepthStencilState = &depthStencilStateInfo;
	pipelineInfo.pColorBlendState = &colorBlendStateInfo;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = graphicsPipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	result = vkCreateGraphicsPipelines(device->getLogicalDevice(), nullptr, 1, &pipelineInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS) {
		error::log("cannot create Graphics Pipeline.");
	}
}

void TriangleRenderer::createCmdPool()
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cmdPoolInfo.queueFamilyIndex = device->getGraphicsFamilyIndex();

	VkResult result = vkCreateCommandPool(device->getLogicalDevice(), &cmdPoolInfo, nullptr, &cmdPool);
	if (result != VK_SUCCESS) {
		error::log("cannot create Command Pool.");
	}
}

void TriangleRenderer::createCmdBuffers()
{
	cmdBuffers.resize(swapchain->getImageCount());

	VkCommandBufferAllocateInfo cmdBufferInfo = {};
	cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufferInfo.commandPool = cmdPool;
	cmdBufferInfo.commandBufferCount = static_cast<uint32_t>(cmdBuffers.size());
	cmdBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkResult result = vkAllocateCommandBuffers(device->getLogicalDevice(), &cmdBufferInfo, cmdBuffers.data());
	if (result != VK_SUCCESS) {
		error::log("cannot allocate cmdBuffers.");
	}
}

void TriangleRenderer::recordCmdBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.flags = 0;
	cmdBeginInfo.pInheritanceInfo = nullptr;

	VkResult result = vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo);
	if (result != VK_SUCCESS) {
		error::log("cannot begin command buffer.");
	}

	VkClearValue clearValue = {};
	clearValue.color = { 0.3, 0.1, 0.7, 1.0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = swapchainFramebuffers[imageIndex];
	renderPassBeginInfo.renderArea.extent = swapchain->getExtent();
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearValue;

	vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
	vkCmdEndRenderPass(cmdBuffer);

	result = vkEndCommandBuffer(cmdBuffer);
	if (result != VK_SUCCESS) {
		error::log("cannot end Command Buffer.");
	}
}

void TriangleRenderer::createSyncTools()
{
	uint32_t imageCount = swapchain->getImageCount();
	imageAvailableSemaphores.resize(imageCount);
	renderFinishedSemaphores.resize(imageCount);
	bufferFences.resize(imageCount);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.flags = 0;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (uint32_t i = 0; i < imageCount; i++) {
		VkResult result = vkCreateSemaphore(device->getLogicalDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
		if (result != VK_SUCCESS) {
			error::log("cannot create Image Available Semaphore.");
		}

		result = vkCreateSemaphore(device->getLogicalDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
		if (result != VK_SUCCESS) {
			error::log("cannot create Render Finished Semaphore.");
		}

		result = vkCreateFence(device->getLogicalDevice(), &fenceInfo, nullptr, &bufferFences[i]);
		if (result != VK_SUCCESS) {
			error::log("cannot create Buffer Fence.");
		}
	}
}

std::vector<const char *> TriangleRenderer::getRequiredExtensions()
{
	uint32_t extensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + extensionCount);

	return extensions;
}