#include "VulkanWindow.h"

#include <QCoreApplication>
#include <QVulkanFunctions>
#include <QPlatformSurfaceEvent>
#include <QFile>
#include <QFileInfo>
#include <QDir>

#include "Vulkan/Manager.h"
#include "Vulkan/Tools.h"
#include "Vulkan/Initializers.h"

VKAPI_ATTR VkBool32 VKAPI_CALL messageCallbackFunc(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char* pLayerPrefix, const char* pMsg, void* pUserData)
{
    // Select prefix depending on flags passed to the callback
    // Note that multiple flags may be set for a single validation message
    QString prefix("");

    // Error that may result in undefined behaviour
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        prefix += "ERROR:";

    // Warnings may hint at unexpected / non-spec API usage
    if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
        prefix += "WARNING:";

    // May indicate sub-optimal usage of the API
    if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
        prefix += "PERFORMANCE:";

    // Informal messages that may become handy during debugging
    if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
        prefix += "INFO:";

    // Diagnostic info from the Vulkan loader and layers
    // Usually not helpful in terms of API usage, but may help to debug layer and loader problems
    if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
        prefix += "DEBUG:";

    qDebug() << prefix << " [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg;

    // The return value of this callback controls wether the Vulkan call that caused
    // the validation message will be aborted or not
    // We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message
    // (and return a VkResult) to abort
    // If you instead want to have calls abort, pass in VK_TRUE and the function will
    // return VK_ERROR_VALIDATION_FAILED_EXT
    return VK_FALSE;
}

VulkanWindow::VulkanWindow(QVulkanInstance* vulkanInstance) : QWindow(), instance(vulkanInstance)
{
    setSurfaceType(VulkanSurface);
    setVulkanInstance(vulkanInstance);
}

VulkanWindow::~VulkanWindow()
{
    releaseResources();
}

void VulkanWindow::exposeEvent(QExposeEvent* event)
{
    if(isExposed() && !initialized)
    {
        qDebug() << "Initializing VulkanWindow";

        initialize();
        swapChain.initSurface(surface);
        createCommandPool();
        setupSwapChain();
        createCommandBuffers();
        createSynchronizationPrimitives();
        setupDepthStencil();
        setupRenderPass();
        createPipelineCache();
        setupFramebuffer();
        initializeResources();

        initialized = prepared = true;

        requestUpdate();
    }

    if(!isExposed() && initialized)
    {
        initialized = false;

        releaseResources();
    }
}

void VulkanWindow::resizeEvent(QResizeEvent* event)
{
    VulkanManager->viewportSize = size();
}

bool VulkanWindow::event(QEvent* event)
{
    switch(event->type())
    {
    case QEvent::UpdateRequest:
        renderFrame();
        break;

    case QEvent::PlatformSurface:
        if (static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            releaseSwapChain();
        break;

    default:
        break;
    }

    return QWindow::event(event);
}

void VulkanWindow::createCommandPool()
{
    Vulkan::Manager* vkManager = VulkanManager;

    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK_RESULT(vkManager->deviceFuncs->vkCreateCommandPool(vkManager->device, &cmdPoolInfo, nullptr, &commandPool));
}

void VulkanWindow::createCommandBuffers()
{
    // Create one command buffer for each swap chain image and reuse for rendering
    drawCmdBuffers.resize(swapChain.imageCount);

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = Vulkan::Initializers::commandBufferAllocateInfo(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(drawCmdBuffers.size()));

    VK_CHECK_RESULT(vkDevFunctions->vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));
}

void VulkanWindow::createSynchronizationPrimitives()
{
    // Wait fences to sync command buffer access
    VkFenceCreateInfo fenceCreateInfo = Vulkan::Initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);

    waitFences.resize(drawCmdBuffers.size());

    for (auto& fence : waitFences)
        VK_CHECK_RESULT(vkDevFunctions->vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
}

