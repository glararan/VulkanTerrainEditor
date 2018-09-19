#include "MapView.h"

#include "Vulkan/Tools.h"

#include <QFile>

static const int UNIFORM_DATA_SIZE = 16 * sizeof(float);

static inline VkDeviceSize aligned(VkDeviceSize v, VkDeviceSize byteAlign)
{
	return (v + byteAlign - 1) & ~(byteAlign - 1);
}

MapView::MapView()
{
	world = new World();

	camera.setPerspectiveProjection(60.0f, (float)width() / (float)height(), 0.1f, 512.0f);
	camera.setMovementSpeed(7.5f);
}

MapView::~MapView()
{
    delete world;
}

QVulkanWindowRenderer* MapView::createRenderer()
{
	renderer = new MapViewRenderer(this, true);

	return renderer;
}

MapViewRenderer::MapViewRenderer(MapView* parent, bool msaa) : window(parent)
{
	if (msaa)
	{
		const QVector<int> counts = window->supportedSampleCounts();

		qDebug() << "Supported sample counts:" << counts;

		for (int s = 16; s >= 4; s /= 2)
		{
			if (counts.contains(s))
			{
				qDebug("Requesting sample count %d", s);

				window->setSampleCount(s);

				break;
			}
		}
	}
}

MapViewRenderer::~MapViewRenderer()
{
    if (queryPool != VK_NULL_HANDLE)
    {
        deviceFuncs->vkDestroyQueryPool(device, queryPool, nullptr);
        deviceFuncs->vkDestroyBuffer(device, queryResult.buffer, nullptr);
        deviceFuncs->vkFreeMemory(device, queryResult.memory, nullptr);
    }
}

void MapViewRenderer::buildCommandBuffers()
{
    VkCommandBufferBeginInfo cmdBufInfo = Vulkan::Initializers::commandBufferBeginInfo();

    VkClearValue clearValues[2];
    clearValues[0].color = { { 0.025f, 0.025f, 0.025f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = Vulkan::Initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = window->width();
    renderPassBeginInfo.renderArea.extent.height = window->height();
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    for (qint32 i = 0; i < drawCmdBuffers.size(); ++i)
    {
        renderPassBeginInfo.framebuffer = frameBuffers[i];

        VK_CHECK_RESULT(deviceFuncs->vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

        if (enabledFeatures.pipelineStatisticsQuery)
            deviceFuncs->vkCmdResetQueryPool(drawCmdBuffers[i], queryPool, 0, 2);

        deviceFuncs->vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = Vulkan::Initializers::viewport((float)window->width(), (float)window->height(), 0.0f, 1.0f);
        deviceFuncs->vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

        VkRect2D scissor = Vulkan::Initializers::rect2D(window->width(), window->height(), 0, 0);
        deviceFuncs->vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

        deviceFuncs->vkCmdSetLineWidth(drawCmdBuffers[i], 1.0f);

        VkDeviceSize offsets[1] = { 0 };

        // Skysphere
        deviceFuncs->vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skySphere);
        deviceFuncs->vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.skySphere, 0, 1, &descriptorSets.skySphere, 0, NULL);
        deviceFuncs->vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &models.skysphere.vertices.buffer, offsets);
        deviceFuncs->vkCmdBindIndexBuffer(drawCmdBuffers[i], models.skysphere.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
        deviceFuncs->vkCmdDrawIndexed(drawCmdBuffers[i], models.skysphere.indexCount, 1, 0, 0, 0);

        // Terrain
        if (enabledFeatures.pipelineStatisticsQuery)
            deviceFuncs->vkCmdBeginQuery(drawCmdBuffers[i], queryPool, 0, VK_QUERY_CONTROL_PRECISE_BIT); // Begin pipeline statistics query

        // Render
        deviceFuncs->vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, window->getWorld()->getWireframe() ? pipelines.wireframe : pipelines.terrain);
        deviceFuncs->vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.terrain, 0, 1, &descriptorSets.terrain, 0, NULL);
        deviceFuncs->vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &models.terrain.vertices.buffer, offsets);
        deviceFuncs->vkCmdBindIndexBuffer(drawCmdBuffers[i], models.terrain.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
        deviceFuncs->vkCmdDrawIndexed(drawCmdBuffers[i], models.terrain.indexCount, 1, 0, 0, 0);

        if (enabledFeatures.pipelineStatisticsQuery)
            deviceFuncs->vkCmdEndQuery(drawCmdBuffers[i], queryPool, 0); // End pipeline statistics query

        deviceFuncs->vkCmdEndRenderPass(drawCmdBuffers[i]);

        VK_CHECK_RESULT(deviceFuncs->vkEndCommandBuffer(drawCmdBuffers[i]));
    }
}

