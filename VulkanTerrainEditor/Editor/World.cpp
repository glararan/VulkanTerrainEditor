#include "World.h"

World::World()
{
}

World::~World()
{
}

void World::create(Vulkan::Manager& vkManager)
{
	for (int x = 0; x < TILES; ++x)
	{
		for (int y = 0; y < TILES; ++y)
			tiles[x][y].create(vkManager);
	}
}

void World::destroy(Vulkan::Manager& vkManager)
{
	for (int x = 0; x < TILES; ++x)
	{
		for (int y = 0; y < TILES; ++y)
			tiles[x][y].destroy(vkManager);
	}
}

void World::draw(const VkCommandBuffer commandBuffer, QVulkanDeviceFunctions* deviceFuncs, const Camera& camera)
{
    for (int x = 0; x < TILES; ++x)
	{
        for (int y = 0; y < TILES; ++y)
            tiles[x][y].draw(commandBuffer, deviceFuncs, camera, frustum, wireframe, terrainTessellation);
	}
}
