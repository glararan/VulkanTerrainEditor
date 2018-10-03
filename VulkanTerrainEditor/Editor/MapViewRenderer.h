#ifndef MAP_VIEW_RENDERER_H
#define MAP_VIEW_RENDERER_H

#include <QFutureWatcher>
#include <QMutex>
#include <QMatrix4x4>
#include <QVulkanWindowRenderer>
#include <QVulkanFunctions>

#include "Common/Camera.h"

#include "Vulkan/Buffer.h"
#include "Vulkan/Tools.h"

class MapView;
class World;

class MapViewRenderer : public QVulkanWindowRenderer
{
public:
	MapViewRenderer(MapView* parent, bool msaa = false);
	~MapViewRenderer();
	
	void initResources() override;
	void initSwapChainResources() override;
	void releaseSwapChainResources() override;
	void releaseResources() override;

	void startNextFrame() override;

    Camera* getCamera() { return &camera; }
	const World* getWorld() const { return world; }

private:
	void createPipelines();

	void markViewProjectionDirty();

	void renderFrame();

	MapView* window = Q_NULLPTR;

	QVulkanDeviceFunctions* deviceFuncs = Q_NULLPTR;

	World* world = Q_NULLPTR;

	Camera camera;

	VkPipelineCache pipelineCache = VK_NULL_HANDLE;
	
	QMatrix4x4 proj;

	int viewProjectionDirty = 0;

	bool framePending = false;

	QFutureWatcher<void> frameWatcher;

	QMutex guiMutex;
};

#endif // MAP_VIEW_RENDERER_H