void MapViewRenderer::rebuildCommandBuffers()
{
    if(!checkCommandBuffers())
    {
        destroyCommandBuffers();
        createCommandBuffers();
    }

    buildCommandBuffers();
}

VkCommandBuffer MapViewRenderer::createCommandBuffer(VkCommandBufferLevel level, const bool& begin)
{
    VkCommandBuffer cmdBuffer;

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = Vulkan::Initializers::commandBufferAllocateInfo(cmdPool, level, 1);

    VK_CHECK_RESULT(deviceFuncs->vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

    // If requested, also start the new command buffer
    if (begin)
    {
        VkCommandBufferBeginInfo cmdBufInfo = Vulkan::Initializers::commandBufferBeginInfo();

        VK_CHECK_RESULT(deviceFuncs->vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
    }

    return cmdBuffer;
}

void MapViewRenderer::createCommandBuffers()
{
    // Create one command buffer for each swap chain image and reuse for rendering
    drawCmdBuffers.resize(swapChain.imageCount);

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = Vulkan::Initializers::commandBufferAllocateInfo(cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<quint32>(drawCmdBuffers.size()));

    VK_CHECK_RESULT(deviceFuncs->vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));
}

VkShaderModule MapViewRenderer::createShader(const QString& name)
{
	QFile file(name);

	if (!file.open(QIODevice::ReadOnly))
	{
		qWarning("Failed to read shader %s", qPrintable(name));

		return VK_NULL_HANDLE;
	}

	QByteArray blob = file.readAll();

	file.close();

	VkShaderModuleCreateInfo shaderInfo;

	memset(&shaderInfo, 0, sizeof(shaderInfo));

	shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderInfo.codeSize = blob.size();
    shaderInfo.pCode = reinterpret_cast<const quint32*>(blob.constData());

	VkShaderModule shaderModule;

	VkResult result = deviceFuncs->vkCreateShaderModule(window->device(), &shaderInfo, nullptr, &shaderModule);

	if (result != VK_SUCCESS)
	{
		qWarning("Failed to create shader module: %d", result);

		return VK_NULL_HANDLE;
	}

	return shaderModule;
}

void MapViewRenderer::destroyCommandBuffers()
{
    deviceFuncs->vkFreeCommandBuffers(device, cmdPool, static_cast<quint32>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

void MapViewRenderer::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, const bool& free)
{
    if (commandBuffer == VK_NULL_HANDLE)
        return;

    VK_CHECK_RESULT(deviceFuncs->vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK_RESULT(deviceFuncs->vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK_RESULT(deviceFuncs->vkQueueWaitIdle(queue));

    if (free)
        deviceFuncs->vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
}

void MapViewRenderer::getQueryResults()
{
    // We use vkGetQueryResults to copy the results into a host visible buffer
    deviceFuncs->vkGetQueryPoolResults(device, queryPool, 0, 1, sizeof(pipelineStats), pipelineStats, sizeof(quint64), VK_QUERY_RESULT_64_BIT);
}

bool MapViewRenderer::checkCommandBuffers() const
{
    for (auto& cmdBuffer : drawCmdBuffers)
    {
        if (cmdBuffer == VK_NULL_HANDLE)
            return false;
    }

    return true;
}

void MapViewRenderer::initResources()
{
	qDebug("initResources");

    VkDevice vkDevice = window->device();

    deviceFuncs = window->vulkanInstance()->deviceFunctions(vkDevice);

    device = Vulkan::VulkanDevice(window->physicalDevice(), deviceFuncs);

    VkPhysicalDeviceFeatures deviceFeatures;

    window->vulkanInstance()->functions()->vkGetPhysicalDeviceFeatures(window->physicalDevice(), &deviceFeatures);

    // Tessellation shader support is required for this example
    if (deviceFeatures.tessellationShader)
        enabledFeatures.tessellationShader = VK_TRUE;
    else
        qFatal("Selected GPU does not support tessellation shaders!", VK_ERROR_FEATURE_NOT_PRESENT);

    // Fill mode non solid is required for wireframe display
    if (deviceFeatures.fillModeNonSolid)
        enabledFeatures.fillModeNonSolid = VK_TRUE;

    // Pipeline statistics
    if (deviceFeatures.pipelineStatisticsQuery)
        enabledFeatures.pipelineStatisticsQuery = VK_TRUE;

    // Enable anisotropic filtering if supported
    if (deviceFeatures.samplerAnisotropy)
        enabledFeatures.samplerAnisotropy = VK_TRUE;

    // Enable texture compression
    if (deviceFeatures.textureCompressionBC)
        enabledFeatures.textureCompressionBC = VK_TRUE;
    else if (deviceFeatures.textureCompressionASTC_LDR)
        enabledFeatures.textureCompressionASTC_LDR = VK_TRUE;
    else if (deviceFeatures.textureCompressionETC2)
        enabledFeatures.textureCompressionETC2 = VK_TRUE;

    if(deviceFeatures.pipelineStatisticsQuery)
        initQueryResultBuffer();

    buildCommandBuffers();

	// Prepare the vertex and uniform data. The vertex data will never
	// change so one buffer is sufficient regardless of the value of
	// QVulkanWindow::CONCURRENT_FRAME_COUNT. Uniform data is changing per
	// frame however so active frames have to have a dedicated copy.

	// Use just one memory allocation and one buffer. We will then specify the
	// appropriate offsets for uniform buffers in the VkDescriptorBufferInfo.
	// Have to watch out for
	// VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment, though.

	// The uniform buffer is not strictly required in this example, we could
	// have used push constants as well since our single matrix (64 bytes) fits
	// into the spec mandated minimum limit of 128 bytes. However, once that
	// limit is not sufficient, the per-frame buffers, as shown below, will
	// become necessary.
	const int concurrentFrameCount = window->concurrentFrameCount();
	const VkPhysicalDeviceLimits* pdevLimits = &window->physicalDeviceProperties()->limits;
	const VkDeviceSize uniAlign = pdevLimits->minUniformBufferOffsetAlignment;

    qDebug("uniform buffer offset alignment is %u", (quint32)uniAlign);

	VkBufferCreateInfo bufInfo;
	memset(&bufInfo, 0, sizeof(bufInfo));

	bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

	// Our internal layout is vertex, uniform, uniform, ... with each uniform buffer start offset aligned to uniAlign.
	const VkDeviceSize vertexAllocSize = aligned(sizeof(vertexData), uniAlign);
	const VkDeviceSize uniformAllocSize = aligned(UNIFORM_DATA_SIZE, uniAlign);

	bufInfo.size = vertexAllocSize + concurrentFrameCount * uniformAllocSize;
	bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	VkResult result = deviceFuncs->vkCreateBuffer(device, &bufInfo, nullptr, &buffer);

	if (result != VK_SUCCESS)
		qFatal("Failed to create buffer: %d", result);

	VkMemoryRequirements memReq;
	deviceFuncs->vkGetBufferMemoryRequirements(device, buffer, &memReq);

	VkMemoryAllocateInfo memAllocInfo =
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		nullptr,
		memReq.size,
		window->hostVisibleMemoryIndex()
	};

	result = deviceFuncs->vkAllocateMemory(device, &memAllocInfo, nullptr, &memBuffer);

	if (result != VK_SUCCESS)
		qFatal("Failed to allocate memory: %d", result);

	result = deviceFuncs->vkBindBufferMemory(device, buffer, memBuffer, 0);

	if (result != VK_SUCCESS)
		qFatal("Failed to bind buffer memory: %d", result);

	quint8* p;

	result = deviceFuncs->vkMapMemory(device, memBuffer, 0, memReq.size, 0, reinterpret_cast<void **>(&p));

	if (result != VK_SUCCESS)
		qFatal("Failed to map memory: %d", result);

	memcpy(p, vertexData, sizeof(vertexData));

	QMatrix4x4 identity;

	memset(uniformBufInfo, 0, sizeof(uniformBufInfo));

	for (int i = 0; i < concurrentFrameCount; ++i)
	{
		const VkDeviceSize offset = vertexAllocSize + i * uniformAllocSize;

		memcpy(p + offset, identity.constData(), 16 * sizeof(float));

		uniformBufInfo[i].buffer = buffer;
		uniformBufInfo[i].offset = offset;
		uniformBufInfo[i].range = uniformAllocSize;
	}

	deviceFuncs->vkUnmapMemory(device, memBuffer);

	VkVertexInputBindingDescription vertexBindingDesc =
	{
		0, // binding
		5 * sizeof(float),
		VK_VERTEX_INPUT_RATE_VERTEX
	};

	VkVertexInputAttributeDescription vertexAttrDesc[] =
	{
		{ // position
			0, // location
			0, // binding
			VK_FORMAT_R32G32_SFLOAT,
			0
		},
		{ // color
			1,
			0,
			VK_FORMAT_R32G32B32_SFLOAT,
			2 * sizeof(float)
		}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.pNext = nullptr;
	vertexInputInfo.flags = 0;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
	vertexInputInfo.vertexAttributeDescriptionCount = 2;
	vertexInputInfo.pVertexAttributeDescriptions = vertexAttrDesc;

	// Set up descriptor set and its layout.
    VkDescriptorPoolSize descPoolSizes = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, quint32(concurrentFrameCount) };
	VkDescriptorPoolCreateInfo descPoolInfo;

	memset(&descPoolInfo, 0, sizeof(descPoolInfo));

	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.maxSets = concurrentFrameCount;
	descPoolInfo.poolSizeCount = 1;
	descPoolInfo.pPoolSizes = &descPoolSizes;

	result = deviceFuncs->vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descPool);

	if (result != VK_SUCCESS)
		qFatal("Failed to create descriptor pool: %d", result);

	VkDescriptorSetLayoutBinding layoutBinding =
	{
		0, // binding
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1,
		VK_SHADER_STAGE_VERTEX_BIT,
		nullptr
	};

	VkDescriptorSetLayoutCreateInfo descLayoutInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,
		&layoutBinding
	};

	result = deviceFuncs->vkCreateDescriptorSetLayout(device, &descLayoutInfo, nullptr, &descSetLayout);

	if (result != VK_SUCCESS)
		qFatal("Failed to create descriptor set layout: %d", result);

	for (int i = 0; i < concurrentFrameCount; ++i)
	{
		VkDescriptorSetAllocateInfo descSetAllocInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			nullptr,
			descPool,
			1,
			&descSetLayout
		};

		result = deviceFuncs->vkAllocateDescriptorSets(device, &descSetAllocInfo, &m_descSet[i]);

		if (result != VK_SUCCESS)
			qFatal("Failed to allocate descriptor set: %d", result);

		VkWriteDescriptorSet descWrite;

		memset(&descWrite, 0, sizeof(descWrite));

		descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descWrite.dstSet = m_descSet[i];
		descWrite.descriptorCount = 1;
		descWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descWrite.pBufferInfo = &uniformBufInfo[i];

		deviceFuncs->vkUpdateDescriptorSets(device, 1, &descWrite, 0, nullptr);
	}

	// Pipeline cache
	VkPipelineCacheCreateInfo pipelineCacheInfo;

	memset(&pipelineCacheInfo, 0, sizeof(pipelineCacheInfo));

	pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

	result = deviceFuncs->vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr, &pipelineCache);

	if (result != VK_SUCCESS)
		qFatal("Failed to create pipeline cache: %d", result);

	// Pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;

	memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));

	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descSetLayout;

	result = deviceFuncs->vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

	if (result != VK_SUCCESS)
		qFatal("Failed to create pipeline layout: %d", result);

	// Shaders
	VkShaderModule vertShaderModule = createShader(QCoreApplication::instance()->applicationDirPath() + QStringLiteral("/shaders/triangle.vert.spv"));
	VkShaderModule fragShaderModule = createShader(QCoreApplication::instance()->applicationDirPath() + QStringLiteral("/shaders/triangle.frag.spv"));

	// Graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo;

	memset(&pipelineInfo, 0, sizeof(pipelineInfo));

	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	VkPipelineShaderStageCreateInfo shaderStages[2] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			vertShaderModule,
			"main",
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragShaderModule,
			"main",
			nullptr
		}
	};

	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;

	VkPipelineInputAssemblyStateCreateInfo ia;

	memset(&ia, 0, sizeof(ia));

	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	pipelineInfo.pInputAssemblyState = &ia;

	// The viewport and scissor will be set dynamically via vkCmdSetViewport/Scissor.
	// This way the pipeline does not need to be touched when resizing the window.
	VkPipelineViewportStateCreateInfo vp;

	memset(&vp, 0, sizeof(vp));

	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	pipelineInfo.pViewportState = &vp;

	VkPipelineRasterizationStateCreateInfo rs;

	memset(&rs, 0, sizeof(rs));

	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE; // we want the back face as well
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.lineWidth = 1.0f;

	pipelineInfo.pRasterizationState = &rs;

	VkPipelineMultisampleStateCreateInfo ms;

	memset(&ms, 0, sizeof(ms));

	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	// Enable multisampling.
	ms.rasterizationSamples = window->sampleCountFlagBits();

	pipelineInfo.pMultisampleState = &ms;

	VkPipelineDepthStencilStateCreateInfo ds;

	memset(&ds, 0, sizeof(ds));

	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_TRUE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	pipelineInfo.pDepthStencilState = &ds;

	VkPipelineColorBlendStateCreateInfo cb;

	memset(&cb, 0, sizeof(cb));

	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

	// no blend, write out all of rgba
	VkPipelineColorBlendAttachmentState att;

	memset(&att, 0, sizeof(att));

	att.colorWriteMask = 0xF;

	cb.attachmentCount = 1;
	cb.pAttachments = &att;

	pipelineInfo.pColorBlendState = &cb;

	VkDynamicState dynEnable[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dyn;

	memset(&dyn, 0, sizeof(dyn));

	dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn.dynamicStateCount = sizeof(dynEnable) / sizeof(VkDynamicState);
	dyn.pDynamicStates = dynEnable;

	pipelineInfo.pDynamicState = &dyn;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = window->defaultRenderPass();

	result = deviceFuncs->vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipeline);

	if (result != VK_SUCCESS)
		qFatal("Failed to create graphics pipeline: %d", result);

	if (vertShaderModule)
		deviceFuncs->vkDestroyShaderModule(device, vertShaderModule, nullptr);

	if (fragShaderModule)
		deviceFuncs->vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

