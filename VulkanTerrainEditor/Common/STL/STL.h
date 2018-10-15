#ifndef STL_H
#define STL_H

#include <QObject>
#include <QTextStream>
#include <QtConcurrent>
#include <QVector>

#include "Glm.h"

#include "Terrain/MapTile.h"

#define STL_DIMENSION 64

class Vertex
{
public:
    Vertex();
    Vertex(glm::vec3 vertex1, glm::vec3 vertex2, glm::vec3 vertex3, glm::vec3 vertex4);

    void scale(const float value);

    QVector<glm::vec3> getVertexs();

private:
    glm::vec3 vertexs[4];
};

class Face
{
public:
    enum Facets
    {
        South  = 0,
        Left   = 1,
        Top    = 2,
        Right  = 3,
        Bottom = 4,
        North  = 5
    };

    Face();
    Face(Facets face);

    void createVertices();

    void optimalize();

    void setInputData(QVector<glm::vec3>& data);

    void writeToFile(QTextStream* stream, float scale);

private:
    Facets facet;

    QFuture<QVector<Vertex>> future;
};

class STL
{
public:
    enum Resolution
    {
        Low,
        High
    };

    STL(MapTile* mapTile, const float scale, const float baseHeight = 5.0f, Resolution resolution = Low);

    void save(const QString& fileName);

private:
    void process();

    MapTile* tile;

    Resolution resolution;

    float scale;
    float height;

    glm::vec3 data[STL_DIMENSION][STL_DIMENSION];
    glm::vec3 dataSouth[STL_DIMENSION][2];
    glm::vec3 dataNorth[STL_DIMENSION][2];
    glm::vec3 dataLeft[STL_DIMENSION][2];
    glm::vec3 dataRight[STL_DIMENSION][2];
    glm::vec3 dataBottom[2][2];

    Face faces[6];
};

#endif // STL_H
