#ifndef MAPTILE_H
#define MAPTILE_H

#include <QtGlobal>
#include <QVulkanFunctions>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector4D>

#include "Editor/MapViewRenderer.h"

#include "Common/Frustum.h"
#include "Vulkan/Manager.h"
#include "Vulkan/Model.h"
#include "Vulkan/Buffer.h"

#include "MapChunk.h"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

#define CHUNKS 4

class MapTile
{
public:
	MapTile();
	~MapTile();

	void create(Vulkan::Manager& vkManager);
	void destroy(Vulkan::Manager& vkManager);

    void draw(const VkCommandBuffer commandBuffer, QVulkanDeviceFunctions* deviceFuncs, const Camera& camera, const Frustum& frustum, const bool& wireframe = false, const bool& tessellation = true);

private:
	void createDescriptorPool(Vulkan::Manager& vkManager);
	void createDescriptorSetLayouts(Vulkan::Manager& vkManager);
	void createDescriptorSets(Vulkan::Manager& vkManager);
	void createPipeline(Vulkan::Manager& vkManager);
    void createPipelineCache(Vulkan::Manager& vkManager);
	void createUniformBuffers(Vulkan::Manager& vkManager);
	void createVertexBuffers(Vulkan::Manager& vkManager);

    void updateUniformBuffers(const Camera& camera);

    //MapChunk chunks[CHUNKS][CHUNKS];

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineCache pipelineCache = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

	VkShaderModule vertexShader = VK_NULL_HANDLE;
	VkShaderModule fragmentShader = VK_NULL_HANDLE;

	Vulkan::Model model;

	Vulkan::VertexLayout vertexLayout = Vulkan::VertexLayout(
		{
			Vulkan::VERTEX_COMPONENT_POSITION,
			//Vulkan::VERTEX_COMPONENT_NORMAL,
			//Vulkan::VERTEX_COMPONENT_UV
		}
	);

	struct
	{
        QMatrix4x4 mvp;
        /*QMatrix4x4 projection;
		QMatrix4x4 modelView;
		
		QVector4D lightPosition = QVector4D(-48.0f, -40.0f, 46.0f, 0.0f);
		QVector4D frustumPlanes[6];

		QVector2D viewportDimension;

		float displacementFactor = 32.0f;
		float tessellationFactor = 0.75f;
        float tessellatedEdgeSize = 20.0f;*/
	} uniformTessellation;

	Vulkan::Buffer uniformTessellationBuffer;
	VkDeviceMemory uniformTesellationBufferMemory = VK_NULL_HANDLE;
};

#endif // MAPTILE_H
