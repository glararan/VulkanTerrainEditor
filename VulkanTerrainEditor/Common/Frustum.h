#ifndef FRUSTUM_H
#define FRUSTUM_H

#include <QVarLengthArray>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>

#include "Glm.h"

class Frustum
{
public:
    enum Side
    {
        Left   = 0,
        Right  = 1,
        Top    = 2,
        Bottom = 3,
        Back   = 4,
        Front  = 5
    };

    bool check(const glm::vec3& min, const glm::vec3& max) const;
    bool checkSphere(const glm::vec3& position, const float& radius) const;

    void update(const glm::mat4x4& matrix);

    glm::vec4 getPlane(const int index) const;

private:
    QVarLengthArray<glm::vec4, 6> planes;
};

#endif // FRUSTUM_H
