#include "MapViewRenderer.h"

#include "MapView.h"
#include "World.h"

#include "Vulkan/Manager.h"
#include "Vulkan/Initializers.h"

#include <QtConcurrentRun>

MapViewRenderer::MapViewRenderer(MapView* parent, bool msaa) : window(parent), world(new World())
{
    camera.setPerspectiveProjection(60.0f, static_cast<float>(parent->width()) / static_cast<float>(window->height()), 0.1f, 512.0f);
	camera.setMovementSpeed(7.5f);

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

	QObject::connect(&frameWatcher, &QFutureWatcherBase::finished, [this]
	{
		if (framePending)
		{
			framePending = false;

			window->frameReady();
			window->requestUpdate();
		}
	});
}

MapViewRenderer::~MapViewRenderer()
{
	delete world;
}

void MapViewRenderer::initResources()
{
	QVulkanInstance* instance = window->vulkanInstance();

	VkDevice device = window->device();

	const VkPhysicalDeviceLimits* physicalDeviceLimits = &window->physicalDeviceProperties()->limits;
	const VkDeviceSize uniformAlign = physicalDeviceLimits->minUniformBufferOffsetAlignment;

	deviceFuncs = instance->deviceFunctions(device);

	Vulkan::Manager vulkanManager(window);

	world->create(vulkanManager);
}

void MapViewRenderer::initSwapChainResources()
{
	proj = window->clipCorrectionMatrix();

	const QSize size = window->swapChainImageSize();

    proj.perspective(45.0f, size.width() / static_cast<float>(size.height()), 0.01f, 1000.0f);

	markViewProjectionDirty();
}

void MapViewRenderer::releaseResources()
{
	Vulkan::Manager vulkanManager(window);

	world->destroy(vulkanManager);
}

void MapViewRenderer::releaseSwapChainResources()
{
	frameWatcher.waitForFinished();

	if (framePending)
	{
		framePending = false;

		window->frameReady();
	}
}

void MapViewRenderer::createPipelines()
{
	VkDevice device = window->device();

	VkPipelineCacheCreateInfo pipelineCacheInfo;
	memset(&pipelineCacheInfo, 0, sizeof(pipelineCacheInfo));

	pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

	VK_CHECK_RESULT(deviceFuncs->vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr, &pipelineCache));
}

void MapViewRenderer::markViewProjectionDirty()
{
	viewProjectionDirty = window->concurrentFrameCount();
}

void MapViewRenderer::startNextFrame()
{
	Q_ASSERT(!framePending);

	framePending = true;

	QFuture<void> future = QtConcurrent::run(this, &MapViewRenderer::renderFrame);

	frameWatcher.setFuture(future);
}

void MapViewRenderer::renderFrame()
{
	QMutexLocker locker(&guiMutex);

	/*ensureBuffers();
	ensureInstanceBuffer();
	m_pipelinesFuture.waitForFinished();*/

	VkCommandBuffer commandBuffer = window->currentCommandBuffer();

	const QSize size = window->swapChainImageSize();

	VkClearColorValue clearColor = { { 0.67f, 0.84f, 0.9f, 1.0f } };
	VkClearDepthStencilValue clearDS = { 1, 0 };
	VkClearValue clearValues[3];

	memset(clearValues, 0, sizeof(clearValues));

	clearValues[0].color = clearValues[2].color = clearColor;
	clearValues[1].depthStencil = clearDS;
	
	VkRenderPassBeginInfo rpBeginInfo = Vulkan::Initializers::renderPassBeginInfo();
	rpBeginInfo.renderPass = window->defaultRenderPass();
	rpBeginInfo.framebuffer = window->currentFramebuffer();
    rpBeginInfo.renderArea.extent.width = static_cast<uint32_t>(size.width());
    rpBeginInfo.renderArea.extent.height = static_cast<uint32_t>(size.height());
	rpBeginInfo.clearValueCount = window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
	rpBeginInfo.pClearValues = clearValues;

	VkCommandBuffer cmdBuf = window->currentCommandBuffer();
	
	deviceFuncs->vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport =
	{
		0, 0,
		float(size.width()), float(size.height()),
		0, 1
	};
	
	deviceFuncs->vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor =
	{
		{ 0, 0 },
		{ uint32_t(size.width()), uint32_t(size.height()) }
	};
	
	deviceFuncs->vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    world->draw(window->currentCommandBuffer(), deviceFuncs, camera);

	deviceFuncs->vkCmdEndRenderPass(cmdBuf);
}
