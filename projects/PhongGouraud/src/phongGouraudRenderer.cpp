#include "phongGouraudRenderer.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "errorLog.h"
#include "utils.hpp"
#include "shader.h"

#include <chrono>
#include <vector>
#include <array>

#define MSSA_SAMPLES VK_SAMPLE_COUNT_1_BIT

PhongGouraudRenderer::PhongGouraudRenderer(bool isDebug) : BaseRenderer(isDebug)
{
}

PhongGouraudRenderer::~PhongGouraudRenderer()
{
    delete light;
    delete camera;
    delete model;
    delete texture;

    for (size_t i = 0; i < imageAvailableSemaphores.size(); i++) {
        vkDestroySemaphore(device->getLogicalDevice(), imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device->getLogicalDevice(), renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(device->getLogicalDevice(), bufferFences[i], nullptr);
    }

    vkDestroyDescriptorPool(device->getLogicalDevice(), descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device->getLogicalDevice(), descriptorSetLayout, nullptr);

    vkFreeMemory(device->getLogicalDevice(), mvpBufferMemory, nullptr);
    vkDestroyBuffer(device->getLogicalDevice(), mvpBuffer, nullptr);

    // vkDestroyPipelineLayout(device->getLogicalDevice(), gouraudPipelineLayout, nullptr);
    vkDestroyPipeline(device->getLogicalDevice(), gouraudPipeline, nullptr);
    vkDestroyPipelineLayout(device->getLogicalDevice(), phongPipelineLayout, nullptr);
    vkDestroyPipeline(device->getLogicalDevice(), phongPipeline, nullptr);

    vkDestroyCommandPool(device->getLogicalDevice(), cmdPool, nullptr);

    for (auto& framebuffer : swapchainFramebuffers) {
        vkDestroyFramebuffer(device->getLogicalDevice(), framebuffer, nullptr);
    }

    vkFreeMemory(device->getLogicalDevice(), depthImageMemory, nullptr);
    vkDestroyImageView(device->getLogicalDevice(), depthImageView, nullptr);
    vkDestroyImage(device->getLogicalDevice(), depthImage, nullptr);

    vkFreeMemory(device->getLogicalDevice(), colorImageMemory, nullptr);
    vkDestroyImageView(device->getLogicalDevice(), colorImageView, nullptr);
    vkDestroyImage(device->getLogicalDevice(), colorImage, nullptr);
}

void PhongGouraudRenderer::prepareRenderer()
{
    BaseRenderer::prepareRenderer();

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

    createColorResources();
    createDepthResources();
    createSwapchainFramebuffers();
    createDescriptorSetLayout();
    createPipelines();
    createCmdPool();
    createCmdBuffers();

    std::string modelsPath = MODELS_DIR;
	std::string texturesPath = TEXTURES_DIR;

    model = new Model(modelsPath + "/head.obj", texturesPath + "/head.tga", device->getLogicalDevice(), device->getPhysicalDevice());
    texture = new Texture(texturesPath + "/head.tga", device->getLogicalDevice(), device->getPhysicalDevice(), cmdPool, device->getGraphicsFamilyIndex(), device->getGraphicsQueue());

    createMVPBuffer();
    createDescriptorPool();
    createDescriptorSet();
    createSyncTools();

    isSetupCompleted = true;
}

void PhongGouraudRenderer::draw()
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
		throw std::runtime_error("ERROR: cannot Present Image.");
	}

	currentFrame = (currentFrame + 1) % swapchain->getImageCount();
}

