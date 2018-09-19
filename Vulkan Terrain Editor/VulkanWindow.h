#ifndef VULKANWINDOW_H
#define VULKANWINDOW_H

#include <QVulkanWindow>

class VulkanRenderer : public QVulkanWindowRenderer
{
public:
	VulkanRenderer(QVulkanWindow* vulkanWindow, bool msaa = false);

	void initResources() override;
	void initSwapChainResources() override;
	void releaseSwapChainResources() override;
	void releaseResources() override;

	void startNextFrame() override;

protected:
	VkShaderModule createShader(const QString& name);

	QVulkanWindow* window;
	QVulkanDeviceFunctions* deviceFuncs;

	VkDeviceMemory memBuffer = VK_NULL_HANDLE;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDescriptorBufferInfo uniformBufInfo[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT];

	VkDescriptorPool descPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet m_descSet[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT];

	VkPipelineCache pipelineCache = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	QMatrix4x4 projMat;

	float rotation = 0.0f;
};

class VulkanWindow : public QVulkanWindow
{
public:
	QVulkanWindowRenderer* createRenderer() override;
};

#endif // VULKANWINDOW_H