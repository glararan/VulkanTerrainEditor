#include "World.h"

World::World()
{
}

World::~World()
{
}

void World::create()
{
	for (int x = 0; x < TILES; ++x)
	{
		for (int y = 0; y < TILES; ++y)
            tiles[x][y].create();
	}
}

void World::destroy()
{
	for (int x = 0; x < TILES; ++x)
	{
		for (int y = 0; y < TILES; ++y)
            tiles[x][y].destroy();
	}
}

void World::draw(const Camera& camera)
{
    for (int x = 0; x < TILES; ++x)
	{
        for (int y = 0; y < TILES; ++y)
            tiles[x][y].draw(camera, frustum, wireframe, terrainTessellation);
	}
}
