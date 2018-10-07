#ifndef CAMERA_H
#define CAMERA_H

#include <QVector4D>
#include <QVector3D>
#include <QVector2D>
#include <QMatrix4x4>
#include <QQuaternion>

class Camera
{
public:
	Camera();
	~Camera();

	float getMovementSpeed() const { return movementSpeed; }
	float getRotationSpeed() const { return rotationSpeed; }

	QVector3D getPosition() const { return position; }
    QVector3D getRotation() const { return rotation; }

	QMatrix4x4 getProjectionMatrix() const { return projectionMatrix; }
	QMatrix4x4 getViewProjectionMatrix() const { return projectionMatrix * viewMatrix; }
    QMatrix4x4 getViewMatrix() const { return viewMatrix; }

	void move(const QVector3D& pos);

	void rotate(const QQuaternion& quaternion);

    void yaw(const float& angle);
    void pitch(const float& angle);
	void roll(const float& angle);

    QQuaternion yawRotation(const float& angle) const;
    QQuaternion pitchRotation(const float& angle) const;
    QQuaternion rollRotation(const float& angle) const;

	void setNearPlane(const float& value);
	void setFarPlane(const float& value);
	void setFieldOfView(const float& value);
	void setAspectRatio(const float& value);
    void setPerspectiveProjection(const QMatrix4x4& clipCorrectionMatrix, const float& FOV, const float& AR, const float& nPlane, const float& fPlane);

	void setMovementSpeed(const float& value) { movementSpeed = value; }
	void setRotationSpeed(const float& value) { rotationSpeed = value; }

    void translate(const QVector3D& delta);

    void strafe(const float& amount);
    void walk(const float& amount);

private:
	float nearPlane = 0.1f;
	float farPlane = 1024.0f;
	float fieldOfView = 60.0f;
	float aspectRatio = 1.0f;

    QVector3D rotation = QVector3D(); // x = yaw, y = pitch, z = roll

	float movementSpeed = 1.0f;
	float rotationSpeed = 1.0f;

    QVector3D position = QVector3D(0.0f, 0.0f, 20.0f);
    QVector3D viewCenter = QVector3D(1.0f, 0.0f, 0.0f); // right
    QVector3D toCenter = QVector3D(0.0f, 0.0f, -1.0f); // forward
	QVector3D up = QVector3D(0.0f, 1.0f, 0.0f);

    QMatrix4x4 yawMatrix;
    QMatrix4x4 pitchMatrix;
    QMatrix4x4 rollMatrix;

	QMatrix4x4 viewMatrix;
	QMatrix4x4 projectionMatrix;

    void clamp360(float* variable);

	void updateViewMatrix();
    void updateProjectionMatrix();
};

#endif // CAMERA_H
