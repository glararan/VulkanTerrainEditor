#include "Manager.h"

#include "Initializers.h"
#include "Tools.h"

#include <QFile>

namespace Vulkan
{
	Manager::Manager(MapView* mapView) : mapView(mapView)
	{
		instance = mapView->vulkanInstance();

        device = mapView->device();
		physicalDevice = mapView->physicalDevice();
		renderPass = mapView->defaultRenderPass();

        hostVisibleMemoryIndex = mapView->hostVisibleMemoryIndex();
        deviceLocalMemoryIndex = mapView->deviceLocalMemoryIndex();

        functions = instance->functions();
        deviceFuncs = instance->deviceFunctions(device);

		functions->vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
	}

	VkResult Manager::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, void* data)
	{
		// Create the buffer handle
		VkBufferCreateInfo bufferCreateInfo = Vulkan::Initializers::bufferCreateInfo(usageFlags, size);
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(deviceFuncs->vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer));

		// Create the memory backing up the buffer handle
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc = Vulkan::Initializers::memoryAllocateInfo();

		deviceFuncs->vkGetBufferMemoryRequirements(device, *buffer, &memReqs);

		memAlloc.allocationSize = memReqs.size;
		// Find a memory type index that fits the properties of the buffer
		memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);

		VK_CHECK_RESULT(deviceFuncs->vkAllocateMemory(device, &memAlloc, nullptr, memory));

		// If a pointer to the buffer data has been passed, map the buffer and copy over the data
		if (data != nullptr)
		{
			void* mapped;

			VK_CHECK_RESULT(deviceFuncs->vkMapMemory(device, *memory, 0, size, 0, &mapped));

			memcpy(mapped, data, size);

			// If host coherency hasn't been requested, do a manual flush to make writes visible
			if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			{
				VkMappedMemoryRange mappedRange = Vulkan::Initializers::mappedMemoryRange();
				mappedRange.memory = *memory;
				mappedRange.offset = 0;
				mappedRange.size = size;

				deviceFuncs->vkFlushMappedMemoryRanges(device, 1, &mappedRange);
			}

			deviceFuncs->vkUnmapMemory(device, *memory);
		}

		// Attach the memory to the buffer object
		VK_CHECK_RESULT(deviceFuncs->vkBindBufferMemory(device, *buffer, *memory, 0));

		return VK_SUCCESS;
	}


	VkResult Manager::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, Vulkan::Buffer* buffer, VkDeviceSize size, void* data)
	{
		buffer->device = device;
		buffer->functions = deviceFuncs;

		// Create the buffer handle
		VkBufferCreateInfo bufferCreateInfo = Vulkan::Initializers::bufferCreateInfo(usageFlags, size);

		VK_CHECK_RESULT(deviceFuncs->vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer->buffer));

		// Create the memory backing up the buffer handle
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc = Vulkan::Initializers::memoryAllocateInfo();

		deviceFuncs->vkGetBufferMemoryRequirements(device, buffer->buffer, &memReqs);

		memAlloc.allocationSize = memReqs.size;
		// Find a memory type index that fits the properties of the buffer
		memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);

		VK_CHECK_RESULT(deviceFuncs->vkAllocateMemory(device, &memAlloc, nullptr, &buffer->memory));

		buffer->alignment = memReqs.alignment;
		buffer->size = memAlloc.allocationSize;
		buffer->usageFlags = usageFlags;
		buffer->memoryPropertyFlags = memoryPropertyFlags;

		// If a pointer to the buffer data has been passed, map the buffer and copy over the data
		if (data != nullptr)
		{
			VK_CHECK_RESULT(buffer->map());

			memcpy(buffer->mapped, data, size);

			if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
				buffer->flush();

			buffer->unmap();
		}

		// Initialize a default descriptor that covers the whole buffer size
		buffer->setupDescriptor();

		// Attach the memory to the buffer object
		return buffer->bind();
	}

	VkCommandBuffer Manager::createCommandBuffer(VkCommandBufferLevel level, bool begin)
	{
		VkCommandBuffer commandBuffer;

		VkCommandBufferAllocateInfo commandBufferAllocateInfo = Vulkan::Initializers::commandBufferAllocateInfo(mapView->graphicsCommandPool(), level, 1);

		VK_CHECK_RESULT(deviceFuncs->vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

		// If requested, also start the new command buffer
		if (begin)
		{
			VkCommandBufferBeginInfo commandBufferBeginInfo = Vulkan::Initializers::commandBufferBeginInfo();

			VK_CHECK_RESULT(deviceFuncs->vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		}

		return commandBuffer;

	}

	VkShaderModule Manager::createShader(const QString& name)
	{
		QFile file(name);

		if (!file.open(QIODevice::ReadOnly))
		{
			qWarning("Failed to read shader %s", qPrintable(name));

			return VK_NULL_HANDLE;
		}

		QByteArray blob = file.readAll();

		file.close();

		VkShaderModuleCreateInfo shaderInfo;

		memset(&shaderInfo, 0, sizeof(shaderInfo));

		shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderInfo.codeSize = blob.size();
		shaderInfo.pCode = reinterpret_cast<const quint32*>(blob.constData());

		VkShaderModule shaderModule;

		VK_CHECK_RESULT(deviceFuncs->vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule));

		return shaderModule;
	}

	void Manager::flushCommandBuffer(VkCommandBuffer commandBuffer, bool free)
	{
		if (commandBuffer == VK_NULL_HANDLE)
			return;

		VK_CHECK_RESULT(deviceFuncs->vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		VK_CHECK_RESULT(deviceFuncs->vkQueueSubmit(mapView->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));
		VK_CHECK_RESULT(deviceFuncs->vkQueueWaitIdle(mapView->graphicsQueue()));

		if (free)
			deviceFuncs->vkFreeCommandBuffers(device, mapView->graphicsCommandPool(), 1, &commandBuffer);
	}

	uint32_t Manager::getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound)
	{
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
		{
			if ((typeBits & 1) == 1)
			{
				if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					if (memTypeFound)
						*memTypeFound = true;

					return i;
				}
			}

			typeBits >>= 1;
		}

		if (memTypeFound)
		{
			*memTypeFound = false;

			return 0;
		}

		throw std::runtime_error("Could not find a matching memory type");
	}

	VkPipelineShaderStageCreateInfo Manager::loadShader(VkShaderModule& shader, VkShaderStageFlagBits stage)
	{
		VkPipelineShaderStageCreateInfo shaderStage = {};
		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.stage = stage;
		shaderStage.module = shader;
		shaderStage.pName = "main"; // todo : make param

        Q_ASSERT(shaderStage.module != VK_NULL_HANDLE);

		return shaderStage;
	}
}
