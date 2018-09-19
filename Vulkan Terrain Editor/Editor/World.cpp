#include "World.h"

World::World()
{
}

World::~World()
{
}

void World::draw()
{
    for (int x = 0; x < TILES; ++x)
	{
        for (int y = 0; y < TILES; ++y)
            tiles[x][y].draw(frustum, wireframe, terrainTessellation);
	}
}
