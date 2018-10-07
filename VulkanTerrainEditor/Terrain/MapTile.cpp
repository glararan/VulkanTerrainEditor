#include "MapTile.h"

#include "Vulkan/Initializers.h"
#include "Vulkan/Tools.h"

#include <QCoreApplication>

#define ARRAY_LENGTH(x) ((sizeof(x) / sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

MapTile::MapTile()
{
    terrainModel.rotate(-90, 1, 0, 0);
}

MapTile::~MapTile()
{
}

void MapTile::create()
{
    Vulkan::Manager* manager = VulkanManager;

    createVertexBuffers(manager);
    createUniformBuffers(manager);
    createDescriptorSetLayouts(manager);
    createPipeline(manager);
    createDescriptorPool(manager);
    createDescriptorSets(manager);
}

void MapTile::createDescriptorPool(Vulkan::Manager* vkManager)
{
    VkDescriptorPoolSize poolSizes[] =
	{
        Vulkan::Initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        //Vulkan::Initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3),
	};

    VkDescriptorPoolCreateInfo descriptorPoolInfo = Vulkan::Initializers::descriptorPoolCreateInfo(ARRAY_LENGTH(poolSizes), poolSizes, 2);

    VK_CHECK_RESULT(vkManager->deviceFuncs->vkCreateDescriptorPool(vkManager->device, &descriptorPoolInfo, nullptr, &descriptorPool));
}

