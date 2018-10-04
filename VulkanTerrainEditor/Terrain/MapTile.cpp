#include "MapTile.h"

#include "Vulkan/Initializers.h"
#include "Vulkan/Tools.h"

#include <QCoreApplication>

MapTile::MapTile()
{
}


MapTile::~MapTile()
{
}

void MapTile::create(Vulkan::Manager& vkManager)
{
	createVertexBuffers(vkManager);
    createUniformBuffers(vkManager);
    createDescriptorSetLayouts(vkManager);
	createPipeline(vkManager);
    createDescriptorPool(vkManager);
    createDescriptorSets(vkManager);
}

void MapTile::createDescriptorPool(Vulkan::Manager& vkManager)
{
	QVector<VkDescriptorPoolSize> poolSizes =
	{
        Vulkan::Initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        //Vulkan::Initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3),
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo = Vulkan::Initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 2);

	VK_CHECK_RESULT(vkManager.deviceFuncs->vkCreateDescriptorPool(vkManager.device, &descriptorPoolInfo, nullptr, &descriptorPool));
}

void MapTile::createDescriptorSetLayouts(Vulkan::Manager& vkManager)
{
	VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo;
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;

	QVector<VkDescriptorSetLayoutBinding> setLayoutBindings;

	setLayoutBindings =
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

	descriptorLayoutCreateInfo = Vulkan::Initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkManager.deviceFuncs->vkCreateDescriptorSetLayout(vkManager.device, &descriptorLayoutCreateInfo, nullptr, &descriptorSetLayout));

	pipelineLayoutCreateInfo = Vulkan::Initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

	VK_CHECK_RESULT(vkManager.deviceFuncs->vkCreatePipelineLayout(vkManager.device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
}

void MapTile::createDescriptorSets(Vulkan::Manager& vkManager)
{
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;

	QVector<VkWriteDescriptorSet> writeDescriptorSets;

	descriptorSetAllocateInfo = Vulkan::Initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

	VK_CHECK_RESULT(vkManager.deviceFuncs->vkAllocateDescriptorSets(vkManager.device, &descriptorSetAllocateInfo, &descriptorSet));

	writeDescriptorSets = 
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

	vkManager.deviceFuncs->vkUpdateDescriptorSets(vkManager.device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void MapTile::createPipeline(Vulkan::Manager& vkManager)
{
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = Vulkan::Initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE); // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST

	VkPipelineRasterizationStateCreateInfo rasterizationState = Vulkan::Initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);

	VkPipelineColorBlendAttachmentState blendAttachmentState = Vulkan::Initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendState = Vulkan::Initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VkPipelineDepthStencilStateCreateInfo depthStencilState = Vulkan::Initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportState = Vulkan::Initializers::pipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleState = Vulkan::Initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);

	QVector<VkDynamicState> dynamicStateEnables =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicState = Vulkan::Initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

	// We render the terrain as a grid of quad patches
	VkPipelineTessellationStateCreateInfo tessellationState = Vulkan::Initializers::pipelineTessellationStateCreateInfo(4);

	// Vertex bindings an attributes
	// Binding description
	QVector<VkVertexInputBindingDescription> vertexInputBindings =
	{
		Vulkan::Initializers::vertexInputBindingDescription(0, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX),
	};

	// Attribute descriptions
	QVector<VkVertexInputAttributeDescription> vertexInputAttributes =
	{
		Vulkan::Initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),					// Position
		//Vulkan::Initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),	// Normal
		//Vulkan::Initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6),	// UV
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState = Vulkan::Initializers::pipelineVertexInputStateCreateInfo();
	vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
	vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
	vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
	vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

    QVarLengthArray<VkPipelineShaderStageCreateInfo, 2> shaderStages(2);

    vertexShader = vkManager.createShader(":/shaders/terrain.vert.spv");
    fragmentShader = vkManager.createShader(":/shaders/terrain.frag.spv");

	shaderStages[0] = vkManager.loadShader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = vkManager.loadShader(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = Vulkan::Initializers::pipelineCreateInfo(pipelineLayout, vkManager.renderPass, 0);

	pipelineCreateInfo.pVertexInputState = &vertexInputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.pTessellationState = &tessellationState;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.renderPass = vkManager.renderPass;

    VK_CHECK_RESULT(vkManager.deviceFuncs->vkCreateGraphicsPipelines(vkManager.device, vkManager.pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));
}

void MapTile::createUniformBuffers(Vulkan::Manager& vkManager)
{
	VK_CHECK_RESULT(vkManager.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformTessellationBuffer, sizeof(uniformTessellation)));

    VK_CHECK_RESULT(uniformTessellationBuffer.map());
}

