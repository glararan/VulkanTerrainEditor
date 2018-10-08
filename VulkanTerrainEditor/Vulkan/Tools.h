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
		
		VkShaderModule loadShader(const QString& fileName, VkDevice device, QVulkanDeviceFunctions* functions); // Load a SPIR-V shader (binary)
		VkShaderModule loadShaderGLSL(const QString& fileName, VkDevice device, QVulkanDeviceFunctions* functions, VkShaderStageFlagBits stage); // Note: GLSL support requires vendor-specific extensions to be enabled and is not a core-feature of Vulkan

        void setImageLayout(VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        void setImageLayout(VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}
}

#endif // TOOLS_H
