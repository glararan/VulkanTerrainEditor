#include "MapTile.h"

#include "VulkanWindow.h"

#include "Vulkan/Initializers.h"
#include "Vulkan/Tools.h"

#include <QCoreApplication>

#define ARRAY_LENGTH(x) ((sizeof(x) / sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

MapTile::MapTile()
{
}

MapTile::~MapTile()
{
}

void MapTile::buildCommandBuffers()
{
    Vulkan::Manager* vkManager = VulkanManager;

    VulkanWindow* window = vkManager->getWindow();

    QVulkanDeviceFunctions* functions = vkManager->deviceFuncs;

    VkCommandBufferBeginInfo cmdBufInfo = Vulkan::Initializers::commandBufferBeginInfo();

    VkClearValue clearValues[2];
    clearValues[0].color = vkManager->defaultClearColor;
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = Vulkan::Initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = window->getRenderPass();
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = static_cast<uint32_t>(window->width());
    renderPassBeginInfo.renderArea.extent.height = static_cast<uint32_t>(window->height());
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    QVector<VkCommandBuffer> commandBuffers = window->getCommandBuffers();
    QVector<VkFramebuffer> frameBuffers = window->getFrameBuffers();

    for (auto commandBuffer : commandBuffers)
    {
        renderPassBeginInfo.framebuffer = frameBuffers[commandBuffers.indexOf(commandBuffer)];

        VK_CHECK_RESULT(functions->vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));

        functions->vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = Vulkan::Initializers::viewport((float)window->width(), (float)window->height(), 0.0f, 1.0f);
        functions->vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = Vulkan::Initializers::rect2D(window->width(), window->height(), 0, 0);
        functions->vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        functions->vkCmdSetLineWidth(commandBuffer, 1.0f);

        VkDeviceSize offsets[1] = { 0 };

        // Render
        functions->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        functions->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
        functions->vkCmdBindVertexBuffers(commandBuffer, VERTEX_BUFFER_BIND_ID, 1, &model.vertices.buffer, offsets);
        functions->vkCmdBindIndexBuffer(commandBuffer, model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
        functions->vkCmdDrawIndexed(commandBuffer, model.indexCount, 1, 0, 0, 0);

        functions->vkCmdEndRenderPass(commandBuffer);

        VK_CHECK_RESULT(functions->vkEndCommandBuffer(commandBuffer));
    }
}

void MapTile::create()
{
    Vulkan::Manager* manager = VulkanManager;

    // load assets
    // Textures
    QString texFormatSuffix;

    VkFormat texFormat;

    // Get supported compressed texture format
    if (manager->deviceFeatures.textureCompressionBC)
    {
        texFormatSuffix = "_bc3_unorm";
        texFormat = VK_FORMAT_BC3_UNORM_BLOCK;
    }
    else
        qFatal("Device does not support any compressed texture format!");

    textures.heightMap.loadFromFile(QCoreApplication::applicationDirPath() + "/heightmap.ktx", VK_FORMAT_R16_UNORM);
    textures.terrainArray.loadFromFile(QCoreApplication::applicationDirPath() + "/texturearray" + texFormatSuffix + ".ktx", texFormat);

    VkSamplerCreateInfo samplerInfo = Vulkan::Initializers::samplerCreateInfo();

    // Setup a mirroring sampler for the height map
    manager->deviceFuncs->vkDestroySampler(manager->device, textures.heightMap.sampler, nullptr);

    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = (float)textures.heightMap.mipLevels;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VK_CHECK_RESULT(manager->deviceFuncs->vkCreateSampler(manager->device, &samplerInfo, nullptr, &textures.heightMap.sampler));

    textures.heightMap.descriptor.sampler = textures.heightMap.sampler;

    // Setup a repeating sampler for the terrain texture layers
    manager->deviceFuncs->vkDestroySampler(manager->device, textures.terrainArray.sampler, nullptr);

    samplerInfo = Vulkan::Initializers::samplerCreateInfo();
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = (float)textures.terrainArray.mipLevels;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    if (manager->deviceFeatures.samplerAnisotropy)
    {
        samplerInfo.maxAnisotropy = 4.0f;
        samplerInfo.anisotropyEnable = VK_TRUE;
    }

    VK_CHECK_RESULT(manager->deviceFuncs->vkCreateSampler(manager->device, &samplerInfo, nullptr, &textures.terrainArray.sampler));

    textures.terrainArray.descriptor.sampler = textures.terrainArray.sampler;

    // create
    createVertexBuffers(manager);
    createUniformBuffers(manager);
    createDescriptorSetLayouts(manager);
    createPipeline(manager);
    createDescriptorPool(manager);
    createDescriptorSets(manager);

    buildCommandBuffers();
}

