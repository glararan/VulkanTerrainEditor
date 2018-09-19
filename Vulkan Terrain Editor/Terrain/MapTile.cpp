#include "MapTile.h"

MapTile::MapTile()
{
}


MapTile::~MapTile()
{
}

void MapTile::draw(const Frustum& frustum, const bool& wireframe, const bool& tessellation)
{
    for(quint32 x = 0; x < CHUNKS; ++x)
    {
        for(quint32 y = 0; y < CHUNKS; ++y)
            chunks->draw(frustum, wireframe, tessellation);
    }
}
