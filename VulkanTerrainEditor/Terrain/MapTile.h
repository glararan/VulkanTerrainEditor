#ifndef MAPTILE_H
#define MAPTILE_H

#include <QtGlobal>
#include <QVulkanFunctions>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector4D>

#include "Editor/MapViewRenderer.h"

#include "Common/Frustum.h"
#include "Common/Shader.h"
#include "Common/Texture.h"
#include "Vulkan/Manager.h"
#include "Vulkan/Model.h"
#include "Vulkan/Buffer.h"

#include "MapChunk.h"
#include "Glm.h"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

#define CHUNKS 4

class MapTile
{
public:
	MapTile();
	~MapTile();

    void create();
    void destroy();

    void draw(const Camera& camera, const Frustum& frustum, const bool& wireframe = false, const bool& tessellation = true);

private:
    void createDescriptorPool(Vulkan::Manager* vkManager);
    void createDescriptorSetLayouts(Vulkan::Manager* vkManager);
    void createDescriptorSets(Vulkan::Manager* vkManager);
    void createPipeline(Vulkan::Manager* vkManager);
    void createUniformBuffers(Vulkan::Manager* vkManager);
    void createVertexBuffers(Vulkan::Manager* vkManager);

    void updateUniformBuffers(const Camera& camera, Frustum frustum);

    //MapChunk chunks[CHUNKS][CHUNKS];

    VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    Shader vertexShader;
    Shader tessellationControlShader;
    Shader tessellationEvaluationShader;
    Shader fragmentShader;

	Vulkan::Model model;

	Vulkan::VertexLayout vertexLayout = Vulkan::VertexLayout(
		{
			Vulkan::VERTEX_COMPONENT_POSITION,
            Vulkan::VERTEX_COMPONENT_NORMAL,
            Vulkan::VERTEX_COMPONENT_UV
		}
    );

    struct
    {
        Texture2D heightMap;
        Texture2DArray terrainArray;
    } textures;

	struct
    {
        glm::mat4x4 projection;
        glm::mat4x4 modelView;
		
        glm::vec4 lightPosition = glm::vec4(-48.0f, -40.0f, 46.0f, 0.0f);
        glm::vec4 frustumPlanes[6];

        glm::vec2 viewportDimension;

		float displacementFactor = 32.0f;
		float tessellationFactor = 0.75f;
        float tessellatedEdgeSize = 20.0f;
	} uniformTessellation;

	Vulkan::Buffer uniformTessellationBuffer;
	VkDeviceMemory uniformTesellationBufferMemory = VK_NULL_HANDLE;

    struct Heightmap
    {
    public:
        Heightmap(const QString& filename, uint32_t patchSize)
        {
            gli::texture2d heightmapTexture(gli::load(filename.toStdString().c_str()));

            dim = static_cast<uint32_t>(heightmapTexture.extent().x);
            heightData = new uint16_t[dim * dim];

            memcpy(heightData, heightmapTexture.data(), heightmapTexture.size());

            scale = dim / patchSize;
        }

        ~Heightmap()
        {
            delete[] heightData;
        }

        float getHeight(uint32_t x, uint32_t y)
        {
            glm::ivec2 rpos = glm::ivec2(x, y) * glm::ivec2(scale);
            rpos.x = qMax(0, std::min(rpos.x, (int)dim-1));
            rpos.y = qMax(0, std::min(rpos.y, (int)dim-1));
            rpos /= glm::ivec2(scale);

            return *(heightData + (rpos.x + rpos.y * dim) * scale) / 65535.0f;
        }

    private:
        uint16_t* heightData;
        uint32_t dim;
        uint32_t scale;
    };
};

#endif // MAPTILE_H