void MapTile::createDescriptorPool(Vulkan::Manager* vkManager)
{
    VkDescriptorPoolSize poolSizes[] =
	{
        Vulkan::Initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3),
        Vulkan::Initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3),
	};

    VkDescriptorPoolCreateInfo descriptorPoolInfo = Vulkan::Initializers::descriptorPoolCreateInfo(ARRAY_LENGTH(poolSizes), poolSizes, 2);

    VK_CHECK_RESULT(vkManager->deviceFuncs->vkCreateDescriptorPool(vkManager->device, &descriptorPoolInfo, nullptr, &descriptorPool));
}

void MapTile::createDescriptorSetLayouts(Vulkan::Manager* vkManager)
{
    VkDescriptorSetLayoutBinding setLayoutBindings[] =
    {
		// Binding 0 : Shared Tessellation shader uniform buffer object
        Vulkan::Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0),
		// Binding 1 : Height map
        Vulkan::Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		// Binding 2 : Terrain texture array layers
        Vulkan::Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
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
		// Binding 1 : Displacement map
        Vulkan::Initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.heightMap.descriptor),
		// Binding 2 : Color map (alpha channel)
        Vulkan::Initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.terrainArray.descriptor),
	};

    vkManager->deviceFuncs->vkUpdateDescriptorSets(vkManager->device, ARRAY_LENGTH(writeDescriptorSets), writeDescriptorSets, 0, NULL);
}

