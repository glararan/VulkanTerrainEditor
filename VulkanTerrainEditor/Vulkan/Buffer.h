#ifndef BUFFER_H
#define BUFFER_H

#include <QVulkanFunctions>

namespace Vulkan
{
    struct Buffer
    {
        QVulkanDeviceFunctions* functions = Q_NULLPTR;

        VkDevice device;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDescriptorBufferInfo descriptor;
        VkDeviceSize size = 0;
        VkDeviceSize alignment = 0;

        void* mapped = nullptr;

        VkBufferUsageFlags usageFlags; // Usage flags to be filled by external source at buffer creation (to query at some later point)
        VkMemoryPropertyFlags memoryPropertyFlags; // Memory propertys flags to be filled by external source at buffer creation (to query at some later point)

        /**
                * Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
                *
                * @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete buffer range.
                * @param offset (Optional) Byte offset from beginning
                *
                * @return VkResult of the buffer mapping call
                */
        VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
        {
            return functions->vkMapMemory(device, memory, offset, size, 0, &mapped);
        }

        /**
                * Unmap a mapped memory range
                *
                * @note Does not return a result as vkUnmapMemory can't fail
                */
        void unmap()
        {
            if (mapped)
            {
                functions->vkUnmapMemory(device, memory);

                mapped = nullptr;
            }
        }

        /**
                * Attach the allocated memory block to the buffer
                *
                * @param offset (Optional) Byte offset (from the beginning) for the memory region to bind
                *
                * @return VkResult of the bindBufferMemory call
                */
        VkResult bind(VkDeviceSize offset = 0)
        {
            return functions->vkBindBufferMemory(device, buffer, memory, offset);
        }

        /**
                * Setup the default descriptor for this buffer
                *
                * @param size (Optional) Size of the memory range of the descriptor
                * @param offset (Optional) Byte offset from beginning
                *
                */
        void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
        {
            descriptor.offset = offset;
            descriptor.buffer = buffer;
            descriptor.range = size;
        }

        /**
                * Copies the specified data to the mapped buffer
                *
                * @param data Pointer to the data to copy
                * @param size Size of the data to copy in machine units
                *
                */
        void copyTo(void* data, VkDeviceSize size)
        {
            Q_ASSERT(mapped);

            memcpy(mapped, data, size);
        }

        /**
                * Flush a memory range of the buffer to make it visible to the device
                *
                * @note Only required for non-coherent memory
                *
                * @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the complete buffer range.
                * @param offset (Optional) Byte offset from beginning
                *
                * @return VkResult of the flush call
                */
        VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
        {
            VkMappedMemoryRange mappedRange = {};
            mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            mappedRange.memory = memory;
            mappedRange.offset = offset;
            mappedRange.size = size;

            return functions->vkFlushMappedMemoryRanges(device, 1, &mappedRange);
        }

        /**
                * Invalidate a memory range of the buffer to make it visible to the host
                *
                * @note Only required for non-coherent memory
                *
                * @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
                * @param offset (Optional) Byte offset from beginning
                *
                * @return VkResult of the invalidate call
                */
        VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
        {
            VkMappedMemoryRange mappedRange = {};
            mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            mappedRange.memory = memory;
            mappedRange.offset = offset;
            mappedRange.size = size;

            return functions->vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);
        }

        /**
                * Release all Vulkan resources held by this buffer
                */
        void destroy()
        {
            if (buffer)
                functions->vkDestroyBuffer(device, buffer, nullptr);

            if (memory)
                functions->vkFreeMemory(device, memory, nullptr);
        }
    };
}

#endif // BUFFER_H
