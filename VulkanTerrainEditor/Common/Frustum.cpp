#include "Frustum.h"

#include <QVector>

bool Frustum::check(const glm::vec3& min, const glm::vec3& max) const
{
    QVector<glm::vec3> points;
    points.push_back(glm::vec3(min.x, min.y, min.z));
    points.push_back(glm::vec3(min.x, min.y, max.z));
    points.push_back(glm::vec3(min.x, max.y, min.z));
    points.push_back(glm::vec3(min.x, max.y, max.z));
    points.push_back(glm::vec3(max.x, min.y, min.z));
    points.push_back(glm::vec3(max.x, min.y, max.z));
    points.push_back(glm::vec3(max.x, max.y, min.z));
    points.push_back(glm::vec3(max.x, max.y, max.z));

	return false;
}

bool Frustum::checkSphere(const glm::vec3& position, const float& radius) const
{
    for (auto i = 0; i < planes.size(); ++i)
    {
        if ((planes[i].x * position.x) + (planes[i].y * position.y) + (planes[i].z * position.z) + planes[i].w <= -radius)
            return false;
    }

    return true;
}

void Frustum::update(const glm::mat4x4& matrix)
{
    planes.resize(6);

    planes[Left].x = matrix[0].w + matrix[0].x;
    planes[Left].y = matrix[1].w + matrix[1].x;
    planes[Left].z = matrix[2].w + matrix[2].x;
    planes[Left].w = matrix[3].w + matrix[3].x;

    planes[Right].x = matrix[0].w - matrix[0].x;
    planes[Right].y = matrix[1].w - matrix[1].x;
    planes[Right].z = matrix[2].w - matrix[2].x;
    planes[Right].w = matrix[3].w - matrix[3].x;

    planes[Top].x = matrix[0].w - matrix[0].y;
    planes[Top].y = matrix[1].w - matrix[1].y;
    planes[Top].z = matrix[2].w - matrix[2].y;
    planes[Top].w = matrix[3].w - matrix[3].y;

    planes[Bottom].x = matrix[0].w + matrix[0].y;
    planes[Bottom].y = matrix[1].w + matrix[1].y;
    planes[Bottom].z = matrix[2].w + matrix[2].y;
    planes[Bottom].w = matrix[3].w + matrix[3].y;

    planes[Back].x = matrix[0].w + matrix[0].z;
    planes[Back].y = matrix[1].w + matrix[1].z;
    planes[Back].z = matrix[2].w + matrix[2].z;
    planes[Back].w = matrix[3].w + matrix[3].z;

    planes[Front].x = matrix[0].w - matrix[0].z;
    planes[Front].y = matrix[1].w - matrix[1].z;
    planes[Front].z = matrix[2].w - matrix[2].z;
    planes[Front].w = matrix[3].w - matrix[3].z;

    for (auto i = 0; i < planes.size(); i++)
    {
        float length = sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);

        planes[i] /= length;
    }
}

glm::vec4 Frustum::getPlane(const int index) const
{
    if(index > 6 || index < 0)
        return glm::vec4();

    return planes[index];
}
