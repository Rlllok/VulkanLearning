#include "deferredRenderer.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "errorLog.h"
#include "utils.hpp"
#include "shader.h"

#include <chrono>

DeferredRenderer::DeferredRenderer(bool isDebug) : BaseRenderer(isDebug)
{
 
}

DeferredRenderer::~DeferredRenderer()
{
    delete camera;
    delete light;
    delete texture;
    delete model;

    for (uint32_t i = 0; i < swapchain->getImageCount(); i++) {
        vkDestroySemaphore(device->getLogicalDevice(), imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device->getLogicalDevice(), renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(device->getLogicalDevice(), bufferFences[i], nullptr);
    }

    vkDestroyDescriptorPool(device->getLogicalDevice(), lightDescriptorPool, nullptr);
    vkDestroyDescriptorPool(device->getLogicalDevice(), inputDescriptorPool, nullptr);
    vkDestroyDescriptorPool(device->getLogicalDevice(), descriptorPool, nullptr);

    vkFreeMemory(device->getLogicalDevice(), mvpBufferMemory, nullptr);
    vkDestroyBuffer(device->getLogicalDevice(), mvpBuffer, nullptr);

    vkDestroyCommandPool(device->getLogicalDevice(), cmdPool, nullptr);

    vkDestroyPipeline(device->getLogicalDevice(), composePipeline, nullptr);
    vkDestroyPipelineLayout(device->getLogicalDevice(), composePipelineLayout, nullptr);
    vkDestroyPipeline(device->getLogicalDevice(), gBufferPipeline, nullptr);
    vkDestroyPipelineLayout(device->getLogicalDevice(), gBufferPipelineLayout, nullptr);

    vkDestroyDescriptorSetLayout(device->getLogicalDevice(), lightDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device->getLogicalDevice(), inputDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device->getLogicalDevice(), descriptorSetLayout, nullptr);

    for (size_t i = 0; i < swapchainFramebuffers.size(); i++) {
        vkDestroyFramebuffer(device->getLogicalDevice(), swapchainFramebuffers[i], nullptr);
    }

    vkFreeMemory(device->getLogicalDevice(), depthImageMemory, nullptr);
    vkDestroyImageView(device->getLogicalDevice(), depthImageView, nullptr);
    vkDestroyImage(device->getLogicalDevice(), depthImage, nullptr);

    for (size_t i = 0; i < colorImages.size(); i++) {
        vkDestroyImage(device->getLogicalDevice(), colorImages[i], nullptr);
        vkDestroyImageView(device->getLogicalDevice(), colorImageViews[i], nullptr);
        vkFreeMemory(device->getLogicalDevice(), colorImageMemories[i], nullptr);
    }

    for (size_t i = 0; i < normImages.size(); i++) {
        vkDestroyImage(device->getLogicalDevice(), normImages[i], nullptr);
        vkDestroyImageView(device->getLogicalDevice(), normImageViews[i], nullptr);
        vkFreeMemory(device->getLogicalDevice(), normImageMemories[i], nullptr);
    }

    for (size_t i = 0; i < positionImages.size(); i++) {
        vkDestroyImage(device->getLogicalDevice(), positionImages[i], nullptr);
        vkDestroyImageView(device->getLogicalDevice(), positionImageViews[i], nullptr);
        vkFreeMemory(device->getLogicalDevice(), positionImageMemories[i], nullptr);
    }
}

void DeferredRenderer::prepareRenderer()
{
    camera = new Camera(
		glm::vec3(0.0f, 0.0f, -5.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		45.0f,
		600 / float(800),
		0.1f
	);

    light = new Light(
		device->getLogicalDevice(),
		device->getPhysicalDevice(),
		glm::vec3(1.0f, 1.0f, -3.0f),
		glm::vec3(1.0f)
	);

    BaseRenderer::prepareRenderer();
    createColorAttachments();
    createNormAttachments();
    createPositionAttachments();
    createDepthResources();
    createSwapchainFramebuffers();
    createDescriptorSetLayout();
    createInputDescriptorSetLayout();
    createLightDescriptorSetLayout();
	createGbufferPipeline();
	createComposePipeline();
	createCmdPool();
	createCmdBuffers();

	std::string modelsPath = MODELS_DIR;
	std::string texturesPath = TEXTURES_DIR;

    model = new Model(modelsPath + "/head.obj", texturesPath + "/head.tga", device->getLogicalDevice(), device->getPhysicalDevice());
    texture = new Texture(texturesPath + "/head.tga", device->getLogicalDevice(), device->getPhysicalDevice(), cmdPool, device->getGraphicsFamilyIndex(), device->getGraphicsQueue());

	createMVPBuffer();
	createDescriptorPool();
	createInputDescriptorPool();
	createLightDescriptorPool();
	createDescriptorSet();
	createInputDescriptorSet();
	createLightDescriptorSets();
	createSyncTools();

    isSetupCompleted = true;
}

void DeferredRenderer::draw()
{
    static uint32_t currentFrame = 0;

	vkWaitForFences(device->getLogicalDevice(), 1, &bufferFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(device->getLogicalDevice(), 1, &bufferFences[currentFrame]);

	uint32_t imageIndex;
    swapchain->acquireNextImage(imageAvailableSemaphores[currentFrame], &imageIndex);

	updateMVPBuffer();

	vkResetCommandBuffer(cmdBuffers[currentFrame], 0);
	recordCmdBuffer(cmdBuffers[currentFrame], imageIndex);

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffers[currentFrame];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];

	// Fence become Signaled
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

void DeferredRenderer::createRenderPass()
{
    VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapchain->getImageFormat();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription normAttachment = {};
	normAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	normAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	normAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	normAttachment.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
	normAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	normAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	normAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	normAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription positionAttachment = {};
	positionAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	positionAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	positionAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	positionAttachment.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
	positionAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	positionAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	positionAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	positionAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = VK_FORMAT_D32_SFLOAT;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription swapchainAttachment = {};
	swapchainAttachment.format = swapchain->getImageFormat();
	swapchainAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	swapchainAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	swapchainAttachment.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
	swapchainAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	swapchainAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	swapchainAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapchainAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference normReference = {};
	normReference.attachment = 1;
	normReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference positionReference = {};
	positionReference.attachment = 2;
	positionReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 3;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorResolveReference = {};
	colorResolveReference.attachment = 4;
	colorResolveReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 3> colorAttachments = {
		colorReference,
		normReference,
		positionReference
	};

	std::array<VkSubpassDescription, 2> subpasses = {};
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
	subpasses[0].pColorAttachments = colorAttachments.data();
	subpasses[0].pDepthStencilAttachment = &depthReference;

	// INPUT REFERENCES
	VkAttachmentReference colorInputReference = {};
	colorInputReference.attachment = 0;
	colorInputReference.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference normInputReference = {};
	normInputReference.attachment = 1;
	normInputReference.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference positionInputReference = {};
	positionInputReference.attachment = 2;
	positionInputReference.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	std::array<VkAttachmentReference, 3> inputAttachments = {
		colorInputReference,
		normInputReference,
		positionInputReference
	};

	subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[1].colorAttachmentCount = 1;
	subpasses[1].pColorAttachments = &colorResolveReference;
	subpasses[1].inputAttachmentCount = static_cast<uint32_t>(inputAttachments.size());
	subpasses[1].pInputAttachments = inputAttachments.data();

	std::array<VkSubpassDependency, 3> subpassDependencies = {};
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].srcAccessMask = 0;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	subpassDependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstSubpass = 0;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	subpassDependencies[1].srcAccessMask = 0;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	subpassDependencies[2].srcSubpass = 0;
	subpassDependencies[2].dstSubpass = 1;
	subpassDependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	subpassDependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;


	std::array<VkAttachmentDescription, 5> attachments = {
		colorAttachment,
		normAttachment,
		positionAttachment,
		depthAttachment,
		swapchainAttachment
	};

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
	renderPassInfo.pSubpasses = subpasses.data();
	renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassInfo.pDependencies = subpassDependencies.data();

	VkResult result = vkCreateRenderPass(device->getLogicalDevice(), &renderPassInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS) {
        error::log("cannot create Render Pass.");
    }
}

void DeferredRenderer::createColorAttachments()
{
    uint32_t imageCount = swapchain->getImageCount();
    colorImages.resize(imageCount);
	colorImageViews.resize(imageCount);
	colorImageMemories.resize(imageCount);

	for (size_t i = 0; i < colorImages.size(); i++) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = swapchain->getImageFormat();
		imageInfo.extent.height = swapchain->getExtent().height;
		imageInfo.extent.width = swapchain->getExtent().width;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkResult result = vkCreateImage(device->getLogicalDevice(), &imageInfo, nullptr, &colorImages[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: cannot create Color Image.");
		}

		VkMemoryRequirements memRequirements = {};
		vkGetImageMemoryRequirements(device->getLogicalDevice(), colorImages[i], &memRequirements);

		VkMemoryAllocateInfo allocateInfo = {};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memRequirements.size;
		allocateInfo.memoryTypeIndex = findMemoryType(
			device->getPhysicalDevice(),
			memRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		result = vkAllocateMemory(device->getLogicalDevice(), &allocateInfo, nullptr, &colorImageMemories[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: cannot allocate Color Image Memory.");
		}

		vkBindImageMemory(device->getLogicalDevice(), colorImages[i], colorImageMemories[i], 0);

		VkImageViewCreateInfo imageViewInfo = {};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = colorImages[i];
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = swapchain->getImageFormat();
		imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.layerCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseMipLevel = 0;

		result = vkCreateImageView(device->getLogicalDevice(), &imageViewInfo, nullptr, &colorImageViews[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: cannot create Color Image View.");
		}
	}
}

void DeferredRenderer::createNormAttachments()
{
    uint32_t imageCount = swapchain->getImageCount();
    normImages.resize(imageCount);
	normImageViews.resize(imageCount);
	normImageMemories.resize(imageCount);

	for (size_t i = 0; i < normImages.size(); i++) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imageInfo.extent.width = swapchain->getExtent().width;
		imageInfo.extent.height = swapchain->getExtent().height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkResult result = vkCreateImage(device->getLogicalDevice(), &imageInfo, nullptr, &normImages[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: cannot create Norm Image.");
		}

		VkMemoryRequirements memRequirements = {};
		vkGetImageMemoryRequirements(device->getLogicalDevice(), normImages[i], &memRequirements);

		VkMemoryAllocateInfo allocateInfo = {};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memRequirements.size;
		allocateInfo.memoryTypeIndex = findMemoryType(
			device->getPhysicalDevice(),
			memRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		result = vkAllocateMemory(device->getLogicalDevice(), &allocateInfo, nullptr, &normImageMemories[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: cannot allocate Norm Image Memory.");
		}

		vkBindImageMemory(device->getLogicalDevice(), normImages[i], normImageMemories[i], 0);

		VkImageViewCreateInfo imageViewInfo = {};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = normImages[i];
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.layerCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;

		result = vkCreateImageView(device->getLogicalDevice(), &imageViewInfo, nullptr, &normImageViews[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: cannot create Norm Image View.");
		}
	}
}

void DeferredRenderer::createPositionAttachments()
{
    uint32_t imageCount = swapchain->getImageCount();
    positionImages.resize(imageCount);
	positionImageViews.resize(imageCount);
	positionImageMemories.resize(imageCount);

	for (size_t i = 0; i < positionImages.size(); i++) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imageInfo.extent.width = swapchain->getExtent().width;
		imageInfo.extent.height = swapchain->getExtent().height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkResult result = vkCreateImage(device->getLogicalDevice(), &imageInfo, nullptr, &positionImages[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: cannot create Position Image.");
		}

		VkMemoryRequirements memRequirements = {};
		vkGetImageMemoryRequirements(device->getLogicalDevice(), positionImages[i], &memRequirements);

		VkMemoryAllocateInfo allocateInfo = {};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memRequirements.size;
		allocateInfo.memoryTypeIndex = findMemoryType(
			device->getPhysicalDevice(),
			memRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		result = vkAllocateMemory(device->getLogicalDevice(), &allocateInfo, nullptr, &positionImageMemories[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: cannot allocate Position Image Memory.");
		}

		vkBindImageMemory(device->getLogicalDevice(), positionImages[i], positionImageMemories[i], 0);

		VkImageViewCreateInfo imageViewInfo = {};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = positionImages[i];
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.layerCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;

		result = vkCreateImageView(device->getLogicalDevice(), &imageViewInfo, nullptr, &positionImageViews[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: cannot create Position Image View.");
		}
	}
}

void DeferredRenderer::createDepthResources()
{
    VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_D32_SFLOAT;
	imageInfo.extent.width = swapchain->getExtent().width;
	imageInfo.extent.height = swapchain->getExtent().height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkResult result = vkCreateImage(device->getLogicalDevice(), &imageInfo, nullptr, &depthImage);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Depth Image.");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetImageMemoryRequirements(device->getLogicalDevice(), depthImage, &memRequirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memRequirements.size;
	allocateInfo.memoryTypeIndex = findMemoryType(
		device->getPhysicalDevice(),
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	result = vkAllocateMemory(device->getLogicalDevice(), &allocateInfo, nullptr, &depthImageMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate Depth Image Memory.");
	}

	vkBindImageMemory(device->getLogicalDevice(), depthImage, depthImageMemory, 0);

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

	result = vkCreateImageView(device->getLogicalDevice(), &imageViewInfo, nullptr, &depthImageView);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Depth Image View.");
	}
}

void DeferredRenderer::createSwapchainFramebuffers()
{
    swapchainFramebuffers.resize(swapchain->getImageCount());

	for (size_t i = 0; i < swapchainFramebuffers.size(); i++) {
		std::array<VkImageView, 5> attachments = {
			colorImageViews[i],
			normImageViews[i],
			positionImageViews[i],
			depthImageView,
			swapchain->getImageViews()[i]
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapchain->getExtent().width;
		framebufferInfo.height = swapchain->getExtent().height;
		framebufferInfo.layers = 1;

		VkResult result = vkCreateFramebuffer(device->getLogicalDevice(), &framebufferInfo, nullptr, &swapchainFramebuffers[i]);
		if (result != VK_SUCCESS) {
			error::log("cannot create Swapchain Framebuffer.");
		}
	}
}

void DeferredRenderer::createDescriptorSetLayout()
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

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { mvpLayoutBinding, textureLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VkResult result = vkCreateDescriptorSetLayout(device->getLogicalDevice(), &layoutInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS) {
		error::log("cannot create Descriptor Set Layout.");
	}
}

void DeferredRenderer::createInputDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding colorInputBinding = {};
	colorInputBinding.binding = 0;
	colorInputBinding.descriptorCount = 1;
	colorInputBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	colorInputBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	colorInputBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding normInputBinding = {};
	normInputBinding.binding = 1;
	normInputBinding.descriptorCount = 1;
	normInputBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	normInputBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	normInputBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding positionInputBinding = {};
	positionInputBinding.binding = 2;
	positionInputBinding.descriptorCount = 1;
	positionInputBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	positionInputBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	positionInputBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
		colorInputBinding,
		normInputBinding,
		positionInputBinding
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VkResult result = vkCreateDescriptorSetLayout(device->getLogicalDevice(), &layoutInfo, nullptr, &inputDescriptorSetLayout);
	if (result != VK_SUCCESS) {
		error::log("cannot create Input Descriptor Set Layout.");
	}
}

void DeferredRenderer::createLightDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding lightUniformBinding = {};
	lightUniformBinding.binding = 0;
	lightUniformBinding.descriptorCount = 1;
	lightUniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	lightUniformBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lightUniformBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &lightUniformBinding;

	VkResult result = vkCreateDescriptorSetLayout(device->getLogicalDevice(), &layoutInfo, nullptr, &lightDescriptorSetLayout);
	if (result != VK_SUCCESS) {
		error::log("cannot create Light Descriptor Set Layout.");
	}
}

void DeferredRenderer::createDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes = {};
	poolSizes[0].descriptorCount = 1;
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.maxSets = 1;
	descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descriptorPoolInfo.pPoolSizes = poolSizes.data();

	VkResult result = vkCreateDescriptorPool(device->getLogicalDevice(), &descriptorPoolInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS) {
		error::log("cannot create Descriptor Pool.");
	}
}

void DeferredRenderer::createInputDescriptorPool()
{
    VkDescriptorPoolSize poolSize = {};
	poolSize.descriptorCount = 1;
	poolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = swapchain->getImageCount();
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;

	VkResult result = vkCreateDescriptorPool(device->getLogicalDevice(), &poolInfo, nullptr, &inputDescriptorPool);
	if (result != VK_SUCCESS) {
		error::log("cannot create Input Descriptor Pool.");
	}
}

void DeferredRenderer::createLightDescriptorPool()
{
    VkDescriptorPoolSize poolSize = {};
	poolSize.descriptorCount = 1;
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.maxSets = swapchain->getImageCount();
	descriptorPoolInfo.poolSizeCount = 1;
	descriptorPoolInfo.pPoolSizes = &poolSize;

	VkResult result = vkCreateDescriptorPool(device->getLogicalDevice(), &descriptorPoolInfo, nullptr, &lightDescriptorPool);
	if (result != VK_SUCCESS)
	{
		error::log("cannot create Light Descriptor Pool.");
	}
}

void DeferredRenderer::createDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.descriptorSetCount = 1;
	allocateInfo.pSetLayouts = &descriptorSetLayout;

	VkResult result = vkAllocateDescriptorSets(device->getLogicalDevice(), &allocateInfo, &descriptorSet);
	if (result != VK_SUCCESS) {
		error::log("cannot allocate Descriptor Set.");
	}

	VkDescriptorBufferInfo descriptorBufferInfo = {};
	descriptorBufferInfo.buffer = mvpBuffer;
	descriptorBufferInfo.offset = 0;
	descriptorBufferInfo.range = sizeof(MVP);

	VkDescriptorImageInfo descriptorImageInfo = {};
	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptorImageInfo.imageView = texture->getImageView();
	descriptorImageInfo.sampler = texture->getSampler();

	std::array<VkWriteDescriptorSet, 2> writeSets = {};

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

	vkUpdateDescriptorSets(device->getLogicalDevice(), static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

void DeferredRenderer::createInputDescriptorSet()
{
    inputDescriptorSets.resize(swapchain->getImageCount());

	std::vector<VkDescriptorSetLayout> setLayouts(swapchain->getImageCount(), inputDescriptorSetLayout);

	VkDescriptorSetAllocateInfo setAllocateInfo = {};
	setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocateInfo.descriptorPool = inputDescriptorPool;
	setAllocateInfo.descriptorSetCount = static_cast<uint32_t>(inputDescriptorSets.size());
	setAllocateInfo.pSetLayouts = setLayouts.data();

	VkResult result = vkAllocateDescriptorSets(device->getLogicalDevice(), &setAllocateInfo, inputDescriptorSets.data());
	if (result != VK_SUCCESS) {
		error::log("cannot allocate Input Descriptor Set.");
	}

	for (size_t i = 0; i < inputDescriptorSets.size(); i++) {
		VkDescriptorImageInfo colorAttachmentDescriptor = {};
		colorAttachmentDescriptor.sampler = VK_NULL_HANDLE;
		colorAttachmentDescriptor.imageView = colorImageViews[i];
		colorAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo normAttachmentDescriptor = {};
		normAttachmentDescriptor.sampler = VK_NULL_HANDLE;
		normAttachmentDescriptor.imageView = normImageViews[i];
		normAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo positionAttachmentDescriptor = {};
		positionAttachmentDescriptor.sampler = VK_NULL_HANDLE;
		positionAttachmentDescriptor.imageView = positionImageViews[i];
		positionAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet colorWrite = {};
		colorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		colorWrite.dstSet = inputDescriptorSets[i];
		colorWrite.dstBinding = 0;
		colorWrite.dstArrayElement = 0;
		colorWrite.descriptorCount = 1;
		colorWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		colorWrite.pImageInfo = &colorAttachmentDescriptor;

		VkWriteDescriptorSet normWrite = {};
		normWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		normWrite.dstSet = inputDescriptorSets[i];
		normWrite.dstBinding = 1;
		normWrite.dstArrayElement = 0;
		normWrite.descriptorCount = 1;
		normWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		normWrite.pImageInfo = &normAttachmentDescriptor;

		VkWriteDescriptorSet positionWrite = {};
		positionWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		positionWrite.dstSet = inputDescriptorSets[i];
		positionWrite.dstBinding = 2;
		positionWrite.dstArrayElement = 0;
		positionWrite.descriptorCount = 1;
		positionWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		positionWrite.pImageInfo = &positionAttachmentDescriptor;

		std::array<VkWriteDescriptorSet, 3> writes = {
			colorWrite,
			normWrite,
			positionWrite
		};

		vkUpdateDescriptorSets(device->getLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}
}

void DeferredRenderer::createLightDescriptorSets()
{
    lightDescriptorSets.resize(swapchain->getImageCount());

	std::vector<VkDescriptorSetLayout> setLayouts(swapchain->getImageCount(), lightDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = lightDescriptorPool;
	allocateInfo.descriptorSetCount = static_cast<uint32_t>(lightDescriptorSets.size());
	allocateInfo.pSetLayouts = setLayouts.data();

	VkResult result = vkAllocateDescriptorSets(device->getLogicalDevice(), &allocateInfo, lightDescriptorSets.data());
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate Descriptor Set.");
	}

	for (size_t i = 0; i < lightDescriptorSets.size(); i++) {
		VkDescriptorBufferInfo descriptorBufferInfo = {};
		descriptorBufferInfo.buffer = light->getBuffer();
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = sizeof(Light::Properties);

		VkWriteDescriptorSet writeSet = {};

		writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet.dstSet = lightDescriptorSets[i];
		writeSet.dstBinding = 0;
		writeSet.dstArrayElement = 0;
		writeSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeSet.descriptorCount = 1;
		writeSet.pBufferInfo = &descriptorBufferInfo;

		vkUpdateDescriptorSets(device->getLogicalDevice(), 1, &writeSet, 0, nullptr);
	}
}

void DeferredRenderer::createGbufferPipeline()
{
    	// SHADERS
	Shader vertShader(device->getLogicalDevice(), "shaders/phong_vert.spv");
	Shader fragShader(device->getLogicalDevice(), "shaders/phong_frag.spv");

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
	viewport.width = static_cast<float>(swapchain->getExtent().width);
	viewport.height = static_cast<float>(swapchain->getExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.extent = swapchain->getExtent();
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
	multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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

	std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates(3, colorBlendAttachment);

	VkPipelineColorBlendStateCreateInfo colorBlendInfo = {};
	colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendInfo.logicOpEnable = VK_FALSE;
	colorBlendInfo.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
	colorBlendInfo.pAttachments = blendAttachmentStates.data();

	// PIPELINE LAYOUT
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	VkResult result = vkCreatePipelineLayout(device->getLogicalDevice(), &pipelineLayoutInfo, nullptr, &gBufferPipelineLayout);
	if (result != VK_SUCCESS) {
		error::log("cannot create Pipeline Layout.");
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
	graphicsPipelineInfo.layout = gBufferPipelineLayout;
	graphicsPipelineInfo.subpass = 0;
	graphicsPipelineInfo.renderPass = renderPass;
	graphicsPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineInfo.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(device->getLogicalDevice(), VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &gBufferPipeline);
	if (result != VK_SUCCESS) {
		error::log("cannot create Graphics Pipeline.");
	}
}

void DeferredRenderer::createComposePipeline()
{
    Shader vertShader(device->getLogicalDevice(), "shaders/second_vert.spv");
	Shader fragShader(device->getLogicalDevice(), "shaders/second_frag.spv");

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

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = { vertStageInfo, fragStageInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {};
	vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateInfo.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.width = static_cast<float>(swapchain->getExtent().width);
	viewport.height = static_cast<float>(swapchain->getExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 0.0f;
	viewport.x = 0;
	viewport.y = 0;

	VkRect2D scissor = {};
	scissor.extent = swapchain->getExtent();
	scissor.offset.x = 0;
	scissor.offset.y = 0;

	VkPipelineViewportStateCreateInfo viewportStateInfo = {};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports = &viewport;
	viewportStateInfo.scissorCount = 1;
	viewportStateInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateInfo = {};
	rasterizationStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateInfo.cullMode = VK_CULL_MODE_NONE;
	rasterizationStateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateInfo.lineWidth = 1.0f;

    // MULTISAMPLE STATE
	VkPipelineMultisampleStateCreateInfo multisampleInfo = {};
	multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleInfo.flags = 0;

	// DEPTH STENCIL STATE
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};
	depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilInfo.depthTestEnable = VK_TRUE;
	depthStencilInfo.depthWriteEnable = VK_TRUE;
	depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilInfo.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo blendStateInfo = {};
	blendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendStateInfo.logicOpEnable = VK_FALSE;
	blendStateInfo.attachmentCount = 1;
	blendStateInfo.pAttachments = &colorBlendAttachment;

	std::array<VkDescriptorSetLayout, 2> setLayouts = {
		inputDescriptorSetLayout,
		lightDescriptorSetLayout
	};

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(PushConstant);

	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	layoutInfo.pSetLayouts = setLayouts.data();
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;

	VkResult result = vkCreatePipelineLayout(device->getLogicalDevice(), &layoutInfo, nullptr, &composePipelineLayout);
	if (result != VK_SUCCESS) {
		error::log("cannot create Compose Pipeline Layout.");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputStateInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineInfo.pTessellationState = nullptr;
	pipelineInfo.pViewportState = &viewportStateInfo;
	pipelineInfo.pRasterizationState = &rasterizationStateInfo;
	pipelineInfo.pMultisampleState = &multisampleInfo;
	pipelineInfo.pDepthStencilState = &depthStencilInfo;
	pipelineInfo.pColorBlendState = &blendStateInfo;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = composePipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 1;

	result = vkCreateGraphicsPipelines(device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &composePipeline);
	if (result != VK_SUCCESS) {
		error::log("ERROR: cannot create Compose Pipeline.");
	}
}

void DeferredRenderer::createCmdPool()
{
    VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = device->getGraphicsFamilyIndex();

	VkResult result = vkCreateCommandPool(device->getLogicalDevice(), &commandPoolInfo, nullptr, &cmdPool);
	if (result != VK_SUCCESS) {
		error::log("cannot create Command Pool.");
	}
}

void DeferredRenderer::createCmdBuffers()
{
    cmdBuffers.resize(swapchain->getImageCount());

	VkCommandBufferAllocateInfo commandBufferInfo = {};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferInfo.commandPool = cmdPool;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferInfo.commandBufferCount = static_cast<uint32_t>(cmdBuffers.size());

	VkResult result = vkAllocateCommandBuffers(device->getLogicalDevice(), &commandBufferInfo, cmdBuffers.data());
	if (result != VK_SUCCESS) {
		error::log("cannot allocate Command Buffer.");
	}
}

void DeferredRenderer::recordCmdBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBeginInfo.flags = 0;
	bufferBeginInfo.pInheritanceInfo = nullptr;

	VkResult result = vkBeginCommandBuffer(cmdBuffer, &bufferBeginInfo);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot begin Command Buffer recording.");
	}

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = swapchainFramebuffers[imageIndex];
	renderPassBeginInfo.renderArea.extent = swapchain->getExtent();
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;

	VkClearValue colorClear = {};
	colorClear.color = { 0.0f, 0.0f, 0.0f, 1.0f };

	VkClearValue normClear = {};
	normClear.color = { 0.0f, 0.0f, 0.0f, 1.0f };

	VkClearValue positionClear = {};
	positionClear.color = { 0.0f, 0.0f, 0.0f, 1.0f };

	VkClearValue depthClear = {};
	depthClear.depthStencil = { 1.0, 0 };

	VkClearValue swapchainClear = {};
	swapchainClear.color = { 0.0f, 1.0f, 0.0f, 1.0f };

	std::array<VkClearValue, 5> clearValues = {
		colorClear,
		normClear,
		positionClear,
		depthClear,
		swapchainClear
	};

	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gBufferPipeline);

		VkBuffer vertexBuffers[] = {model->getVertexBuffer()};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

		VkBuffer indexBuffer = model->getIndexBuffer();
		vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(
			cmdBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			gBufferPipelineLayout,
			0,
			1,
			&descriptorSet,
			0,
			nullptr
		);

		uint32_t indexCount = model->getIndexCount();
		vkCmdDrawIndexed(cmdBuffer, indexCount, 1, 0, 0, 0);

		vkCmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, composePipeline);

		std::array<VkDescriptorSet, 2> descriptorSets = {
			inputDescriptorSets[imageIndex],
			lightDescriptorSets[imageIndex]
		};
		vkCmdBindDescriptorSets(
			cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, composePipelineLayout,
			0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr
		);
		vkCmdPushConstants(cmdBuffer, composePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &pushConstant);
		vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
	vkCmdEndRenderPass(cmdBuffer);

	result = vkEndCommandBuffer(cmdBuffer);
	if (result != VK_SUCCESS) {
		error::log("cannot end Command Buffer recording.");
	}
}

void DeferredRenderer::createSyncTools()
{
    uint32_t imageCount = swapchain->getImageCount();

	imageAvailableSemaphores.resize(imageCount);
	renderFinishedSemaphores.resize(imageCount);
	bufferFences.resize(imageCount);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < imageCount; i++) {
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

void DeferredRenderer::createMVPBuffer()
{
    VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.size = sizeof(MVP);
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(device->getLogicalDevice(), &bufferInfo, nullptr, &mvpBuffer);
	if (result != VK_SUCCESS) {
		error::log("cannot create MVP buffer.");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetBufferMemoryRequirements(device->getLogicalDevice(), mvpBuffer, &memRequirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memRequirements.size;
	allocateInfo.memoryTypeIndex = findMemoryType(
		device->getPhysicalDevice(),
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	result = vkAllocateMemory(device->getLogicalDevice(), &allocateInfo, nullptr, &mvpBufferMemory);
	if (result != VK_SUCCESS) {
		error::log("cannot allocate MVP Buffer Memory.");
	}

	vkBindBufferMemory(device->getLogicalDevice(), mvpBuffer, mvpBufferMemory, 0);

	vkMapMemory(device->getLogicalDevice(), mvpBufferMemory, 0, sizeof(MVP), 0, &mvpBufferMapped);
}

void DeferredRenderer::updateMVPBuffer()
{
    static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
	
	mvp.model = glm::mat4(1.0f);
	mvp.model = glm::rotate(mvp.model, deltaTime * glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	mvp.view = camera->getViewMatrix();
	mvp.projection = glm::perspective(glm::radians(camera->getFOV()), swapchain->getExtent().width / (float)swapchain->getExtent().height, 0.1f, 10.f);
	mvp.projection[1][1] *= -1;

	memcpy(mvpBufferMapped, &mvp, sizeof(MVP));
}
