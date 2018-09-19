#include "Camera.h"

Camera::Camera()
{
	updateViewMatrix();
}

Camera::~Camera()
{
}

void Camera::move(const QVector3D& pos)
{
	position = pos;
	viewCenter = QVector3D(pos.x(), pos.y(), pos.z() - 1.0f);
	up = QVector3D(0.0f, 1.0f, 0.0f);
}

void Camera::rotate(const QQuaternion& quaternion)
{
	up = quaternion.rotatedVector(up);
	toCenter = quaternion.rotatedVector(toCenter);
	viewCenter = position + toCenter;
}

void Camera::pan(const float& angle)
{
	rotate(panRotation(-angle));
}

void Camera::tilt(const float& angle)
{
	rotate(tiltRotation(angle));
}

void Camera::roll(const float& angle)
{
	rotate(rollRotation(-angle));
}

QQuaternion& Camera::panRotation(const float& angle) const
{
	return QQuaternion::fromAxisAndAngle(up, angle);
}

QQuaternion& Camera::tiltRotation(const float& angle) const
{
	QVector3D xBasis = QVector3D::crossProduct(up, toCenter.normalized()).normalized();

	return QQuaternion::fromAxisAndAngle(xBasis, -angle);
}

QQuaternion& Camera::rollRotation(const float& angle) const
{
	return QQuaternion::fromAxisAndAngle(toCenter, -angle);
}

void Camera::setNearPlane(const float& value)
{
	if (qFuzzyCompare(nearPlane, value))
		return;

	nearPlane = value;

	if(projectionType == PerspectiveProjection)
		updateProjectionMatrix();
}

void Camera::setFarPlane(const float& value)
{
	if (qFuzzyCompare(farPlane, value))
		return;

	farPlane = value;

	if (projectionType == PerspectiveProjection)
		updateProjectionMatrix();
}

void Camera::setFieldOfView(const float& value)
{
	if (qFuzzyCompare(fieldOfView, value))
		return;

	fieldOfView = value;

	if (projectionType == PerspectiveProjection)
		updateProjectionMatrix();
}

void Camera::setAspectRatio(const float& value)
{
	if (qFuzzyCompare(aspectRatio, value))
		return;

	aspectRatio = value;

	if (projectionType == PerspectiveProjection)
		updateProjectionMatrix();
}

void Camera::setPerspectiveProjection(const float& FOV, const float& AR, const float& nPlane, const float& fPlane)
{
	fieldOfView = FOV;
	aspectRatio = AR;
	nearPlane = nPlane;
	farPlane = fPlane;
	projectionType = PerspectiveProjection;

	updateProjectionMatrix();
}

void Camera::translate(const QVector3D& delta, CameraTranslationOption option)
{
	// Calculate the amount to move by in world coordinates
	QVector3D vWorld;

	if (!qFuzzyIsNull(delta.x()))
	{
		// Calculate the vector for the local x axis
		QVector3D x = QVector3D::crossProduct(toCenter, up).normalized();

		vWorld += delta.x() * x;
	}

	if (!qFuzzyIsNull(delta.y()))
		vWorld += delta.y() * up;

	if (!qFuzzyIsNull(delta.z()))
		vWorld += delta.z() * toCenter.normalized();

	// Update the camera position using the calculated world vector
	position += vWorld;

	// May be also update the view center coordinates
	if (option == TranslateViewCenter)
		viewCenter += vWorld;

	// Refresh the camera -> view center vector
	toCenter = viewCenter - position;

	// Calculate a new up vector. We do this by:
	// 1) Calculate a new local x-direction vector from the cross product of the new
	//    camera to view center vector and the old up vector.
	// 2) The local x vector is the normal to the plane in which the new up vector
	//    must lay. So we can take the cross product of this normal and the new
	//    x vector. The new normal vector forms the last part of the orthonormal basis
	QVector3D x = QVector3D::crossProduct(toCenter, up).normalized();

	up = QVector3D::crossProduct(x, toCenter).normalized();

	updateViewMatrix();
}

void Camera::updateProjectionMatrix()
{
	projectionMatrix.setToIdentity();
	projectionMatrix.perspective(fieldOfView, aspectRatio, nearPlane, farPlane);
}

void Camera::updateViewMatrix()
{
	viewMatrix.setToIdentity();
	viewMatrix.lookAt(position, viewCenter, up);
}