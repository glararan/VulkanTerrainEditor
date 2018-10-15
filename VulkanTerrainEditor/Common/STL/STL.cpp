#include "STL.h"

#include <QDebug>
#include <QFile>

// Cube normals
#define NORMALS_TOP    glm::vec3( 0,  1,  0)
#define NORMALS_BOTTOM glm::vec3( 0, -1,  0)
#define NORMALS_LEFT   glm::vec3(-1,  0,  0)
#define NORMALS_RIGHT  glm::vec3( 1,  0,  0)
#define NORMALS_NORTH  glm::vec3( 0,  0,  1)
#define NORMALS_SOUTH  glm::vec3( 0,  0, -1)

STL::STL(MapTile* mapTile, const float scale, const float baseHeight, Resolution resolution) : tile(mapTile), scale(scale), height(baseHeight), resolution(resolution)
{
    for(int i = 0; i < 6; ++i)
        faces[i] = Face((Face::Facets)i);
}

void STL::process()
{
    if(tile)
    {
        const int size = resolution == High ? 1024.0f : 256.0f;

        float heightDiff = fabs(height - fabs(tile->getHeightmap()->getLowestHeight()));

        int dimension = tile->getHeightmap()->getDimension();

        for(int x = 0; x < dimension; ++x)
        {
            for(int y = 0; y < dimension; ++y)
            {
                glm::vec3 vertex(x, tile->getHeightmap()->getHeight(x, y) + heightDiff, y);

                data[x][y] = vertex;
            }
        }

        if(resolution == Low)
        {
            for(int y = 0; y < dimension; y += 4)
            {
                for(int x = 0; x < dimension; x += 4)
                {
                    float _height = data[x + 0][y + 0].y + data[x + 1][y + 0].y + data[x + 2][y + 0].y + data[x + 3][y + 0].y
                                  + data[x + 0][y + 1].y + data[x + 1][y + 1].y + data[x + 2][y + 1].y + data[x + 3][y + 1].y
                                  + data[x + 0][y + 2].y + data[x + 1][y + 2].y + data[x + 2][y + 2].y + data[x + 3][y + 2].y
                                  + data[x + 0][y + 3].y + data[x + 1][y + 3].y + data[x + 2][y + 3].y + data[x + 3][y + 3].y;
                    _height      /= 16.0f;

                    data[x / 4][y / 4].y = _height;
                }
            }

            for(int y = 0; y < size; ++y)
            {
                for(int x = 0; x < size; ++x)
                {
                    float xPos = float(x) / float(size - 1) * 512.0f;
                    float zPos = float(y) / float(size - 1) * 512.0f;

                    data[x][y].x = xPos;
                    data[x][y].z = zPos;
                }
            }
        }
        else
        {

        }

        QVector<glm::vec3> datas;

        for(int y = 0; y < size; ++y)
        {
            for(int x = 0; x < size; ++x)
                datas.append(data[x][y]);
        }

        faces[(int)Face::Top].setInputData(datas);

        /// South
        datas.clear();

        for(int y = 0; y < 2; ++y)
        {
            for(int x = 0; x < size; ++x)
            {
                if(y != 0)
                    dataSouth[x][y] = glm::vec3();
                else
                    dataSouth[x][0] = data[x][size - 1];
            }
        }

        for(int y = 0; y < 2; ++y)
        {
            for(int x = 0; x < size; ++x)
                datas.append(dataSouth[x][y]);
        }

        faces[(int)Face::South].setInputData(datas);

        /// Left
        datas.clear();

        for(int y = 0; y < 2; ++y)
        {
            for(int x = 0; x < size; ++x)
            {
                if(y != 0)
                    dataLeft[x][y] = glm::vec3();
                else
                    dataLeft[x][0] = data[0][x];
            }
        }

        for(int y = 0; y < 2; ++y)
        {
            for(int x = 0; x < size; ++x)
                datas.append(dataLeft[x][y]);
        }

        faces[(int)Face::Left].setInputData(datas);

        /// Right
        datas.clear();

        for(int y = 0; y < 2; ++y)
        {
            for(int x = 0; x < size; ++x)
            {
                if(y != 0)
                    dataRight[x][y] = glm::vec3();
                else
                    dataRight[x][0] = data[size - 1][x];
            }
        }

        for(int y = 0; y < 2; ++y)
        {
            for(int x = 0; x < size; ++x)
                datas.append(dataRight[x][y]);
        }

        faces[(int)Face::Right].setInputData(datas);

        /// Bottom
        datas.clear();

        // maybe switch X's, now just for proper display in sketchup
        dataBottom[0][0] = glm::vec3(size, 0.0f, 0.0f);
        dataBottom[1][0] = glm::vec3(0.0f, 0.0f, 0.0f);
        dataBottom[0][1] = glm::vec3(size, 0.0f, size);
        dataBottom[1][1] = glm::vec3(0.0f, 0.0f, size);

        for(int y = 0; y < 2; ++y)
        {
            for(int x = 0; x < 2; ++x)
                datas.append(dataBottom[x][y]);
        }

        faces[(int)Face::Bottom].setInputData(datas);

        /// North
        datas.clear();

        for(int y = 0; y < 2; ++y)
        {
            for(int x = 0; x < size; ++x)
            {
                if(y != 0)
                    dataNorth[x][y] = glm::vec3();
                else
                    dataNorth[x][0] = data[size - 1 - x][0];
            }
        }

        for(int y = 0; y < 2; ++y)
        {
            for(int x = 0; x < size; ++x)
                datas.append(dataNorth[x][y]);
        }

        faces[(int)Face::North].setInputData(datas);

        /// Generate vertices
        for(auto& face : faces)
            face.createVertices();

        /// Optimalize vertices
        for(auto& face : faces)
            face.optimalize();
    }
}