void PhongGouraudRenderer::createColorResources()
{
    VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = swapchain->getImageFormat();
	imageInfo.extent.height = swapchain->getExtent().height;
	imageInfo.extent.width = swapchain->getExtent().width;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = MSSA_SAMPLES;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkResult result = vkCreateImage(device->getLogicalDevice(), &imageInfo, nullptr, &colorImage);
	if (result != VK_SUCCESS) {
		error::log("cannot create Color Image.");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetImageMemoryRequirements(device->getLogicalDevice(), colorImage, &memRequirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memRequirements.size;
	allocateInfo.memoryTypeIndex = findMemoryType(
		device->getPhysicalDevice(),
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	result = vkAllocateMemory(device->getLogicalDevice(), &allocateInfo, nullptr, &colorImageMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate Color Image Memory.");
	}

	vkBindImageMemory(device->getLogicalDevice(), colorImage, colorImageMemory, 0);

	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.image = colorImage;
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

	result = vkCreateImageView(device->getLogicalDevice(), &imageViewInfo, nullptr, &colorImageView);
	if (result != VK_SUCCESS) {
		error::log("cannot create Color Image View.");
	}
}

void PhongGouraudRenderer::createDepthResources()
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
	imageInfo.samples = MSSA_SAMPLES;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkResult result = vkCreateImage(device->getLogicalDevice(), &imageInfo, nullptr, &depthImage);
	if (result != VK_SUCCESS) {
		error::log("cannot create Depth Image.");
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
		error::log("cannot allocate Depth Image Memory.");
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
		error::log("cannot create Depth Image View.");
	}
}

void PhongGouraudRenderer::createRenderPass()
{
    VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapchain->getImageFormat();
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
	colorAttachmentResolve.format = swapchain->getImageFormat();
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

	VkResult result = vkCreateRenderPass(device->getLogicalDevice(), &renderPassInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS) {
		error::log("cannot create Render Pass.");
	}
}

void PhongGouraudRenderer::createSwapchainFramebuffers()
{
    uint32_t imageCount = swapchain->getImageCount();
    swapchainFramebuffers.resize(imageCount);

	for (uint32_t i = 0; i < imageCount; i++) {
		std::array<VkImageView, 3> attachments = {
			colorImageView,
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

void PhongGouraudRenderer::createDescriptorSetLayout()
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
	lightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
	lightLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 3> bindings = { mvpLayoutBinding, textureLayoutBinding, lightLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VkResult result = vkCreateDescriptorSetLayout(device->getLogicalDevice(), &layoutInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS) {
		error::log("cannot create Descriptor Set Layout.");
	}
}

void PhongGouraudRenderer::createDescriptorPool()
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

	VkResult result = vkCreateDescriptorPool(device->getLogicalDevice(), &descriptorPoolInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS) {
		error::log("cannot create Descriptor Pool.");
	}
}

void PhongGouraudRenderer::createDescriptorSet()
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

	vkUpdateDescriptorSets(device->getLogicalDevice(), static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

void PhongGouraudRenderer::createPipelines()
{
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

	VkResult result = vkCreatePipelineLayout(device->getLogicalDevice(), &pipelineLayoutInfo, nullptr, &phongPipelineLayout);
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
	graphicsPipelineInfo.layout = phongPipelineLayout;
	graphicsPipelineInfo.subpass = 0;
	graphicsPipelineInfo.renderPass = renderPass;
	graphicsPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineInfo.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(device->getLogicalDevice(), VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &phongPipeline);
	if (result != VK_SUCCESS) {
		error::log("cannot create Graphics Pipeline.");
	}

	Shader gouraudVertShader(device->getLogicalDevice(), "shaders/gouraud_vert.spv");
	Shader gouraudFragShader(device->getLogicalDevice(), "shaders/gouraud_frag.spv");

	VkPipelineShaderStageCreateInfo gouraudVertStageInfo = {};
	gouraudVertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	gouraudVertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	gouraudVertStageInfo.module = gouraudVertShader.getShaderModule();
	gouraudVertStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo gouraudFragStageInfo = {};
	gouraudFragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	gouraudFragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	gouraudFragStageInfo.module = gouraudFragShader.getShaderModule();
	gouraudFragStageInfo.pName = "main";

	std::vector<VkPipelineShaderStageCreateInfo> gouraudShaderStages = {
		gouraudVertStageInfo, gouraudFragStageInfo
	};

	graphicsPipelineInfo.stageCount = static_cast<uint32_t>(gouraudShaderStages.size());
	graphicsPipelineInfo.pStages = gouraudShaderStages.data();

	result = vkCreateGraphicsPipelines(device->getLogicalDevice(), VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &gouraudPipeline);
	if (result != VK_SUCCESS) {
		error::log("cannot create Graphics Pipeline.");
	}
}

void PhongGouraudRenderer::createCmdPool()
{
    VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = device->getGraphicsFamilyIndex();

	VkResult result = vkCreateCommandPool(device->getLogicalDevice(), &commandPoolInfo, nullptr, &cmdPool);
	if (result = VK_SUCCESS) {
		error::log("cannot create Command Buffer");
	}
}

void PhongGouraudRenderer::createCmdBuffers()
{
    cmdBuffers.resize(swapchain->getImageCount());

	VkCommandBufferAllocateInfo commandBufferInfo = {};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferInfo.commandPool = cmdPool;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferInfo.commandBufferCount = static_cast<uint32_t>(cmdBuffers.size());

	VkResult result = vkAllocateCommandBuffers(device->getLogicalDevice(), &commandBufferInfo, cmdBuffers.data());
	if (result != VK_SUCCESS) {
		error::log("cannot allocate CommandBuffer.");
	}
}

void PhongGouraudRenderer::recordCmdBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex)
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
	colorClear.color = { 0.01f, 0.01f, 0.01f, 1.0f };

	VkClearValue depthClear = {};
	depthClear.depthStencil = { 1.0, 0 };

	std::array<VkClearValue, 2> clearValues = { colorClear, depthClear };

	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	
		if (gouraudMode) {
			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gouraudPipeline);
		}
		else
		{
			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, phongPipeline);
		}

		VkBuffer vertexBuffers[] = {model->getVertexBuffer()};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

		VkBuffer indexBuffer = model->getIndexBuffer();
		vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(
			cmdBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			phongPipelineLayout,
			0,
			1,
			&descriptorSet,
			0,
			nullptr
		);

		uint32_t indexCount = model->getIndexCount();
		vkCmdDrawIndexed(cmdBuffer, indexCount, 1, 0, 0, 0);

	vkCmdEndRenderPass(cmdBuffer);

	result = vkEndCommandBuffer(cmdBuffer);
	if (result != VK_SUCCESS) {
		error::log("cannot end Command Buffer recording.");
	}
}

void PhongGouraudRenderer::createSyncTools()
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

void PhongGouraudRenderer::createMVPBuffer()
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

void PhongGouraudRenderer::updateMVPBuffer()
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
