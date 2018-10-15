#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include <QVulkanInstance>

#include "Tools.h"

namespace Vulkan
{
    typedef struct _SwapChainBuffers
    {
        VkImage image;
        VkImageView view;
    } SwapChainBuffer;

    class SwapChain
    {
    public:
        VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);

        void connect();
        void create(QSize size, bool vsync = false);
        void initSurface(VkSurfaceKHR vkSurface);

        VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);

        uint32_t queueNodeIndex = UINT32_MAX;
        uint32_t imageCount;

        VkFormat colorFormat;
        VkColorSpaceKHR colorSpace;

        QVector<SwapChainBuffer> buffers;

    private:
        PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
        PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
        PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
        PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
        PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
        PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
        PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
        PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
        PFN_vkQueuePresentKHR fpQueuePresentKHR;

        VkSwapchainKHR swapChain = VK_NULL_HANDLE;

        VkSurfaceKHR surface;

        QVector<VkImage> images;

    };
}

#endif // SWAPCHAIN_H