void MapTile::createDescriptorSetLayouts(Vulkan::Manager* vkManager)
{
    VkDescriptorSetLayoutBinding setLayoutBindings[] =
    {
        // Binding 0 : Vertex shader uniform buffer object
        Vulkan::Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
		// Binding 0 : Shared Tessellation shader uniform buffer object
        //Vulkan::Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0),
		// Binding 1 : Height map
        //Vulkan::Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		// Binding 2 : Terrain texture array layers
        //Vulkan::Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
	};

    VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo = Vulkan::Initializers::descriptorSetLayoutCreateInfo(setLayoutBindings, ARRAY_LENGTH(setLayoutBindings));

    VK_CHECK_RESULT(vkManager->deviceFuncs->vkCreateDescriptorSetLayout(vkManager->device, &descriptorLayoutCreateInfo, nullptr, &descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Vulkan::Initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

    VK_CHECK_RESULT(vkManager->deviceFuncs->vkCreatePipelineLayout(vkManager->device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
}

void MapTile::createDescriptorSets(Vulkan::Manager* vkManager)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = Vulkan::Initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

    VK_CHECK_RESULT(vkManager->deviceFuncs->vkAllocateDescriptorSets(vkManager->device, &descriptorSetAllocateInfo, &descriptorSet));

    VkWriteDescriptorSet writeDescriptorSets[] =
	{
        // Binding 0 : Shared shader uniform buffer object
        Vulkan::Initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformTessellationBuffer.descriptor),
		// Binding 0 : Shared tessellation shader uniform buffer object
        //Vulkan::Initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformTessellationBuffer.descriptor),
		// Binding 1 : Displacement map
		//Vulkan::Initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &uniformTessellationBuffer.descriptor),
		// Binding 2 : Color map (alpha channel)
		//Vulkan::Initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &uniformTessellationBuffer.descriptor),
	};

    vkManager->deviceFuncs->vkUpdateDescriptorSets(vkManager->device, ARRAY_LENGTH(writeDescriptorSets), writeDescriptorSets, 0, NULL);
}

void MapTile::createPipeline(Vulkan::Manager* vkManager)
{
    VkDynamicState dynamicStateEnables[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        //VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = Vulkan::Initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE); // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
    VkPipelineRasterizationStateCreateInfo rasterizationState = Vulkan::Initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState = Vulkan::Initializers::pipelineColorBlendAttachmentState(0xF, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendState = Vulkan::Initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VkPipelineDepthStencilStateCreateInfo depthStencilState = Vulkan::Initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportState = Vulkan::Initializers::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleState = Vulkan::Initializers::pipelineMultisampleStateCreateInfo(vkManager->getSampleCountFlagBits());
    VkPipelineDynamicStateCreateInfo dynamicState = Vulkan::Initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables, ARRAY_LENGTH(dynamicStateEnables));

	// We render the terrain as a grid of quad patches
	VkPipelineTessellationStateCreateInfo tessellationState = Vulkan::Initializers::pipelineTessellationStateCreateInfo(4);

	// Vertex bindings an attributes
	// Binding description
    VkVertexInputBindingDescription vertexInputBindings[] =
	{
		Vulkan::Initializers::vertexInputBindingDescription(0, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX),
	};

	// Attribute descriptions
    VkVertexInputAttributeDescription vertexInputAttributes[] =
	{
		Vulkan::Initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),					// Position
		//Vulkan::Initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),	// Normal
		//Vulkan::Initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6),	// UV
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState = Vulkan::Initializers::pipelineVertexInputStateCreateInfo();
    vertexInputState.flags = 0;
    vertexInputState.pNext = nullptr;
    vertexInputState.vertexBindingDescriptionCount = ARRAY_LENGTH(vertexInputBindings);
    vertexInputState.pVertexBindingDescriptions = vertexInputBindings;
    vertexInputState.vertexAttributeDescriptionCount = ARRAY_LENGTH(vertexInputAttributes);
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes;

    if(!vertexShader.isValid())
        vertexShader.load(":/shaders/terrain.vert.spv");

    if(!fragmentShader.isValid())
        fragmentShader.load(":/shaders/terrain.frag.spv");

    VkPipelineShaderStageCreateInfo shaderStages[] =
    {
        vkManager->loadShader(vertexShader.data()->shaderModule, VK_SHADER_STAGE_VERTEX_BIT),
        vkManager->loadShader(fragmentShader.data()->shaderModule, VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = Vulkan::Initializers::pipelineCreateInfo(pipelineLayout, vkManager->renderPass, 0);

	pipelineCreateInfo.pVertexInputState = &vertexInputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
    //pipelineCreateInfo.pTessellationState = &tessellationState;
    pipelineCreateInfo.stageCount = ARRAY_LENGTH(shaderStages);
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.renderPass = vkManager->renderPass;

    VK_CHECK_RESULT(vkManager->deviceFuncs->vkCreateGraphicsPipelines(vkManager->device, vkManager->pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));
}

void MapTile::createUniformBuffers(Vulkan::Manager* vkManager)
{
    VK_CHECK_RESULT(vkManager->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformTessellationBuffer, sizeof(uniformTessellation)));

    VK_CHECK_RESULT(uniformTessellationBuffer.map());
}

void MapTile::createVertexBuffers(Vulkan::Manager* vkManager)
{
#define PATCH_SIZE 64

	const uint32_t vertexCount = PATCH_SIZE * PATCH_SIZE;

	struct Vertex
	{
		QVector3D position;
    };

	Vertex* vertices = new Vertex[vertexCount];

	const float wx = 2.0f;
	const float wy = 2.0f;

	for (auto x = 0; x < PATCH_SIZE; ++x)
	{
		for (auto y = 0; y < PATCH_SIZE; ++y)
		{
            uint32_t index = x + y * PATCH_SIZE;

            //vertices[index].position = QVector3D(x * wx + wx / 2.0f - (float)PATCH_SIZE * wx / 2.0f, 0.0f, y * wy + wy / 2.0f - (float)PATCH_SIZE * wy / 2.0f);
            vertices[index].position = QVector3D(x * wx, 0.0f, y * wy);
		}
	}

	const uint32_t w = PATCH_SIZE - 1;
    const uint32_t indexCount = w * w * 6;

    model.indexCount = indexCount;

    QVector<uint32_t> indices;

    //uint32_t* indices = new uint32_t[indexCount];

    for (auto y = 0; y < PATCH_SIZE - 1; ++y)
    {
        for (auto x = 0; x < PATCH_SIZE - 1; ++x)
        {
            // top left, bot left, bot right
            indices.append(x + y * PATCH_SIZE);
            indices.append(x + (y + 1) * PATCH_SIZE);
            indices.append((x + 1) + (y + 1) * PATCH_SIZE);

            // top left, bot right, top right
            indices.append(x + y * PATCH_SIZE);
            indices.append((x + 1) + (y + 1) * PATCH_SIZE);
            indices.append((x + 1) + y * PATCH_SIZE);
        }
    }

    /*for (auto x = 0; x < w; ++x)
	{
		for (auto y = 0; y < w; ++y)
		{
			uint32_t index = (x + y * w) * 4;



			indices[index] = x + y * PATCH_SIZE;
			indices[index + 1] = indices[index] + PATCH_SIZE;
			indices[index + 2] = indices[index + 1] + 1;
			indices[index + 3] = indices[index] + 1;
		}
    }*/

	uint32_t vertexBufferSize = vertexCount * sizeof(Vertex);
    uint32_t indexBufferSize = static_cast<uint32_t>(indices.size()) * sizeof(uint32_t);

	struct
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertexStaging, indexStaging;

    QVector<float> verticesList;

    for (auto i = 0; i < vertexCount; ++i)
    {
        verticesList.append(vertices[i].position.x());
        verticesList.append(vertices[i].position.y());
        verticesList.append(vertices[i].position.z());
    }

    vertexBufferSize = static_cast<uint32_t>(verticesList.size()) * sizeof(float);

    VK_CHECK_RESULT(vkManager->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBufferSize, &vertexStaging.buffer, &vertexStaging.memory, verticesList.data()));
    VK_CHECK_RESULT(vkManager->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBufferSize, &indexStaging.buffer, &indexStaging.memory, indices.data()));
    VK_CHECK_RESULT(vkManager->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferSize, &model.vertices.buffer, &model.vertices.memory));
    VK_CHECK_RESULT(vkManager->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBufferSize, &model.indices.buffer, &model.indices.memory));

	// Copy from staging buffers
    QVulkanDeviceFunctions* deviceFuncs = vkManager->deviceFuncs;

    VkDevice device = vkManager->device;

    VkCommandBuffer commandBuffer = vkManager->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VkBufferCopy copyRegion = {};

	copyRegion.size = vertexBufferSize;

    deviceFuncs->vkCmdCopyBuffer(commandBuffer, vertexStaging.buffer, model.vertices.buffer, 1, &copyRegion);

	copyRegion.size = indexBufferSize;

    deviceFuncs->vkCmdCopyBuffer(commandBuffer, indexStaging.buffer, model.indices.buffer, 1, &copyRegion);

    vkManager->flushCommandBuffer(commandBuffer, true);

    deviceFuncs->vkDestroyBuffer(device, vertexStaging.buffer, nullptr);
    deviceFuncs->vkFreeMemory(device, vertexStaging.memory, nullptr);
    deviceFuncs->vkDestroyBuffer(device, indexStaging.buffer, nullptr);
    deviceFuncs->vkFreeMemory(device, indexStaging.memory, nullptr);

	delete[] vertices;
    //delete[] indices;
}

