#include "triangleRenderer.h"

#include "errorLog.h"
#include "validation.h"
#include "shader.h"

#include <array>

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
TriangleRenderer::TriangleRenderer(struct android_app* app, bool isDebug) : BaseRenderer(app, isDebug)
{

}
#else
TriangleRenderer::TriangleRenderer(bool isDebug) : BaseRenderer(isDebug)
{
}
#endif

TriangleRenderer::~TriangleRenderer()
{
    for (uint32_t i = 0; i < swapchain->getImageCount(); i++) {
		vkDestroyFence(device->getLogicalDevice(), bufferFences[i], nullptr);
		vkDestroySemaphore(device->getLogicalDevice(), imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(device->getLogicalDevice(), renderFinishedSemaphores[i], nullptr);
	}

    vkDestroyCommandPool(device->getLogicalDevice(), cmdPool, nullptr);

    vkDestroyPipeline(device->getLogicalDevice(), graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device->getLogicalDevice(), graphicsPipelineLayout, nullptr);
}

void TriangleRenderer::prepareRenderer()
{
    BaseRenderer::prepareRenderer();
    createGraphicsPipeline();
    createCmdPool();
    createCmdBuffers();
    createSyncTools();

    isSetupCompleted = true;
}

void TriangleRenderer::createGraphicsPipeline()
{
    	// STAGES
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	Shader vertShader(device->getLogicalDevice(), "shaders/triangle_vert.spv", app);
	Shader fragShader(device->getLogicalDevice(), "shaders/triangle_frag.spv", app);
#else
	Shader vertShader(device->getLogicalDevice(), "shaders/triangle_vert.spv");
	Shader fragShader(device->getLogicalDevice(), "shaders/triangle_frag.spv");
#endif

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
	clearValue.color = { 1, 1, 1, 1.0 };

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
		error::log("cannot Present Image.");
	}

	currentFrame = (currentFrame + 1) % swapchain->getImageCount();
}
