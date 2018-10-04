#ifndef MANAGER_H
#define MANAGER_H

#include <cstdint>

#include <QVulkanFunctions>

#include "Buffer.h"

#include "Common/Singleton/singleton.h"
#include "Editor/MapView.h"

#define VulkanManager Vulkan::Manager::getInstance()

namespace Vulkan
{
	class Manager
	{
    public:
        static Manager* getInstance();

        void initialize(MapView* mapView, VkPipelineCache vkPipelineCache);

		VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, void* data = nullptr);
		VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, Vulkan::Buffer* buffer, VkDeviceSize size, void* data = nullptr);
        VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin, bool oneTimeUse = false);
		VkShaderModule createShader(const QString& name);

		void flushCommandBuffer(VkCommandBuffer commandBuffer, bool free);

		VkCommandBuffer getCommandBuffer() const { return mapView->currentCommandBuffer(); }
		uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr);

		VkPipelineShaderStageCreateInfo loadShader(VkShaderModule& shader, VkShaderStageFlagBits stage);

		VkDevice device;
		VkPhysicalDevice physicalDevice;
		VkRenderPass renderPass;
		VkPhysicalDeviceMemoryProperties memoryProperties;
        VkPipelineCache pipelineCache;

		QVulkanInstance* instance;
		QVulkanFunctions* functions;
		QVulkanDeviceFunctions* deviceFuncs;

        uint32_t hostVisibleMemoryIndex;
        uint32_t deviceLocalMemoryIndex;

	private:
        static Manager* createInstance();

		MapView* mapView;
	};
}

#endif // MANAGER_H
