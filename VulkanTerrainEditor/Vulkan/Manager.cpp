#include "Manager.h"

#include "Initializers.h"
#include "Tools.h"

#include "VulkanWindow.h"

#include <QFile>

namespace Vulkan
{
    void Manager::initialize(VulkanWindow* vulkanWindow)
    {
        window = vulkanWindow;

        instance = window->getVulkanInstance();
        physicalDevice = window->getPhysicalDevice();
        functions = instance->functions();

        functions->vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
        functions->vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
		functions->vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

        uint32_t queueFamilyCount;

        functions->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        Q_ASSERT(queueFamilyCount > 0);

        queueFamilyProperties.resize(queueFamilyCount);

        functions->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

        VK_CHECK_RESULT(createLogicalDevice());

        deviceFuncs = instance->deviceFunctions(device);
	}

    Manager* Manager::createInstance()
    {
        return new Manager();
    }

    Manager* Manager::getInstance()
    {
        return Singleton<Manager>::instance(Manager::createInstance);
    }

    VkResult Manager::createLogicalDevice(bool useSwapChain, VkQueueFlags requestedQueueTypes)
    {
        QVector<VkDeviceQueueCreateInfo> queueCreateInfos;

        const float defaultQueuePriority(0.0f);

        // Graphics queue
        if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
        {
            queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);

            VkDeviceQueueCreateInfo queueInfo {};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = queueFamilyIndices.graphics;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &defaultQueuePriority;

            queueCreateInfos.push_back(queueInfo);
        }
        else
            queueFamilyIndices.graphics = VK_NULL_HANDLE;

        // Dedicated compute queue
        if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
        {
            queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);

            if (queueFamilyIndices.compute != queueFamilyIndices.graphics)
            {
                // If compute family index differs, we need an additional queue create info for the compute queue
                VkDeviceQueueCreateInfo queueInfo {};
                queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueInfo.queueFamilyIndex = queueFamilyIndices.compute;
                queueInfo.queueCount = 1;
                queueInfo.pQueuePriorities = &defaultQueuePriority;

                queueCreateInfos.push_back(queueInfo);
            }
        }
        else // Else we use the same queue
            queueFamilyIndices.compute = queueFamilyIndices.graphics;

        // Dedicated transfer queue
        if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
        {
            queueFamilyIndices.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);

            if ((queueFamilyIndices.transfer != queueFamilyIndices.graphics) && (queueFamilyIndices.transfer != queueFamilyIndices.compute))
            {
                // If compute family index differs, we need an additional queue create info for the compute queue
                VkDeviceQueueCreateInfo queueInfo {};
                queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueInfo.queueFamilyIndex = queueFamilyIndices.transfer;
                queueInfo.queueCount = 1;
                queueInfo.pQueuePriorities = &defaultQueuePriority;

                queueCreateInfos.push_back(queueInfo);
            }
        }
        else // Else we use the same queue
            queueFamilyIndices.transfer = queueFamilyIndices.graphics;

        QVector<const char*> deviceExtensions;

        //qDebug() << "supported:" << instance->supportedExtensions();
        //qDebug() << "enabled:" << instance->extensions();

        /*for(auto& extension : instance->extensions())
        {
            if(strcmp(extension.constData(), "VK_EXT_debug_report") == 0)
                continue;

            deviceExtensions.push_back(extension.constData());
        }*/

        if (useSwapChain) // If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
            deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.count();
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.constData();
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

        // Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
        /*if (extensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
        {
            deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
            enableDebugMarkers = true;
        }*/

        if (deviceExtensions.size() > 0)
        {
            deviceCreateInfo.enabledExtensionCount = deviceExtensions.count();
            deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.constData();
        }

        VkResult result = functions->vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);

        if (result == VK_SUCCESS)
        {
            deviceFuncs = instance->deviceFunctions(device);

            commandPool = createCommandPool(queueFamilyIndices.graphics); // Create a default command pool for graphics command buffers
        }

        return result;
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

    VkCommandBuffer Manager::createCommandBuffer(VkCommandBufferLevel level, bool begin, bool oneTimeUse)
    {
        VkCommandPool commandPool = window->getCommandPool();

        VkCommandBufferAllocateInfo commandBufferAllocateInfo = Vulkan::Initializers::commandBufferAllocateInfo(window->getCommandPool(), level, 1);

        VkCommandBuffer commandBuffer;

		VK_CHECK_RESULT(deviceFuncs->vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

		// If requested, also start the new command buffer
		if (begin)
		{
			VkCommandBufferBeginInfo commandBufferBeginInfo = Vulkan::Initializers::commandBufferBeginInfo();
            commandBufferBeginInfo.flags = oneTimeUse ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

			VK_CHECK_RESULT(deviceFuncs->vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
		}

		return commandBuffer;
	}

    VkCommandPool Manager::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags)
    {
        VkCommandPoolCreateInfo cmdPoolInfo = {};
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
        cmdPoolInfo.flags = createFlags;

        VkCommandPool cmdPool;

        VK_CHECK_RESULT(deviceFuncs->vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));

        return cmdPool;
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

        VK_CHECK_RESULT(deviceFuncs->vkQueueSubmit(window->getQueue(), 1, &submitInfo, VK_NULL_HANDLE));
        VK_CHECK_RESULT(deviceFuncs->vkQueueWaitIdle(window->getQueue()));

		if (free)
            deviceFuncs->vkFreeCommandBuffers(device, window->getCommandPool(), 1, &commandBuffer);
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

    uint32_t Manager::getQueueFamilyIndex(VkQueueFlagBits queueFlags)
    {
        // Dedicated queue for compute
        // Try to find a queue family index that supports compute but not graphics
        if (queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
            {
                if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
                    return i;
            }
        }

        // Dedicated queue for transfer
        // Try to find a queue family index that supports transfer but not graphics and compute
        if (queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
            {
                if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
                    return i;
            }
        }

        // For other queue types or if no separate compute queue is present, return the first one to support the requested flags
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
        {
            if (queueFamilyProperties[i].queueFlags & queueFlags)
                return i;
        }

        throw std::runtime_error("Could not find a matching queue family index");
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
