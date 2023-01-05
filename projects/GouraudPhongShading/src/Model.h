#pragma once

// std
#include <string>
#include <vector>
#include <array>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec2 texCoord;
	glm::vec3 norm;

	static VkVertexInputBindingDescription	getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		bindingDescription.stride = sizeof(Vertex);

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 4> attributeDescriptors = {};

		attributeDescriptors[0].binding = 0;
		attributeDescriptors[0].location = 0;
		attributeDescriptors[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptors[0].offset = offsetof(Vertex, position);

		attributeDescriptors[1].binding = 0;
		attributeDescriptors[1].location = 1;
		attributeDescriptors[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptors[1].offset = offsetof(Vertex, color);

		attributeDescriptors[2].binding = 0;
		attributeDescriptors[2].location = 2;
		attributeDescriptors[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptors[2].offset = offsetof(Vertex, texCoord);

		attributeDescriptors[3].binding = 0;
		attributeDescriptors[3].location = 3;
		attributeDescriptors[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptors[3].offset = offsetof(Vertex, norm);

		return attributeDescriptors;
	}
};

class Model
{
public:

	Model() {};
	Model(std::string modelPath, std::string texturePath, VkDevice device, VkPhysicalDevice physicalDevice);
	~Model();

	VkBuffer getVertexBuffer();
	uint32_t getVertexCount();
	VkBuffer getIndexBuffer();
	uint32_t getIndexCount();

private:

	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
	
	VkResult result;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	void loadModel(const std::string& modelPath);
	void createVertexBuffer();
	void createIndexBuffer();
};

