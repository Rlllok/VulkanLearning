#include "Texture.h"

// std
#include <stdexcept>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Utils.hpp"

Texture::Texture(std::string texturePath, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, uint32_t graphicsQueueIndex, VkQueue queue)
{
	this->device = device;
	this->physicalDevice = physicalDevice;
	this->texturePath = texturePath;
	this->commandPool = commandPool;
	this->graphicsQueueIndex = graphicsQueueIndex;
	this->queue = queue;
	
	createCopyCommandBuffer();

	createTextureImage();
	createTextureImageView();
	createTextureSampler();
}

Texture::~Texture()
{
	vkDestroySampler(device, textureSampler, nullptr);
	vkDestroyImageView(device, textureImageView, nullptr);
	vkFreeMemory(device, textureImageMemory, nullptr);
	vkDestroyImage(device, textureImage, nullptr);
}

VkImageView Texture::getImageView()
{
	return textureImageView;
}

VkSampler Texture::getSampler()
{
	return textureSampler;
}

void Texture::createTextureImage()
{
	int texWidth;
	int texHeight;
	int texChannels;

	stbi_uc* pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) {
		throw std::runtime_error("ERROR: cannot read texture file \"" + texturePath + "\"");
	}

	textureExtent.width = texWidth;
	textureExtent.height = texHeight;
	textureExtent.depth = 1;

	mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	createStagingBuffer(imageSize);

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageInfo.extent = textureExtent;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	result = vkCreateImage(device, &imageInfo, nullptr, &textureImage);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Texture Image.");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetImageMemoryRequirements(device, textureImage, &memRequirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memRequirements.size;
	allocateInfo.memoryTypeIndex = findMemoryType(
		physicalDevice,
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	result = vkAllocateMemory(device, &allocateInfo, nullptr, &textureImageMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate Texture Memory.");
	}

	vkBindImageMemory(device, textureImage, textureImageMemory, 0);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, imageSize);
	vkUnmapMemory(device, stagingBufferMemory);

	stbi_image_free(pixels);

	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, textureImage, textureExtent);
	generateMipMaps(textureImage, texWidth, texHeight, mipLevels);
	//transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkFreeMemory(device, stagingBufferMemory, nullptr);
	vkDestroyBuffer(device, stagingBuffer, nullptr);
}

void Texture::createTextureImageView()
{
	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.image = textureImage;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewInfo.subresourceRange.layerCount = 1;
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.levelCount = mipLevels;
	imageViewInfo.subresourceRange.baseMipLevel = 0;

	result = vkCreateImageView(device, &imageViewInfo, nullptr, &textureImageView);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Texture Image View.");
	}
}

void Texture::createTextureSampler()
{
	VkPhysicalDeviceProperties deviceProperties = {};
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipLodBias = 0.0f;
	//samplerInfo.minLod = static_cast<float>(mipLevels / 2);
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = static_cast<float>(mipLevels);
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	result = vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Texture Sampler.");
	}
}

void Texture::createStagingBuffer(VkDeviceSize bufferSize)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	result = vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Staging Buffer for Texture.");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memRequirements.size;
	allocateInfo.memoryTypeIndex = findMemoryType(
		physicalDevice,
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	result = vkAllocateMemory(device, &allocateInfo, nullptr, &stagingBufferMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate Staging Buffer Memory.");
	}

	vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);
}

void Texture::createCopyCommandBuffer()
{
	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	result = vkAllocateCommandBuffers(device, &allocateInfo, &copyCommandBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate Copy Command Buffer.");
	}
}

void Texture::copyBufferToImage(VkBuffer buffer, VkImage image, VkExtent3D imageExtent)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(copyCommandBuffer, &beginInfo);
		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.mipLevel = 0;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = imageExtent;

		vkCmdCopyBufferToImage(
			copyCommandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);
	vkEndCommandBuffer(copyCommandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &copyCommandBuffer;

	// Flush Command Buffer
	VkFence fence;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	vkCreateFence(device, &fenceInfo, nullptr, &fence);

	vkQueueSubmit(queue, 1, &submitInfo, fence);

	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT32_MAX);
	vkDestroyFence(device, fence, nullptr);
}

void Texture::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(copyCommandBuffer, &beginInfo);
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = textureImage;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseMipLevel = 0;

		VkPipelineStageFlags srcStage;
		VkPipelineStageFlags dstStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else {
			throw std::invalid_argument("ERROR: unsupported image layout transition.");
		}

		vkCmdPipelineBarrier(
			copyCommandBuffer,
			srcStage,
			dstStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	vkEndCommandBuffer(copyCommandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &copyCommandBuffer;

	// Flush Command Buffer
	VkFence fence;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	vkCreateFence(device, &fenceInfo, nullptr, &fence);

	vkQueueSubmit(queue, 1, &submitInfo, fence);

	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT32_MAX);
	vkDestroyFence(device, fence, nullptr);
}


void Texture::generateMipMaps(VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(copyCommandBuffer, &beginInfo);
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for (uint32_t i = 1; i < mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(
				copyCommandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);

			VkImageBlit imageBlit = {};
			imageBlit.srcOffsets[0] = { 0, 0, 0 };
			imageBlit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.srcSubresource.layerCount = 1;
			imageBlit.srcSubresource.baseArrayLayer = 0;
			imageBlit.srcSubresource.mipLevel = i - 1;
			imageBlit.dstOffsets[0] = { 0, 0, 0 };
			imageBlit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.dstSubresource.layerCount = 1;
			imageBlit.dstSubresource.baseArrayLayer = 0;
			imageBlit.dstSubresource.mipLevel = i;

			vkCmdBlitImage(
				copyCommandBuffer,
				textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &imageBlit,
				VK_FILTER_LINEAR
			);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(
				copyCommandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);

			if (mipWidth > 1) { mipWidth /= 2; }
			if (mipHeight > 1) { mipHeight /= 2; }
		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			copyCommandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	vkEndCommandBuffer(copyCommandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &copyCommandBuffer;

	VkFence fence;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	vkCreateFence(device, &fenceInfo, nullptr, &fence);

	vkQueueSubmit(queue, 1, &submitInfo, fence);

	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT32_MAX);
	vkDestroyFence(device, fence, nullptr);
}
