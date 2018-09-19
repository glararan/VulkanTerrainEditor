#ifndef FRUSTUM_H
#define FRUSTUM_H

#include <QVarLengthArray>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>

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

    bool check(const QVector3D& min, const QVector3D& max) const;
    bool checkSphere(const QVector3D& position, const float& radius) const;

    void update(const QMatrix4x4& matrix);

private:
    QVarLengthArray<QVector4D, 6> planes;
};

#endif // FRUSTUM_H
