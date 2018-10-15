#ifndef VULKANWINDOW_H
#define VULKANWINDOW_H

#include <QWindow>
#include <QVulkanInstance>

#include "Vulkan/SwapChain.h"

#include "Common/Camera.h"

class VulkanWindow : public QWindow
{
    Q_OBJECT

public:
    VulkanWindow(QVulkanInstance* vulkanInstance);
    ~VulkanWindow();

    struct
    {
        bool validation = true;
    } settings;

    QVulkanInstance* getVulkanInstance() { return instance; }

    Camera* getCamera() { return &camera; }

    uint32_t getFPS() const { return lastFPS; }

    VkPhysicalDevice getPhysicalDevice() { return physicalDevice; }
    VkRenderPass getRenderPass() { return renderPass; }
    VkQueue getQueue() { return queue; }
    VkCommandPool getCommandPool() { return commandPool; }

    QVector<VkCommandBuffer> getCommandBuffers() const { return drawCmdBuffers; }
    QVector<VkFramebuffer> getFrameBuffers() const { return frameBuffers; }

protected:
    virtual void buildCommandBuffers();
    virtual void initializeResources();
    virtual void releaseResources();
    virtual void prepareFrame();
    virtual void submitFrame();
    virtual void render();

    QVulkanInstance* instance;

    // Surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // Device
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;

    // Vulkan
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkFormat depthFormat;
    VkSubmitInfo submitInfo;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;

    QVector<VkCommandBuffer> drawCmdBuffers;
    QVector<VkFence> waitFences;
    QVector<VkFramebuffer>frameBuffers;

    VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    Vulkan::SwapChain swapChain;

    struct
    {
        // Swap chain image presentation
        VkSemaphore presentComplete;
        // Command buffer submission and execution
        VkSemaphore renderComplete;
    } semaphores;

    struct
    {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    } depthStencil;

    // Functions
    QVulkanFunctions* vkFunctions;
    QVulkanDeviceFunctions* vkDevFunctions;

    // Debug
    PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = VK_NULL_HANDLE;
    PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = VK_NULL_HANDLE;
    PFN_vkDebugReportMessageEXT debugBreakCallback = VK_NULL_HANDLE;

    VkDebugReportCallbackEXT messageCallback;

    // Initialize
    bool initialized = false;

    uint32_t currentBuffer = 0;

    // fps
    uint32_t frameCounter = 0;
    uint32_t lastFPS = 0;

    float fpsTimer = 0.0f;
    float frameTimer = 1.0f;

    bool prepared = false;

    Camera camera;

private:
    void exposeEvent(QExposeEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;

    void createCommandPool();
    void createCommandBuffers();
    void createSynchronizationPrimitives();
    void createPipelineCache();
    void destroyCommandBuffers();
    void initialize();
    void setupSwapChain();
    void setupDebugging();
    void setupDepthStencil();
    void setupFramebuffer();
    void setupRenderPass();
    void releaseSwapChain();
    void windowResize();

    // Render
    void renderFrame();

};

#endif // VULKANWINDOW_H
