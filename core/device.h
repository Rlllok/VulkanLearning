#pragma once

#include "volk.h"

#include <vector>

class Device
{
public:

    struct QueueFamilyIndecies
    {
        uint32_t graphicsFamily;
        uint32_t computeFamily;
        uint32_t transferFamily;
    };

    Device(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, VkQueueFlags queueTypes);
    ~Device();

    // Getters
    VkDevice            getLogicalDevice()          { return logicalDevice; }
    VkPhysicalDevice    getPhysicalDevice()         { return physicalDevice; }
    uint32_t            getGraphicsFamilyIndex()    { return queueFamilyIndecies.graphicsFamily; }
    VkQueue             getGraphicsQueue()          { return graphicsQueue; }

private:

    VkPhysicalDevice                        physicalDevice;
    VkPhysicalDeviceProperties              properties;
    VkPhysicalDeviceFeatures                enabledFeatures;
    std::vector<const char*>                enabledExtensions;
    VkPhysicalDeviceMemoryProperties        memoryProperties;
    std::vector<VkQueueFamilyProperties>    queueFamilyProperties;
    QueueFamilyIndecies                     queueFamilyIndecies;
    VkQueue                                 graphicsQueue;                                 
    VkDevice                                logicalDevice;

    void init();
    void clean();

    void createLogicalDevice(VkQueueFlags queueTypes);

    // Support functions
    uint32_t getQueueFamilyIndex(VkQueueFlags queueFlags);
};