void MapTile::destroy()
{
    VkDevice device = VulkanManager->device;

    QVulkanDeviceFunctions* deviceFuncs = VulkanManager->deviceFuncs;

	uniformTessellationBuffer.destroy();

    if (vertexShader.isValid())
	{
        deviceFuncs->vkDestroyShaderModule(device, vertexShader.data()->shaderModule, nullptr);

        vertexShader.reset();
	}

    if (fragmentShader.isValid())
	{
        deviceFuncs->vkDestroyShaderModule(device, fragmentShader.data()->shaderModule, nullptr);

        fragmentShader.reset();
	}

	if (pipeline)
	{
        deviceFuncs->vkDestroyPipeline(device, pipeline, nullptr);

		pipeline = VK_NULL_HANDLE;
	}

	if (pipelineLayout)
	{
        deviceFuncs->vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

		pipelineLayout = VK_NULL_HANDLE;
	}

	if (descriptorSetLayout)
	{
        deviceFuncs->vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		descriptorSetLayout = VK_NULL_HANDLE;
	}
}

void MapTile::draw(const Camera& camera, const Frustum& frustum, const bool& wireframe, const bool& tessellation)
{
    /*for(quint32 x = 0; x < CHUNKS; ++x)
    {
        for(quint32 y = 0; y < CHUNKS; ++y)
            chunks->draw(frustum, wireframe, tessellation);
    }*/

	//if (!frustum.check())
		//return;

    VkCommandBuffer commandBuffer = VulkanManager->getCommandBuffer();

    QVulkanDeviceFunctions* deviceFuncs = VulkanManager->deviceFuncs;

    updateUniformBuffers(camera);

    VkDeviceSize offset = 0;

    deviceFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    deviceFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
    deviceFuncs->vkCmdBindVertexBuffers(commandBuffer, VERTEX_BUFFER_BIND_ID, 1, &model.vertices.buffer, &offset);
    deviceFuncs->vkCmdBindIndexBuffer(commandBuffer, model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    deviceFuncs->vkCmdDrawIndexed(commandBuffer, model.indexCount, 1, 0, 0, 0);
}

void MapTile::updateUniformBuffers(const Camera& camera)
{
    uniformTessellation.mvp = camera.getViewProjectionMatrix() * terrainModel;

    memcpy(uniformTessellationBuffer.mapped, uniformTessellation.mvp.constData(), 64);
}
