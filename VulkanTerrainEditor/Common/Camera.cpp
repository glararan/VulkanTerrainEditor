#include "Camera.h"

Camera::Camera()
{
	updateViewMatrix();
}

Camera::~Camera()
{
}

void Camera::clamp360(float* variable)
{
    while(*variable > 360.0f)
        *variable -= 360.0f;

    while(*variable < -360.0f)
        *variable += 360.0f;
}

void Camera::move(const glm::vec3& pos)
{
	position = pos;
    viewCenter = glm::vec3(pos.x, pos.y, pos.z - 1.0f);
    up = glm::vec3(0.0f, 1.0f, 0.0f);
}

void Camera::rotate(const glm::vec3& delta)
{
    rotation += glm::vec3(delta.x * rotationSpeed, delta.y * rotationSpeed, 0.0f);

    updateViewMatrix();
}

/*void Camera::yaw(const float& angle)
{
    rotation[0] += angle;

    clamp360(&rotation[0]);

    yawMatrix.setToIdentity();
    yawMatrix.rotate(rotation[0], 0, 1, 0);

    glm::mat4x4 rotationMatrix = pitchMatrix * yawMatrix;

    toCenter = (QVector4D(0.0f, 0.0f, -1.0f, 0.0f) * rotationMatrix).toVector3D();
    viewCenter = (QVector4D(1.0f, 0.0f, 0.0f, 0.0f) * rotationMatrix).toVector3D();

    updateViewMatrix();
}

void Camera::pitch(const float& angle)
{
    rotation[1] += angle;

    clamp360(&rotation[1]);

    pitchMatrix.setToIdentity();
    pitchMatrix.rotate(rotation[1], 1, 0, 0);

    glm::mat4x4 rotationMatrix = pitchMatrix * yawMatrix;

    toCenter = (QVector4D(0.0f, 0.0f, -1.0f, 0.0f) * rotationMatrix).toVector3D();
    viewCenter = (QVector4D(0.0f, 1.0f, 0.0f, 0.0f) * rotationMatrix).toVector3D();

    updateViewMatrix();
}

void Camera::roll(const float& angle)
{
    rotation[2] += angle;

    clamp360(&rotation[2]);

    rollMatrix.setToIdentity();
    rollMatrix.rotate(rotation[2], 0, 0, 1);

    glm::mat4x4 rotationMatrix = pitchMatrix * yawMatrix;

    toCenter = (QVector4D(0.0f, 0.0f, -1.0f, 0.0f) * rotationMatrix).toVector3D();
    viewCenter = (QVector4D(0.0f, 0.0f, 1.0f, 0.0f) * rotationMatrix).toVector3D();

    updateViewMatrix();
}*/

void Camera::setNearPlane(const float& value)
{
	if (qFuzzyCompare(nearPlane, value))
		return;

	nearPlane = value;

    updateProjectionMatrix();
}

void Camera::setFarPlane(const float& value)
{
	if (qFuzzyCompare(farPlane, value))
		return;

	farPlane = value;

    updateProjectionMatrix();
}

void Camera::setFieldOfView(const float& value)
{
	if (qFuzzyCompare(fieldOfView, value))
		return;

	fieldOfView = value;

    updateProjectionMatrix();
}

void Camera::setAspectRatio(const float& value)
{
	if (qFuzzyCompare(aspectRatio, value))
		return;

	aspectRatio = value;

    updateProjectionMatrix();
}

void Camera::setPerspectiveProjection(const float& FOV, const float& AR, const float& nPlane, const float& fPlane)
{
	fieldOfView = FOV;
	aspectRatio = AR;
	nearPlane = nPlane;
    farPlane = fPlane;

    updateProjectionMatrix();
}

void Camera::translate(const glm::vec3& delta)
{
	// Calculate the amount to move by in world coordinates
    /*QVector3D vWorld;

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

    up = QVector3D::crossProduct(x, toCenter).normalized();*/

    position += delta;

	updateViewMatrix();
}

void Camera::strafe(const float& amount)
{
    glm::vec3 camFront;
    camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
    camFront.y = sin(glm::radians(rotation.x));
    camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
    camFront = glm::normalize(camFront);

    float moveSpeed = amount * movementSpeed;

    position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

    updateViewMatrix();
}

void Camera::walk(const float& amount)
{
    glm::vec3 camFront;
    camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
    camFront.y = sin(glm::radians(rotation.x));
    camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
    camFront = glm::normalize(camFront);

    float moveSpeed = amount * movementSpeed;

    position += camFront * moveSpeed;

    updateViewMatrix();
}

void Camera::updateProjectionMatrix()
{
    projectionMatrix = glm::perspective(fieldOfView, aspectRatio, nearPlane, farPlane);
}

void Camera::updateViewMatrix()
{
    glm::mat4x4 rotationMatrix = glm::mat4x4(1.0f);
    glm::mat4x4 translationMatrix = glm::translate(glm::mat4x4(1.0f), position);

    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    viewMatrix = rotationMatrix * translationMatrix;//translationMatrix * rotationMatrix;
}
