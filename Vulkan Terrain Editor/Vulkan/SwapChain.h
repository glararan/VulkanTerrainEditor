#ifndef SWAP_CHAIN_H
#define SWAP_CHAIN_H

#include <QVulkanFunctions>

namespace Vulkan
{
	// Macro to get a procedure address based on a vulkan instance
#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)                        \
{                                                                       \
	fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetInstanceProcAddr(inst, "vk"#entrypoint)); \
	if (fp##entrypoint == NULL)                                         \
	{																    \
		exit(1);                                                        \
	}                                                                   \
}

// Macro to get a procedure address based on a vulkan device
#define GET_DEVICE_PROC_ADDR(dev, entrypoint)                           \
{                                                                       \
	fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetDeviceProcAddr(dev, "vk"#entrypoint));   \
	if (fp##entrypoint == NULL)                                         \
	{																    \
		exit(1);                                                        \
	}                                                                   \
}

	typedef struct SwapChainBuffers
	{
		VkImage image;
		VkImageView view;
	} SwapChainBuffer;

	class SwapChain
	{
	public:
		void initSurface(void* platformHandle, void* platformWindow);

		void cleanup();

		void connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

		void create(uint32_t* width, uint32_t* height, bool vsync = false);

		VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);

		VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);

		VkFormat colorFormat;
		VkColorSpaceKHR colorSpace;
		/** @brief Handle to the current swap chain, required for recreation */
		VkSwapchainKHR swapChain = VK_NULL_HANDLE;
		uint32_t imageCount;
		QVector<VkImage> images;
		QVector<SwapChainBuffer> buffers;
		/** @brief Queue family index of the detected graphics and presenting device queue */
		uint32_t queueNodeIndex = UINT32_MAX;

	private:
		VkInstance instance;
		VkDevice device;
		VkPhysicalDevice physicalDevice;
		VkSurfaceKHR surface;

		// Function pointers
		PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
		PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
		PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
		PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
		PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
		PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
		PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
		PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
		PFN_vkQueuePresentKHR fpQueuePresentKHR;
	};
}

#endif // SWAP_CHAIN_H