void STL::save(const QString& fileName)
{
    QFile file(fileName);

    if(file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        process();

        QTextStream out(&file);

        out << "solid map" << endl;

        for(auto& face : faces)
            face.writeToFile(&out, scale);

        out << "endsolid map";

        out.flush();

        qDebug() << QObject::tr("Successful data write to") << file.fileName();

        file.close();
    }
}

Face::Face()
{
}

Face::Face(Facets face) : facet(face)
{
}

void Face::createVertices()
{
    //const int size =
}

void Face::optimalize()
{

}

void Face::setInputData(QVector<glm::vec3>& data)
{

}

void Face::writeToFile(QTextStream* stream, float scale)
{
    future.waitForFinished();

    QVector<Vertex> vertexs = future.result();

    for(auto& vertex : vertexs)
        vertex.scale(scale);

    for(auto& vertex: vertexs)
    {
        QVector<glm::vec3> vertices = vertex.getVertexs();

        glm::vec3 normal;

        switch(facet)
        {
        case South:
            normal = NORMALS_SOUTH;
            break;

        case Left:
            normal = NORMALS_LEFT;
            break;

        case Top:
            normal = NORMALS_TOP;
            break;

        case Right:
            normal = NORMALS_RIGHT;
            break;

        case Bottom:
            normal = NORMALS_BOTTOM;
            break;

        case North:
            normal = NORMALS_NORTH;
            break;
        }

        QString facet_normal = QString("facet normal %1 %2 %3").arg(normal.x).arg(normal.y).arg(normal.z);

        *stream << facet_normal << endl;
        *stream << "  outer loop" << endl;
        *stream << "    vertex  " << vertices[0].x << "  " << vertices[0].y << " " << vertices[0].z << endl;
        *stream << "    vertex  " << vertices[3].x << "  " << vertices[3].y << " " << vertices[3].z << endl;
        *stream << "    vertex  " << vertices[2].x << "  " << vertices[2].y << " " << vertices[2].z << endl;
        *stream << "  endloop" << endl;
        *stream << "endfacet" << endl;

        *stream << facet_normal << endl;
        *stream << "  outer loop" << endl;
        *stream << "    vertex  " << vertices[0].x << "  " << vertices[0].y << " " << vertices[0].z << endl;
        *stream << "    vertex  " << vertices[1].x << "  " << vertices[1].y << " " << vertices[1].z << endl;
        *stream << "    vertex  " << vertices[3].x << "  " << vertices[3].y << " " << vertices[3].z << endl;
        *stream << "  endloop" << endl;
        *stream << "endfacet" << endl;
    }
}

Vertex::Vertex()
{
}

Vertex::Vertex(glm::vec3 vertex1, glm::vec3 vertex2, glm::vec3 vertex3, glm::vec3 vertex4)
{
    vertexs[0] = vertex1;
    vertexs[1] = vertex2;
    vertexs[2] = vertex3;
    vertexs[3] = vertex4;
}

QVector<glm::vec3> Vertex::getVertexs()
{
    QVector<glm::vec3> vector;

    for(auto& vertex : vertexs)
        vector.append(vertex);

    return vector;
}

void Vertex::scale(const float value)
{
    for(auto& vertex : vertexs)
        vertex *= value;
}