void VulkanWindow::createPipelineCache()
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    VK_CHECK_RESULT(vkDevFunctions->vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanWindow::destroyCommandBuffers()
{
    vkDevFunctions->vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

void VulkanWindow::initialize()
{
    surface = instance->surfaceForWindow(this);

    if(!surface)
        qFatal("Failed to get surface for window");

    vkFunctions = instance->functions();

    if(settings.validation)
        setupDebugging();

    uint32_t gpuCount = 0;

    VK_CHECK_RESULT(vkFunctions->vkEnumeratePhysicalDevices(instance->vkInstance(), &gpuCount, nullptr));

    Q_ASSERT(gpuCount > 0);

    VK_CHECK_RESULT(vkFunctions->vkEnumeratePhysicalDevices(instance->vkInstance(), &gpuCount, &physicalDevice));

    vkFunctions->vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
    vkFunctions->vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);
    vkFunctions->vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

    qDebug("Device name: %s Driver version: %d.%d.%d", physicalDeviceProperties.deviceName, VK_VERSION_MAJOR(physicalDeviceProperties.driverVersion), VK_VERSION_MINOR(physicalDeviceProperties.driverVersion), VK_VERSION_PATCH(physicalDeviceProperties.driverVersion));

    Vulkan::Manager* vkManager = VulkanManager;
    vkManager->viewportSize = size();
    vkManager->initialize(this);

    device = VulkanManager->device;

    vkDevFunctions = instance->deviceFunctions(device);

    // Get a graphics queue from the device
    vkDevFunctions->vkGetDeviceQueue(device, vkManager->queueFamilyIndices.graphics, 0, &queue);

    VkBool32 validDepthFormat = Vulkan::Tools::getSupportedDepthFormat(&depthFormat);

    Q_ASSERT(validDepthFormat);

    swapChain.connect();

    // Create synchronization objects
    VkSemaphoreCreateInfo semaphoreCreateInfo = Vulkan::Initializers::semaphoreCreateInfo();
    // Create a semaphore used to synchronize image presentation
    // Ensures that the image is displayed before we start submitting new commands to the queu
    VK_CHECK_RESULT(vkDevFunctions->vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
    // Create a semaphore used to synchronize command submission
    // Ensures that the image is not presented until all commands have been sumbitted and executed
    VK_CHECK_RESULT(vkDevFunctions->vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));

    // Set up submit info structure
    // Semaphores will stay the same during application lifetime
    // Command buffer submission info is set by each example
    submitInfo = Vulkan::Initializers::submitInfo();
    submitInfo.pWaitDstStageMask = &submitPipelineStages;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &semaphores.presentComplete;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &semaphores.renderComplete;
}

void VulkanWindow::initializeResources()
{
}

void VulkanWindow::setupSwapChain()
{
    swapChain.create(size(), false);
}

void VulkanWindow::setupDebugging()
{
    CreateDebugReportCallback = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(instance->getInstanceProcAddr("vkCreateDebugReportCallbackEXT"));
    DestroyDebugReportCallback = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(instance->getInstanceProcAddr("vkDestroyDebugReportCallbackEXT"));
    debugBreakCallback = reinterpret_cast<PFN_vkDebugReportMessageEXT>(instance->getInstanceProcAddr("vkDebugReportMessageEXT"));

    VkDebugReportCallbackCreateInfoEXT debugCreateInfo = {};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    debugCreateInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)messageCallbackFunc;
    debugCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;

    VK_CHECK_RESULT(CreateDebugReportCallback(instance->vkInstance(), &debugCreateInfo, nullptr, &messageCallback));
}

void VulkanWindow::setupDepthStencil()
{
    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = depthFormat;
    image.extent = { static_cast<uint32_t>(width()), static_cast<uint32_t>(height()), 1 };
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image.flags = 0;

    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.pNext = NULL;
    mem_alloc.allocationSize = 0;
    mem_alloc.memoryTypeIndex = 0;

    VkImageViewCreateInfo depthStencilView = {};
    depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthStencilView.pNext = NULL;
    depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthStencilView.format = depthFormat;
    depthStencilView.flags = 0;
    depthStencilView.subresourceRange = {};
    depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    depthStencilView.subresourceRange.baseMipLevel = 0;
    depthStencilView.subresourceRange.levelCount = 1;
    depthStencilView.subresourceRange.baseArrayLayer = 0;
    depthStencilView.subresourceRange.layerCount = 1;

    VkMemoryRequirements memReqs;

    VK_CHECK_RESULT(vkDevFunctions->vkCreateImage(device, &image, nullptr, &depthStencil.image));

    vkDevFunctions->vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);

    mem_alloc.allocationSize = memReqs.size;
    mem_alloc.memoryTypeIndex = VulkanManager->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK_RESULT(vkDevFunctions->vkAllocateMemory(device, &mem_alloc, nullptr, &depthStencil.mem));
    VK_CHECK_RESULT(vkDevFunctions->vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

    depthStencilView.image = depthStencil.image;

    VK_CHECK_RESULT(vkDevFunctions->vkCreateImageView(device, &depthStencilView, nullptr, &depthStencil.view));
}