void MapViewRenderer::initSwapChainResources()
{
	qDebug("initSwapChainResources");

	const QSize size = window->swapChainImageSize();

	// Projection matrix
	projMat = window->clipCorrectionMatrix(); // adjust for Vulkan-OpenGL clip space differences
	projMat.perspective(45.0f, size.width() / (float)size.height(), 0.01f, 100.0f);
	projMat.translate(0, 0, -4);
}

void MapViewRenderer::initQueryResultBuffer()
{
    quint32 bufSize = 2 * sizeof(quint64);

    VkMemoryRequirements memReqs;
    VkMemoryAllocateInfo memAlloc = Vulkan::Initializers::memoryAllocateInfo();
    VkBufferCreateInfo bufferCreateInfo = Vulkan::Initializers::bufferCreateInfo(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, bufSize);

    // Results are saved in a host visible buffer for easy access by the application
    VK_CHECK_RESULT(deviceFuncs->vkCreateBuffer(device, &bufferCreateInfo, nullptr, &queryResult.buffer));

    deviceFuncs->vkGetBufferMemoryRequirements(device, queryResult.buffer, &memReqs);

    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK_RESULT(deviceFuncs->vkAllocateMemory(device, &memAlloc, nullptr, &queryResult.memory));
    VK_CHECK_RESULT(deviceFuncs->vkBindBufferMemory(device, queryResult.buffer, queryResult.memory, 0));

    // Create query pool
    if (enabledFeatures.pipelineStatisticsQuery)
    {
        VkQueryPoolCreateInfo queryPoolInfo = {};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
        queryPoolInfo.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT;
        queryPoolInfo.queryCount = 2;

        VK_CHECK_RESULT(deviceFuncs->vkCreateQueryPool(device, &queryPoolInfo, NULL, &queryPool));
    }
}

