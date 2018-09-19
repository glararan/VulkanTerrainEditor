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
	enum ProjectionType
	{
		OrthogonalProjection,
		PerspectiveProjection
	};

	enum CameraTranslationOption
	{
		TranslateViewCenter,
		DontTranslateViewCenter
	};

	Camera();
	~Camera();

	float getMovementSpeed() const { return movementSpeed; }
	float getRotationSpeed() const { return rotationSpeed; }

	QVector3D getPosition() const { return position; }

	QMatrix4x4 getProjectionMatrix() const { return projectionMatrix; }
	QMatrix4x4 getViewProjectionMatrix() const { return projectionMatrix * viewMatrix; }
	QMatrix4x4 getViewMatrix() const { return viewMatrix; }
	
	ProjectionType getProjectionType() const { return projectionType; }

	void move(const QVector3D& pos);

	void rotate(const QQuaternion& quaternion);

	void pan(const float& angle);
	void tilt(const float& angle);
	void roll(const float& angle);

	QQuaternion& panRotation(const float& angle) const;
	QQuaternion& tiltRotation(const float& angle) const;
	QQuaternion& rollRotation(const float& angle) const;

	void setNearPlane(const float& value);
	void setFarPlane(const float& value);
	void setFieldOfView(const float& value);
	void setAspectRatio(const float& value);
	void setPerspectiveProjection(const float& FOV, const float& AR, const float& nPlane, const float& fPlane);

	void setMovementSpeed(const float& value) { movementSpeed = value; }
	void setRotationSpeed(const float& value) { rotationSpeed = value; }

	void translate(const QVector3D& delta, CameraTranslationOption = TranslateViewCenter);

private:
	ProjectionType projectionType = PerspectiveProjection;

	float nearPlane = 0.1f;
	float farPlane = 1024.0f;
	float fieldOfView = 60.0f;
	float aspectRatio = 1.0f;

	float movementSpeed = 1.0f;
	float rotationSpeed = 1.0f;

	QVector3D position = QVector3D(0.0f, 0.0f, 1.0f);
	QVector3D viewCenter = QVector3D(0.0f, 0.0f, 0.0f);
	QVector3D toCenter = QVector3D(0.0f, 0.0f, -1.0f);
	QVector3D up = QVector3D(0.0f, 1.0f, 0.0f);

	QMatrix4x4 viewMatrix;
	QMatrix4x4 projectionMatrix;

	void updateViewMatrix();
	void updateProjectionMatrix();
};

#endif // CAMERA_H