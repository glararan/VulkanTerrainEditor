#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H

#include <QObject>

#include "Glm.h"

class Heightmap
{
public:
    Heightmap();
    Heightmap(const QString& filename, uint32_t patchSize);
    ~Heightmap();

    uint32_t getDimension() const { return dim; }

    float getHeight(uint32_t x, uint32_t y) const;
    float getLowestHeight() const;

private:
    uint16_t* heightData;
    uint32_t dim;
    uint32_t scale;
};

#endif // HEIGHTMAP_H