void MapViewRenderer::releaseSwapChainResources()
{
	qDebug("releaseSwapChainResources");
}

void MapViewRenderer::releaseResources()
{
	qDebug("releaseResources");

	VkDevice device = window->device();

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

	if (pipelineCache)
	{
		deviceFuncs->vkDestroyPipelineCache(device, pipelineCache, nullptr);

		pipelineCache = VK_NULL_HANDLE;
	}

	if (descSetLayout)
	{
		deviceFuncs->vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);

		descSetLayout = VK_NULL_HANDLE;
	}

	if (descPool)
	{
		deviceFuncs->vkDestroyDescriptorPool(device, descPool, nullptr);

		descPool = VK_NULL_HANDLE;
	}

	if (buffer)
	{
		deviceFuncs->vkDestroyBuffer(device, buffer, nullptr);

		buffer = VK_NULL_HANDLE;
	}

	if (memBuffer)
	{
		deviceFuncs->vkFreeMemory(device, memBuffer, nullptr);

		memBuffer = VK_NULL_HANDLE;
	}
}

void MapViewRenderer::startNextFrame()
{
	VkDevice device = window->device();
	VkCommandBuffer commandBuffer = window->currentCommandBuffer();

	const QSize size = window->swapChainImageSize();

	VkClearColorValue clearColor = { 0, 0, 0, 1 };
	VkClearDepthStencilValue clearDS = { 1, 0 };
	VkClearValue clearValues[3];

	memset(clearValues, 0, sizeof(clearValues));

	clearValues[0].color = clearValues[2].color = clearColor;
	clearValues[1].depthStencil = clearDS;

	VkRenderPassBeginInfo rpBeginInfo;

	memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));

	rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBeginInfo.renderPass = window->defaultRenderPass();
	rpBeginInfo.framebuffer = window->currentFramebuffer();
	rpBeginInfo.renderArea.extent.width = size.width();
	rpBeginInfo.renderArea.extent.height = size.height();
	rpBeginInfo.clearValueCount = window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
	rpBeginInfo.pClearValues = clearValues;

	VkCommandBuffer cmdBuf = window->currentCommandBuffer();
	deviceFuncs->vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	quint8* p;

	VkResult error = deviceFuncs->vkMapMemory(device, memBuffer, uniformBufInfo[window->currentFrame()].offset, UNIFORM_DATA_SIZE, 0, reinterpret_cast<void**>(&p));

	if (error != VK_SUCCESS)
		qFatal("Failed to map memory: %d", error);

	QMatrix4x4 matrix = projMat;
	matrix.rotate(rotation, 0, 1, 0);

	memcpy(p, matrix.constData(), 16 * sizeof(float));

	deviceFuncs->vkUnmapMemory(device, memBuffer);

	// Not exactly a real animation system, just advance on every frame for now.
	rotation += 1.0f;

	deviceFuncs->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	deviceFuncs->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &m_descSet[window->currentFrame()], 0, nullptr);

	VkDeviceSize vbOffset = 0;
	deviceFuncs->vkCmdBindVertexBuffers(commandBuffer, 0, 1, &buffer, &vbOffset);

	VkViewport viewport;
	viewport.x = viewport.y = 0;
	viewport.width = size.width();
	viewport.height = size.height();
	viewport.minDepth = 0;
	viewport.maxDepth = 1;

	deviceFuncs->vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor;
	scissor.offset.x = scissor.offset.y = 0;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	deviceFuncs->vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	deviceFuncs->vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	deviceFuncs->vkCmdEndRenderPass(cmdBuf);

	window->frameReady();
	window->requestUpdate(); // render continuously, throttled by the presentation rate
}