void MapTile::createPipeline(Vulkan::Manager* vkManager)
{
    VkDynamicState dynamicStateEnables[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = Vulkan::Initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState = Vulkan::Initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState = Vulkan::Initializers::pipelineColorBlendAttachmentState(0xF, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendState = Vulkan::Initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VkPipelineDepthStencilStateCreateInfo depthStencilState = Vulkan::Initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportState = Vulkan::Initializers::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleState = Vulkan::Initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT); // vkManager->getSampleCountFlagBits());
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
        Vulkan::Initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),	// Normal
        Vulkan::Initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6),	// UV
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState = Vulkan::Initializers::pipelineVertexInputStateCreateInfo();
    ///vertexInputState.flags = 0;
    ///vertexInputState.pNext = nullptr;
    vertexInputState.vertexBindingDescriptionCount = ARRAY_LENGTH(vertexInputBindings);
    vertexInputState.pVertexBindingDescriptions = vertexInputBindings;
    vertexInputState.vertexAttributeDescriptionCount = ARRAY_LENGTH(vertexInputAttributes);
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes;

    if(!vertexShader.isValid())
        vertexShader.load(":/shaders/terrain_vert.spv");

    if(!tessellationControlShader.isValid())
        tessellationControlShader.load(":/shaders/terrain_tesc.spv");

    if(!tessellationEvaluationShader.isValid())
        tessellationEvaluationShader.load(":/shaders/terrain_tese.spv");

    if(!fragmentShader.isValid())
        fragmentShader.load(":/shaders/terrain_frag.spv");

    VkPipelineShaderStageCreateInfo shaderStages[] =
    {
        vkManager->loadShader(vertexShader.data()->shaderModule, VK_SHADER_STAGE_VERTEX_BIT),
        vkManager->loadShader(tessellationControlShader.data()->shaderModule, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
        vkManager->loadShader(tessellationEvaluationShader.data()->shaderModule, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
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
    pipelineCreateInfo.pTessellationState = &tessellationState;
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
#define UV_SCALE   1.0f

	const uint32_t vertexCount = PATCH_SIZE * PATCH_SIZE;

	struct Vertex
	{
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
    };

	Vertex* vertices = new Vertex[vertexCount];

    heightmap = new Heightmap(QCoreApplication::applicationDirPath() + "/heightmap.ktx", PATCH_SIZE);

	const float wx = 2.0f;
	const float wy = 2.0f;

	for (auto x = 0; x < PATCH_SIZE; ++x)
	{
		for (auto y = 0; y < PATCH_SIZE; ++y)
		{
            uint32_t index = x + y * PATCH_SIZE;

            vertices[index].position = glm::vec3(x * wx + wx / 2.0f - (float)PATCH_SIZE * wx / 2.0f, 0.0f, y * wy + wy / 2.0f - (float)PATCH_SIZE * wy / 2.0f);
            vertices[index].uv = glm::vec2(static_cast<float>(x) / PATCH_SIZE, static_cast<float>(y) / PATCH_SIZE) * UV_SCALE;

            // Normals
            float heights[3][3];

            for(auto hx = -1; hx <= 1; ++hx)
            {
                for(auto hy = -1; hy <= 1; ++hy)
                    heights[hx + 1][hy + 1] = heightmap->getHeight(qMax(0, qMin(PATCH_SIZE - 1, x + hx)), qMax(0, qMin(PATCH_SIZE - 1, y + hy)));
            }

            // Calculate normal
            vertices[index].normal.x = heights[0][0] - heights[2][0] + 2.0f * heights[0][1] - 2.0f * heights[2][1] + heights[0][2] - heights[2][2];
            vertices[index].normal.z = heights[0][0] + 2.0f * heights[1][0] + heights[2][0] - heights[0][2] - 2.0f * heights[1][2] - heights[2][2];
            vertices[index].normal.y = 0.25f * sqrt(1.0f - vertices[index].normal.x * vertices[index].normal.x - vertices[index].normal.z * vertices[index].normal.z);

            vertices[index].normal = glm::normalize(vertices[index].normal * glm::vec3(2.0f, 1.0f, 2.0f));
		}
	}

    // Indices
	const uint32_t w = PATCH_SIZE - 1;
    const uint32_t indexCount = w * w * 6;

    model.indexCount = indexCount;

    uint32_t* indices = new uint32_t[indexCount];

    for (auto x = 0; x < w; ++x)
	{
		for (auto y = 0; y < w; ++y)
		{
			uint32_t index = (x + y * w) * 4;

			indices[index] = x + y * PATCH_SIZE;
			indices[index + 1] = indices[index] + PATCH_SIZE;
			indices[index + 2] = indices[index + 1] + 1;
			indices[index + 3] = indices[index] + 1;
		}
    }

	uint32_t vertexBufferSize = vertexCount * sizeof(Vertex);
    uint32_t indexBufferSize = indexCount * sizeof(uint32_t);

	struct
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
    } vertexStaging, indexStaging;

    VK_CHECK_RESULT(vkManager->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBufferSize, &vertexStaging.buffer, &vertexStaging.memory, vertices));
    VK_CHECK_RESULT(vkManager->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBufferSize, &indexStaging.buffer, &indexStaging.memory, indices));
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
    delete[] indices;
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

    if(heightmap)
    {
        delete heightmap;

        heightmap = Q_NULLPTR;
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

    updateUniformBuffers(camera, frustum);
}

void MapTile::updateUniformBuffers(const Camera& camera, Frustum frustum)
{
    uniformTessellation.projection = camera.getProjectionMatrix();
    uniformTessellation.modelView = camera.getViewMatrix() * glm::mat4x4(1.0f);
    uniformTessellation.lightPosition[2] = -0.5f * uniformTessellation.displacementFactor;
    uniformTessellation.viewportDimension = glm::vec2(VulkanManager->viewportSize.width(), VulkanManager->viewportSize.height());

    frustum.update(uniformTessellation.projection * uniformTessellation.modelView);

    for(int i = 0; i < 6; ++i)
        uniformTessellation.frustumPlanes[i] = frustum.getPlane(i);

    memcpy(uniformTessellationBuffer.mapped, &uniformTessellation, sizeof(uniformTessellation));
}
