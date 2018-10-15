#ifndef MANAGER_H
#define MANAGER_H

#include <cstdint>

#include <QVulkanFunctions>
#include <QVector>
#include <QSize>

#include "Buffer.h"

#include "Common/Singleton/Singleton.h"

#define VulkanManager Vulkan::Manager::getInstance()

class VulkanWindow;

namespace Vulkan
{
	class Manager
	{
    public:
        static Manager* getInstance();

        void initialize(VulkanWindow* vulkanWindow);

		VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, void* data = nullptr);
		VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, Vulkan::Buffer* buffer, VkDeviceSize size, void* data = nullptr);
        VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin, bool oneTimeUse = false);
		VkShaderModule createShader(const QString& name);

		void flushCommandBuffer(VkCommandBuffer commandBuffer, bool free);

        uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr);

        VulkanWindow* getWindow() { return window; }

		VkPipelineShaderStageCreateInfo loadShader(VkShaderModule& shader, VkShaderStageFlagBits stage);

		VkDevice device;
		VkPhysicalDevice physicalDevice;
        VkPhysicalDeviceFeatures deviceFeatures;
        VkPhysicalDeviceProperties deviceProperties;
		VkRenderPass renderPass;
		VkPhysicalDeviceMemoryProperties memoryProperties;
        VkPipelineCache pipelineCache;
        VkCommandPool commandPool;

        VkClearColorValue defaultClearColor = { { 0.67f, 0.84f, 0.9f, 1.0f } };

        struct
        {
            uint32_t graphics;
            uint32_t compute;
            uint32_t transfer;
        } queueFamilyIndices;

		QVulkanInstance* instance;
		QVulkanFunctions* functions;
        QVulkanDeviceFunctions* deviceFuncs;

        QSize viewportSize;

    private:
        VkResult createLogicalDevice(bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
        VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        uint32_t getQueueFamilyIndex(VkQueueFlagBits queueFlags);

        QVector<VkQueueFamilyProperties> queueFamilyProperties;

        static Manager* createInstance();

        VulkanWindow* window;
	};
}

#endif // MANAGER_H
