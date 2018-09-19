#ifndef MAPTILE_H
#define MAPTILE_H

#include <QtGlobal>

#include "Common/Frustum.h"
#include "Vulkan/Buffer.h"

#include "MapChunk.h"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

#define CHUNKS 4

class MapTile
{
public:
	struct
	{
		vks::Texture2D heightMap;
		vks::Texture2D skySphere;
		vks::Texture2DArray terrainArray;
	} textures;

	// Vertex layout for the models
	vks::VertexLayout vertexLayout = vks::VertexLayout(
	{
		vks::VERTEX_COMPONENT_POSITION,
		vks::VERTEX_COMPONENT_NORMAL,
		vks::VERTEX_COMPONENT_UV,
	});

	struct
	{
		vks::Model terrain;
		vks::Model skysphere;
	} models;

	struct
	{
        Vulkan::Buffer terrainTessellation;
        Vulkan::Buffer skysphereVertex;
	} uniformBuffers;

	// Shared values for tessellation control and evaluation stages
	struct
	{
		glm::mat4 projection;
		glm::mat4 modelview;
		glm::vec4 lightPos = glm::vec4(-48.0f, -40.0f, 46.0f, 0.0f);
		glm::vec4 frustumPlanes[6];
		float displacementFactor = 32.0f;
		float tessellationFactor = 0.75f;
		glm::vec2 viewportDim;
		// Desired size of tessellated quad patch edge
		float tessellatedEdgeSize = 20.0f;
	} uboTess;

	// Skysphere vertex shader stage
	struct
	{
		glm::mat4 mvp;
	} uboVS;

	struct Pipelines
	{
		VkPipeline terrain;
		VkPipeline wireframe = VK_NULL_HANDLE;
		VkPipeline skysphere;
	} pipelines;

	struct
	{
		VkDescriptorSetLayout terrain;
		VkDescriptorSetLayout skysphere;
	} descriptorSetLayouts;

	struct
	{
		VkPipelineLayout terrain;
		VkPipelineLayout skysphere;
	} pipelineLayouts;

	struct
	{
		VkDescriptorSet terrain;
		VkDescriptorSet skysphere;
    } descriptorSets;

	MapTile();
	~MapTile();

    void draw(const Frustum& frustum, const bool& wireframe = false, const bool& tessellation = true);

private:
    MapChunk chunks[CHUNKS][CHUNKS];
};

#endif // MAPTILE_H
