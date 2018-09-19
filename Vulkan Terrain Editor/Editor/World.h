#ifndef WORLD_H
#define WORLD_H

#include "Terrain/MapTile.h"

#include "Common/Frustum.h"

#define TILES 1

class World
{
public:
	World();
	~World();

	void draw();

	bool getWireframe() const { return wireframe; }

	void setWireframe(const bool& enable) { wireframe = enable; }

private:
	MapTile tiles[TILES][TILES];

    Frustum frustum;

	bool wireframe = false;
	bool terrainTessellation = true;
};

#endif // WORLD_H
