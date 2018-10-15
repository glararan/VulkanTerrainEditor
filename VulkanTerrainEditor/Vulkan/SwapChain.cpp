#include "SwapChain.h"

#include "Manager.h"

#include <QSize>

namespace Vulkan
{
    VkResult SwapChain::acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex)
    {
        // By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
        // With that we don't have to handle VK_NOT_READY
        return fpAcquireNextImageKHR(VulkanManager->device, swapChain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, imageIndex);
    }

    void SwapChain::connect()
    {
        Vulkan::Manager* vkManager = VulkanManager;

        QVulkanInstance* instance = vkManager->instance;

        fpGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(instance->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceSupportKHR"));
        fpGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(instance->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
        fpGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(instance->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceFormatsKHR"));
        fpGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(instance->getInstanceProcAddr("vkGetPhysicalDeviceSurfacePresentModesKHR"));

        fpCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(vkManager->functions->vkGetDeviceProcAddr(vkManager->device, "vkCreateSwapchainKHR"));
        fpDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(vkManager->functions->vkGetDeviceProcAddr(vkManager->device, "vkDestroySwapchainKHR"));
        fpGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(vkManager->functions->vkGetDeviceProcAddr(vkManager->device, "vkGetSwapchainImagesKHR"));
        fpAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkManager->functions->vkGetDeviceProcAddr(vkManager->device, "vkAcquireNextImageKHR"));
        fpQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(vkManager->functions->vkGetDeviceProcAddr(vkManager->device, "vkQueuePresentKHR"));
    }

    void SwapChain::create(QSize size, bool vsync)
    {
        VkSwapchainKHR oldSwapchain = swapChain;

        Vulkan::Manager* vkManager = VulkanManager;

        // Get physical device surface properties and formats
        VkSurfaceCapabilitiesKHR surfCaps;

        VK_CHECK_RESULT(fpGetPhysicalDeviceSurfaceCapabilitiesKHR(vkManager->physicalDevice, surface, &surfCaps));

        // Get available present modes
        uint32_t presentModeCount;

        VK_CHECK_RESULT(fpGetPhysicalDeviceSurfacePresentModesKHR(vkManager->physicalDevice, surface, &presentModeCount, NULL));

        Q_ASSERT(presentModeCount > 0);

        QVector<VkPresentModeKHR> presentModes(presentModeCount);

        VK_CHECK_RESULT(fpGetPhysicalDeviceSurfacePresentModesKHR(vkManager->physicalDevice, surface, &presentModeCount, presentModes.data()));

        VkExtent2D swapchainExtent = {};

        // If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
        if (surfCaps.currentExtent.width == (uint32_t)-1)
        {
            // If the surface size is undefined, the size is set to
            // the size of the images requested.
            swapchainExtent.width = size.width();
            swapchainExtent.height = size.height();
        }
        else
        {
            // If the surface size is defined, the swap chain size must match
            swapchainExtent = surfCaps.currentExtent;

            size.setWidth(surfCaps.currentExtent.width);
            size.setHeight(surfCaps.currentExtent.height);
        }

        // Select a present mode for the swapchain

        // The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
        // This mode waits for the vertical blank ("v-sync")
        VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

        // If v-sync is not requested, try to find a mailbox mode
        // It's the lowest latency non-tearing present mode available
        if (!vsync)
        {
            for (size_t i = 0; i < presentModeCount; ++i)
            {
                if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;

                    break;
                }

                if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
                    swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }

        // Determine the number of images
        uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;

        if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
            desiredNumberOfSwapchainImages = surfCaps.maxImageCount;

        // Find the transformation of the surface
        VkSurfaceTransformFlagsKHR preTransform;

        if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // We prefer a non-rotated transform
        else
            preTransform = surfCaps.currentTransform;

        // Find a supported composite alpha format (not all devices support alpha opaque)
        VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        // Simply select the first composite alpha format available
        QVector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags =
        {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };

        for (auto& compositeAlphaFlag : compositeAlphaFlags)
        {
            if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag)
            {
                compositeAlpha = compositeAlphaFlag;

                break;
            };
        }

        VkSwapchainCreateInfoKHR swapchainCI = {};
        swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCI.pNext = NULL;
        swapchainCI.surface = surface;
        swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
        swapchainCI.imageFormat = colorFormat;
        swapchainCI.imageColorSpace = colorSpace;
        swapchainCI.imageExtent = { swapchainExtent.width, swapchainExtent.height };
        swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
        swapchainCI.imageArrayLayers = 1;
        swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCI.queueFamilyIndexCount = 0;
        swapchainCI.pQueueFamilyIndices = NULL;
        swapchainCI.presentMode = swapchainPresentMode;
        swapchainCI.oldSwapchain = oldSwapchain;
        // Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
        swapchainCI.clipped = VK_TRUE;
        swapchainCI.compositeAlpha = compositeAlpha;

        // Enable transfer source on swap chain images if supported
        if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        // Enable transfer destination on swap chain images if supported
        if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VK_CHECK_RESULT(fpCreateSwapchainKHR(vkManager->device, &swapchainCI, nullptr, &swapChain));

        // If an existing swap chain is re-created, destroy the old swap chain
        // This also cleans up all the presentable images
        if (oldSwapchain != VK_NULL_HANDLE)
        {
            for (uint32_t i = 0; i < imageCount; ++i)
                vkManager->deviceFuncs->vkDestroyImageView(vkManager->device, buffers[i].view, nullptr);

            fpDestroySwapchainKHR(vkManager->device, oldSwapchain, nullptr);
        }

        VK_CHECK_RESULT(fpGetSwapchainImagesKHR(vkManager->device, swapChain, &imageCount, NULL));

        // Get the swap chain images
        images.resize(imageCount);

        VK_CHECK_RESULT(fpGetSwapchainImagesKHR(vkManager->device, swapChain, &imageCount, images.data()));

        // Get the swap chain buffers containing the image and imageview
        buffers.resize(imageCount);

        for (uint32_t i = 0; i < imageCount; i++)
        {
            VkImageViewCreateInfo colorAttachmentView = {};
            colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            colorAttachmentView.pNext = NULL;
            colorAttachmentView.format = colorFormat;
            colorAttachmentView.components =
            {
                VK_COMPONENT_SWIZZLE_R,
                VK_COMPONENT_SWIZZLE_G,
                VK_COMPONENT_SWIZZLE_B,
                VK_COMPONENT_SWIZZLE_A
            };
            colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            colorAttachmentView.subresourceRange.baseMipLevel = 0;
            colorAttachmentView.subresourceRange.levelCount = 1;
            colorAttachmentView.subresourceRange.baseArrayLayer = 0;
            colorAttachmentView.subresourceRange.layerCount = 1;
            colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
            colorAttachmentView.flags = 0;

            buffers[i].image = images[i];

            colorAttachmentView.image = buffers[i].image;

            VK_CHECK_RESULT(vkManager->deviceFuncs->vkCreateImageView(vkManager->device, &colorAttachmentView, nullptr, &buffers[i].view));
        }
    }

    void SwapChain::initSurface(VkSurfaceKHR vkSurface)
    {
        surface = vkSurface;

        Vulkan::Manager* vkManager = VulkanManager;

        // Get available queue family properties
        uint32_t queueCount;

        vkManager->functions->vkGetPhysicalDeviceQueueFamilyProperties(vkManager->physicalDevice, &queueCount, NULL);

        Q_ASSERT(queueCount >= 1);

        QVector<VkQueueFamilyProperties> queueProps(queueCount);

        vkManager->functions->vkGetPhysicalDeviceQueueFamilyProperties(vkManager->physicalDevice, &queueCount, queueProps.data());

        // Iterate over each queue to learn whether it supports presenting:
        // Find a queue with present support
        // Will be used to present the swap chain images to the windowing system
        QVector<VkBool32> supportsPresent(queueCount);

        for (uint32_t i = 0; i < queueCount; ++i)
            fpGetPhysicalDeviceSurfaceSupportKHR(vkManager->physicalDevice, i, surface, &supportsPresent[i]);

        // Search for a graphics and a present queue in the array of queue
        // families, try to find one that supports both
        uint32_t graphicsQueueNodeIndex = UINT32_MAX;
        uint32_t presentQueueNodeIndex = UINT32_MAX;

        for (uint32_t i = 0; i < queueCount; i++)
        {
            if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                if (graphicsQueueNodeIndex == UINT32_MAX)
                    graphicsQueueNodeIndex = i;

                if (supportsPresent[i] == VK_TRUE)
                {
                    graphicsQueueNodeIndex = i;
                    presentQueueNodeIndex = i;

                    break;
                }
            }
        }

        if (presentQueueNodeIndex == UINT32_MAX)
        {
            // If there's no queue that supports both present and graphics
            // try to find a separate present queue
            for (uint32_t i = 0; i < queueCount; ++i)
            {
                if (supportsPresent[i] == VK_TRUE)
                {
                    presentQueueNodeIndex = i;

                    break;
                }
            }
        }

        // Exit if either a graphics or a presenting queue hasn't been found
        if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
            qFatal("Could not find a graphics and/or presenting queue!", -1);

        // todo : Add support for separate graphics and presenting queue
        if (graphicsQueueNodeIndex != presentQueueNodeIndex)
            qFatal("Separate graphics and presenting queues are not supported yet!", -1);

        queueNodeIndex = graphicsQueueNodeIndex;

        // Get list of supported surface formats
        uint32_t formatCount;

        VK_CHECK_RESULT(fpGetPhysicalDeviceSurfaceFormatsKHR(vkManager->physicalDevice, surface, &formatCount, NULL));

        Q_ASSERT(formatCount > 0);

        QVector<VkSurfaceFormatKHR> surfaceFormats(formatCount);

        VK_CHECK_RESULT(fpGetPhysicalDeviceSurfaceFormatsKHR(vkManager->physicalDevice, surface, &formatCount, surfaceFormats.data()));

        // If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
        // there is no preferered format, so we assume VK_FORMAT_B8G8R8A8_UNORM
        if (formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
        {
            colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
            colorSpace = surfaceFormats[0].colorSpace;
        }
        else
        {
            // iterate over the list of available surface format and
            // check for the presence of VK_FORMAT_B8G8R8A8_UNORM
            bool found_B8G8R8A8_UNORM = false;

            for (auto&& surfaceFormat : surfaceFormats)
            {
                if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
                {
                    colorFormat = surfaceFormat.format;
                    colorSpace = surfaceFormat.colorSpace;

                    found_B8G8R8A8_UNORM = true;

                    break;
                }
            }

            // in case VK_FORMAT_B8G8R8A8_UNORM is not available
            // select the first available color format
            if (!found_B8G8R8A8_UNORM)
            {
                colorFormat = surfaceFormats[0].format;
                colorSpace = surfaceFormats[0].colorSpace;
            }
        }
    }

    VkResult SwapChain::queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore)
    {
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain;
        presentInfo.pImageIndices = &imageIndex;

        // Check if a wait semaphore has been specified to wait for before presenting the image
        if (waitSemaphore != VK_NULL_HANDLE)
        {
            presentInfo.pWaitSemaphores = &waitSemaphore;
            presentInfo.waitSemaphoreCount = 1;
        }

        return fpQueuePresentKHR(queue, &presentInfo);
    }
}
