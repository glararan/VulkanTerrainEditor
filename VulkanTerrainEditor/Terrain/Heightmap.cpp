#include "Heightmap.h"

#include <QDebug>

Heightmap::Heightmap()
{
}

Heightmap::Heightmap(const QString& filename, uint32_t patchSize)
{
    gli::texture2d heightmapTexture(gli::load(filename.toStdString().c_str()));

    dim = static_cast<uint32_t>(heightmapTexture.extent().x);
    heightData = new uint16_t[dim * dim];

    memcpy(heightData, heightmapTexture.data(), heightmapTexture.size());

    scale = dim / patchSize;
}

Heightmap::~Heightmap()
{
    delete[] heightData;
}

float Heightmap::getHeight(uint32_t x, uint32_t y) const
{
    glm::ivec2 rpos = glm::ivec2(x, y) * glm::ivec2(scale);
    rpos.x = qMax(0, qMin(rpos.x, (int)dim - 1));
    rpos.y = qMax(0, qMin(rpos.y, (int)dim - 1));
    rpos /= glm::ivec2(scale);

    float a = heightData[0];

    return *(heightData + (rpos.x + rpos.y * dim) * scale) / 65535.0f;
}

float Heightmap::getLowestHeight() const
{
    float lowest = getHeight(0, 0);

    for(int x = 0; x < dim; ++x)
    {
        for(int y = 0; y < dim; ++y)
        {
            float height = getHeight(x, y);

            if(height < lowest)
                lowest = height;
        }
    }

    return lowest;
}
