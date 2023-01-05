#pragma once

// std
#include <string>

#include <vulkan/vulkan.h>

class Texture
{
public:

	Texture(std::string texturePath, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, uint32_t graphicsQueueIndex, VkQueue queue);
	~Texture();

	VkImageView getImageView();
	VkSampler getSampler();

private:
	
	VkResult result;
	
	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkCommandPool commandPool;
	uint32_t graphicsQueueIndex;
	VkQueue queue;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	VkCommandBuffer copyCommandBuffer;

	std::string texturePath;
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;
	VkExtent3D textureExtent;

	uint32_t mipLevels;

	void createTextureImage();
	void createTextureImageView();
	void createTextureSampler();
	void createStagingBuffer(VkDeviceSize bufferSize);
	void createCopyCommandBuffer();
	void copyBufferToImage(VkBuffer buffer, VkImage image, VkExtent3D imageExtent);
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void generateMipMaps(VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
};

