#ifndef MAPCHUNK_H
#define MAPCHUNK_H

#include "Common/Frustum.h"

class MapChunk
{
public:
    MapChunk();
    ~MapChunk();

    void draw(const Frustum& frustum, const bool& wireframe, const bool& tessellation);

    bool isVisible(const Frustum& frustum, const float& distance) const;

private:

};

#endif // MAPCHUNK_H
