#include "MapViewRenderer.h"

#include "MapView.h"
#include "World.h"

#include "Vulkan/Manager.h"
#include "Vulkan/Initializers.h"

#include <QtConcurrentRun>

MapViewRenderer::MapViewRenderer(MapView* parent, bool msaa) : window(parent), world(new World())
{
	camera.setMovementSpeed(7.5f);

    /*if (msaa)
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
    }*/

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

	deviceFuncs = instance->deviceFunctions(device);

    createPipelines();

    VulkanManager->initialize(window, pipelineCache);

    world->create();
}

void MapViewRenderer::initSwapChainResources()
{
	const QSize size = window->swapChainImageSize();

    camera.setPerspectiveProjection(60.0f, static_cast<float>(size.width()) / static_cast<float>(size.height()), 0.01f, 1024.0f);
}

void MapViewRenderer::releaseResources()
{
    world->destroy();

    if (pipelineCache)
    {
        deviceFuncs->vkDestroyPipelineCache(VulkanManager->device, pipelineCache, nullptr);

        pipelineCache = VK_NULL_HANDLE;
    }
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

	VkCommandBuffer commandBuffer = window->currentCommandBuffer();

	const QSize size = window->swapChainImageSize();

    VulkanManager->viewportSize = size;

	VkClearColorValue clearColor = { { 0.67f, 0.84f, 0.9f, 1.0f } };
	VkClearDepthStencilValue clearDS = { 1, 0 };

    VkClearValue clearValues[2];
    clearValues[0].color = clearColor;
    clearValues[1].depthStencil = clearDS;
    /*VkClearValue clearValues[3];

	memset(clearValues, 0, sizeof(clearValues));

	clearValues[0].color = clearValues[2].color = clearColor;
    clearValues[1].depthStencil = clearDS;*/
	
	VkRenderPassBeginInfo rpBeginInfo = Vulkan::Initializers::renderPassBeginInfo();
	rpBeginInfo.renderPass = window->defaultRenderPass();
	rpBeginInfo.framebuffer = window->currentFramebuffer();
    rpBeginInfo.renderArea.extent.width = static_cast<uint32_t>(size.width());
    rpBeginInfo.renderArea.extent.height = static_cast<uint32_t>(size.height());
    rpBeginInfo.clearValueCount = 2;//window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
	rpBeginInfo.pClearValues = clearValues;
	
    deviceFuncs->vkCmdBeginRenderPass(commandBuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = Vulkan::Initializers::viewport(float(size.width()), float(size.height()), 0.0f, 1.0f);
	
	deviceFuncs->vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = Vulkan::Initializers::rect2D(uint32_t(size.width()), uint32_t(size.height()), 0, 0);
	
	deviceFuncs->vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    world->draw(camera);

    deviceFuncs->vkCmdEndRenderPass(commandBuffer);
}
