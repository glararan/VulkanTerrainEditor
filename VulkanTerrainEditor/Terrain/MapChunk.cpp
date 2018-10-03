#include "MapChunk.h"

MapChunk::MapChunk()
{

}

MapChunk::~MapChunk()
{

}

void MapChunk::draw(const Frustum& frustum, const bool& wireframe, const bool& tessellation)
{
    if(!isVisible(frustum, 0.0f))
        return;
}

bool MapChunk::isVisible(const Frustum& frustum, const float& distance) const
{
    return true; // frustum.check();
}
