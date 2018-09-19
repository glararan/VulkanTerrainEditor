#ifndef TOOLS_H
#define TOOLS_H

#include <QDebug>
#include <QVulkanFunctions>

#define VK_FLAGS_NONE 0
#define DEFAULT_FENCE_TIMEOUT 100000000000

#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult result = f;																				\
																										\
	if (result != VK_SUCCESS)																			\
	{																									\
		qDebug() << "Fatal : VkResult is \"" << Vulkan::Tools::errorString(result) << "\" in " << __FILE__ << " at line " << __LINE__; \
																										\
		Q_ASSERT(result == VK_SUCCESS);																	\
	}																									\
}

#define ASSET_PATH "./../data/"

namespace Vulkan
{
	namespace Tools
	{
		extern bool errorSilentMode; // Disable message boxes on fatal errors

		QString errorString(VkResult errorCode); // Returns an error code as a string
		QString physicalDeviceTypeString(VkPhysicalDeviceType type); // Returns the device type as a string

		VkBool32 getSupportedDepthFormat(QVulkanDeviceFunctions* functions, VkPhysicalDevice physicalDevice, VkFormat* depthFormat); // Selected a suitable supported depth format starting with 32 bit down to 16 bit

		void setImageLayout(QVulkanDeviceFunctions* functions, VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT); // Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
		void setImageLayout(QVulkanDeviceFunctions* functions, VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT); // Uses a fixed sub resource layout with first mip level and layer
		void insertImageMemoryBarrier(QVulkanDeviceFunctions* functions, VkCommandBuffer cmdbuffer, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange); // Inser an image memory barrier into the command buffer

		// Display error message and exit on fatal error
		void exitFatal(QString message, int32_t exitCode);
		void exitFatal(QString message, VkResult resultCode);

		VkShaderModule loadShader(const QString& fileName, VkDevice device, QVulkanDeviceFunctions* functions); // Load a SPIR-V shader (binary)
		VkShaderModule loadShaderGLSL(const QString& fileName, VkDevice device, QVulkanDeviceFunctions* functions, VkShaderStageFlagBits stage); // Note: GLSL support requires vendor-specific extensions to be enabled and is not a core-feature of Vulkan
	}
}

#endif