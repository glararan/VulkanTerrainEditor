#ifndef WORLD_H
#define WORLD_H

#include "MapViewRenderer.h"

#include "Terrain/MapTile.h"

#include "Common/Frustum.h"

#include "Vulkan/Manager.h"

#define TILES 1

class World
{
public:
	World();
	~World();

	void create(Vulkan::Manager& vkManager);
	void destroy(Vulkan::Manager& vkManager);

    void draw(const VkCommandBuffer commandBuffer, QVulkanDeviceFunctions* deviceFuncs, const Camera& camera);

	bool getWireframe() const { return wireframe; }

	void setWireframe(const bool& enable) { wireframe = enable; }

private:
	MapTile tiles[TILES][TILES];

    Frustum frustum;

	bool wireframe = false;
	bool terrainTessellation = true;
};

#endif // WORLD_H
