#include "Frustum.h"

#include <QVector>

bool Frustum::check(const QVector3D& min, const QVector3D& max) const
{
    QVector<QVector3D> points;
    points.push_back(QVector3D(min.x(), min.y(), min.z()));
    points.push_back(QVector3D(min.x(), min.y(), max.z()));
    points.push_back(QVector3D(min.x(), max.y(), min.z()));
    points.push_back(QVector3D(min.x(), max.y(), max.z()));
    points.push_back(QVector3D(max.x(), min.y(), min.z()));
    points.push_back(QVector3D(max.x(), min.y(), max.z()));
    points.push_back(QVector3D(max.x(), max.y(), min.z()));
    points.push_back(QVector3D(max.x(), max.y(), max.z()));
}

bool Frustum::checkSphere(const QVector3D& position, const float& radius) const
{
    for (auto i = 0; i < planes.size(); ++i)
    {
        if ((planes[i].x() * position.x()) + (planes[i].y() * position.y()) + (planes[i].z() * position.z()) + planes[i].w() <= -radius)
            return false;
    }

    return true;
}

void Frustum::update(const QMatrix4x4& matrix)
{
    planes[Left].setX(matrix.row(0).w() + matrix.row(0).x());
    planes[Left].setY(matrix.row(1).w() + matrix.row(1).x());
    planes[Left].setZ(matrix.row(2).w() + matrix.row(2).x());
    planes[Left].setW(matrix.row(3).w() + matrix.row(3).x());

    planes[Right].setX(matrix.row(0).w() - matrix.row(0).x());
    planes[Right].setY(matrix.row(1).w() - matrix.row(1).x());
    planes[Right].setZ(matrix.row(2).w() - matrix.row(2).x());
    planes[Right].setW(matrix.row(3).w() - matrix.row(3).x());

    planes[Top].setX(matrix.row(0).w() - matrix.row(0).y());
    planes[Top].setY(matrix.row(1).w() - matrix.row(1).y());
    planes[Top].setZ(matrix.row(2).w() - matrix.row(2).y());
    planes[Top].setW(matrix.row(3).w() - matrix.row(3).y());

    planes[Bottom].setX(matrix.row(0).w() + matrix.row(0).y());
    planes[Bottom].setY(matrix.row(1).w() + matrix.row(1).y());
    planes[Bottom].setZ(matrix.row(2).w() + matrix.row(2).y());
    planes[Bottom].setW(matrix.row(3).w() + matrix.row(3).y());

    planes[Back].setX(matrix.row(0).w() + matrix.row(0).z());
    planes[Back].setY(matrix.row(1).w() + matrix.row(1).z());
    planes[Back].setZ(matrix.row(2).w() + matrix.row(2).z());
    planes[Back].setW(matrix.row(3).w() + matrix.row(3).z());

    planes[Front].setX(matrix.row(0).w() - matrix.row(0).z());
    planes[Front].setY(matrix.row(1).w() - matrix.row(1).z());
    planes[Front].setZ(matrix.row(2).w() - matrix.row(2).z());
    planes[Front].setW(matrix.row(3).w() - matrix.row(3).z());

    for (auto i = 0; i < planes.size(); i++)
    {
        float length = sqrtf(planes[i].x() * planes[i].x() + planes[i].y() * planes[i].y() + planes[i].z() * planes[i].z());

        planes[i] /= length;
    }
}
