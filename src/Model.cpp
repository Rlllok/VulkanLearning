#include "Model.h"

// std
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "utils.hpp"

Model::Model(std::string modelPath, std::string texturePath, VkDevice device, VkPhysicalDevice physicalDevice)
{
	this->device = device;
	this->physicalDevice = physicalDevice;

	loadModel(modelPath);
	createVertexBuffer();
	createIndexBuffer();
}

Model::~Model()
{
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);

	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
}

VkBuffer Model::getVertexBuffer()
{
	return vertexBuffer;
}

uint32_t Model::getVertexCount()
{
	return static_cast<uint32_t>(vertices.size());
}

VkBuffer Model::getIndexBuffer()
{
	return indexBuffer;
}

uint32_t Model::getIndexCount()
{
	return static_cast<uint32_t>(indices.size());
}


void Model::loadModel(const std::string& modelPath)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str())) {
		throw std::runtime_error("ERROR: cannot read model \"" + modelPath + "\".");
	}

	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			Vertex vertex = {};

			vertex.position.x = attrib.vertices[3 * index.vertex_index + 0];
			vertex.position.y = attrib.vertices[3 * index.vertex_index + 1];
			vertex.position.z = attrib.vertices[3 * index.vertex_index + 2];

			vertex.texCoord.x = attrib.texcoords[2 * index.texcoord_index + 0];
			vertex.texCoord.y = 1 - attrib.texcoords[2 * index.texcoord_index + 1];

			vertices.push_back(vertex);
			indices.push_back(indices.size());
		}
	}
}

void Model::createVertexBuffer()
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(vertices[0]) * vertices.size();
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	result = vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Vertex Buffer.");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memRequirements.size;
	allocateInfo.memoryTypeIndex = findMemoryType(
		physicalDevice,
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	result = vkAllocateMemory(device, &allocateInfo, nullptr, &vertexBufferMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate Vertex Buffer Memory.");
	}

	vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

	void* data;
	vkMapMemory(device, vertexBufferMemory, 0, bufferInfo.size, 0, &data);
		memcpy(data, vertices.data(), (size_t)bufferInfo.size);
	vkUnmapMemory(device, vertexBufferMemory);
}

void Model::createIndexBuffer()
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(indices[0]) * indices.size();
	bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	result = vkCreateBuffer(device, &bufferInfo, nullptr, &indexBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Index Buffer.");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetBufferMemoryRequirements(device, indexBuffer, &memRequirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memRequirements.size;
	allocateInfo.memoryTypeIndex = findMemoryType(
		physicalDevice,
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	result = vkAllocateMemory(device, &allocateInfo, nullptr, &indexBufferMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot allocate Index Buffer Memory.");
	}

	vkBindBufferMemory(device, indexBuffer, indexBufferMemory, 0);

	void* data;
	vkMapMemory(device, indexBufferMemory, 0, bufferInfo.size, 0, &data);
		memcpy(data, indices.data(), (size_t)bufferInfo.size);
	vkUnmapMemory(device, indexBufferMemory);
}