void MapTile::createVertexBuffers(Vulkan::Manager& vkManager)
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

    VK_CHECK_RESULT(vkManager.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBufferSize, &vertexStaging.buffer, &vertexStaging.memory, verticesList.data()));
    VK_CHECK_RESULT(vkManager.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBufferSize, &indexStaging.buffer, &indexStaging.memory, indices.data()));
	VK_CHECK_RESULT(vkManager.createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferSize, &model.vertices.buffer, &model.vertices.memory));
	VK_CHECK_RESULT(vkManager.createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBufferSize, &model.indices.buffer, &model.indices.memory));

	// Copy from staging buffers
    VkCommandBuffer copyCommandBuffer = vkManager.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VkBufferCopy copyRegion = {};

	copyRegion.size = vertexBufferSize;

	vkManager.deviceFuncs->vkCmdCopyBuffer(copyCommandBuffer, vertexStaging.buffer, model.vertices.buffer, 1, &copyRegion);

	copyRegion.size = indexBufferSize;

	vkManager.deviceFuncs->vkCmdCopyBuffer(copyCommandBuffer, indexStaging.buffer, model.indices.buffer, 1, &copyRegion);

    vkManager.flushCommandBuffer(copyCommandBuffer, true);

	vkManager.deviceFuncs->vkDestroyBuffer(vkManager.device, vertexStaging.buffer, nullptr);
	vkManager.deviceFuncs->vkFreeMemory(vkManager.device, vertexStaging.memory, nullptr);
	vkManager.deviceFuncs->vkDestroyBuffer(vkManager.device, indexStaging.buffer, nullptr);
	vkManager.deviceFuncs->vkFreeMemory(vkManager.device, indexStaging.memory, nullptr);

	delete[] vertices;
    //delete[] indices;
}

void MapTile::destroy(Vulkan::Manager& vkManager)
{
	uniformTessellationBuffer.destroy();

	if (vertexShader)
	{
		vkManager.deviceFuncs->vkDestroyShaderModule(vkManager.device, vertexShader, nullptr);

        vertexShader = VK_NULL_HANDLE;
	}

	if (fragmentShader)
	{
		vkManager.deviceFuncs->vkDestroyShaderModule(vkManager.device, fragmentShader, nullptr);

		fragmentShader = VK_NULL_HANDLE;
	}

	if (pipeline)
	{
		vkManager.deviceFuncs->vkDestroyPipeline(vkManager.device, pipeline, nullptr);

		pipeline = VK_NULL_HANDLE;
	}

	if (pipelineLayout)
	{
		vkManager.deviceFuncs->vkDestroyPipelineLayout(vkManager.device, pipelineLayout, nullptr);

		pipelineLayout = VK_NULL_HANDLE;
	}

	if (descriptorSetLayout)
	{
		vkManager.deviceFuncs->vkDestroyDescriptorSetLayout(vkManager.device, descriptorSetLayout, nullptr);

		descriptorSetLayout = VK_NULL_HANDLE;
	}
}

void MapTile::draw(const VkCommandBuffer commandBuffer, QVulkanDeviceFunctions* deviceFuncs, const Camera& camera, const Frustum& frustum, const bool& wireframe, const bool& tessellation)
{
    /*for(quint32 x = 0; x < CHUNKS; ++x)
    {
        for(quint32 y = 0; y < CHUNKS; ++y)
            chunks->draw(frustum, wireframe, tessellation);
    }*/

	//if (!frustum.check())
		//return;

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
    QMatrix4x4 model;
    model.setToIdentity();

    uniformTessellation.mvp = camera.getViewProjectionMatrix() * model;

    memcpy(uniformTessellationBuffer.mapped, uniformTessellation.mvp.constData(), 4 * 4 * sizeof(float));
}
