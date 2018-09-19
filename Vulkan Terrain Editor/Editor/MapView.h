#ifndef MAPVIEW_H
#define MAPVIEW_H

#include <QVulkanWindow>

#include "Common/Camera.h"

#include "Vulkan/Device.h"
#include "Vulkan/SwapChain.h"

#include "World.h"

class MapViewRenderer : public QVulkanWindowRenderer
{
	Q_OBJECT

public:
	MapViewRenderer(MapView* parent, bool msaa = false);
    ~MapViewRenderer();

	void initResources() override;
	void initSwapChainResources() override;
	void releaseSwapChainResources() override;
	void releaseResources() override;

	void startNextFrame() override;

protected:
	VkShaderModule createShader(const QString& name);

private:
    void buildCommandBuffers();
    void rebuildCommandBuffers();

    VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, const bool& begin);
    void createCommandBuffers();
    void createCommandPool();

    void destroyCommandBuffers();

    void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, const bool& free);

    bool checkCommandBuffers() const;

    void initQueryResultBuffer();

    void getQueryResults();

    // Pipeline statistics
    struct
    {
        VkBuffer buffer;
        VkDeviceMemory memory;
    } queryResult;

	struct Pipelines
	{
		VkPipeline terrain;
		VkPipeline wireframe = VK_NULL_HANDLE;
		VkPipeline skySphere;
	} pipelines;

	struct
	{
		VkDescriptorSetLayout terrain;
		VkDescriptorSetLayout skySphere;
	} descriptorSetLayouts;

	struct
	{
		VkPipelineLayout terrain;
		VkPipelineLayout skySphere;
	} pipelineLayouts;

	struct
	{
		VkDescriptorSet terrain;
		VkDescriptorSet skySphere;
	} descriptorSets;

	MapView* window = Q_NULLPTR;

    Vulkan::VulkanDevice* device;

	QVulkanDeviceFunctions* deviceFuncs = Q_NULLPTR;

    VkPhysicalDeviceFeatures enabledFeatures {};
    VkQueryPool queryPool = VK_NULL_HANDLE;

	VkRenderPass renderPass;

	VkCommandPool cmdPool;

	Vulkan::SwapChain swapChain;

	QVector<VkCommandBuffer> drawCmdBuffers;
	QVector<VkFramebuffer> frameBuffers;

    quint64 pipelineStats[2] = { 0 };
};

class MapView : public QVulkanWindow
{
	Q_OBJECT

public:
	MapView();
	~MapView();

	QVulkanWindowRenderer* createRenderer() override;

    Camera* getCamera() const { return &camera; }
	World* getWorld() const { return world; }

private:
	MapViewRenderer* renderer = Q_NULLPTR;

    World* world = Q_NULLPTR;

	Camera camera;

	float fpsTimer = 0.0f;

	quint32 frameCounter = 0;
	quint32 lastFPS = 0;


};

#endif // MAPVIEW_H