void VulkanWindow::setupFramebuffer()
{
    VkImageView attachments[2];

    // Depth/Stencil attachment is the same for all frame buffers
    attachments[1] = depthStencil.view;

    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.pNext = NULL;
    frameBufferCreateInfo.renderPass = renderPass;
    frameBufferCreateInfo.attachmentCount = 2;
    frameBufferCreateInfo.pAttachments = attachments;
    frameBufferCreateInfo.width = width();
    frameBufferCreateInfo.height = height();
    frameBufferCreateInfo.layers = 1;

    // Create frame buffers for every swap chain image
    frameBuffers.resize(swapChain.imageCount);

    for (uint32_t i = 0; i < frameBuffers.size(); ++i)
    {
        attachments[0] = swapChain.buffers[i].view;

        VK_CHECK_RESULT(vkDevFunctions->vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
    }
}

void VulkanWindow::setupRenderPass()
{
    VkAttachmentDescription attachments[2];

    // Color attachment
    attachments[0].format = swapChain.colorFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // Depth attachment
    attachments[1].format = depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;
    subpassDescription.pDepthStencilAttachment = &depthReference;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;
    subpassDescription.pResolveAttachments = nullptr;

    // Subpass dependencies for layout transitions
    VkSubpassDependency dependencies[2];

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies;

    VK_CHECK_RESULT(vkDevFunctions->vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}

void VulkanWindow::releaseResources()
{
    prepared = false;

    if (pipelineCache)
    {
        vkDevFunctions->vkDestroyPipelineCache(device, pipelineCache, nullptr);

        pipelineCache = VK_NULL_HANDLE;
    }

    if(messageCallback != VK_NULL_HANDLE)
        DestroyDebugReportCallback(instance->vkInstance(), messageCallback, nullptr);
}

void VulkanWindow::releaseSwapChain()
{
    if(!device)
        return;

    // Flush device to make sure all resources can be freed
    vkDevFunctions->vkDeviceWaitIdle(device);

    setupSwapChain();

    // Recreate the frame buffers
    vkDevFunctions->vkDestroyImageView(device, depthStencil.view, nullptr);
    vkDevFunctions->vkDestroyImage(device, depthStencil.image, nullptr);
    vkDevFunctions->vkFreeMemory(device, depthStencil.mem, nullptr);

    setupDepthStencil();

    for (uint32_t i = 0; i < frameBuffers.size(); ++i)
        vkDevFunctions->vkDestroyFramebuffer(device, frameBuffers[i], nullptr);

    setupFramebuffer();

    // Command buffers need to be recreated as they may store
    // references to the recreated frame buffer
    destroyCommandBuffers();
    createCommandBuffers();
    buildCommandBuffers();

    vkDevFunctions->vkDeviceWaitIdle(device);

    if (width() > 0.0f && height() > 0.0f)
        camera.setAspectRatio((float)width() / (float)height());
}

// Runtime
void VulkanWindow::buildCommandBuffers()
{

}

void VulkanWindow::prepareFrame()
{
    // Acquire the next image from the swap chain
    VkResult error = swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer);

    // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
    if (error == VK_ERROR_OUT_OF_DATE_KHR || error == VK_SUBOPTIMAL_KHR)
        windowResize();
    else
        VK_CHECK_RESULT(error);
}

void VulkanWindow::submitFrame()
{
    VkResult result = swapChain.queuePresent(queue, currentBuffer, semaphores.renderComplete);

    if (!(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR))
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // Swap chain is no longer compatible with the surface and needs to be recreated
            windowResize();

            return;
        }
        else
            VK_CHECK_RESULT(result);
    }

    VK_CHECK_RESULT(vkDevFunctions->vkQueueWaitIdle(queue));
}

void VulkanWindow::renderFrame()
{
    auto tStart = std::chrono::high_resolution_clock::now();

    if(prepared)
        render();

    frameCounter++;

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    frameTimer = (float)tDiff / 1000.0f;
    fpsTimer += (float)tDiff;

    if (fpsTimer > 1000.0f)
    {
        lastFPS = static_cast<uint32_t>((float)frameCounter * (1000.0f / fpsTimer));

        fpsTimer = 0.0f;
        frameCounter = 0;
    }

    requestUpdate();
}

void VulkanWindow::render()
{
}

void VulkanWindow::windowResize()
{
    if(!prepared)
        return;

    prepared = false;

    releaseSwapChain();

    prepared = true;
}
