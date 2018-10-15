#ifndef CAMERA_H
#define CAMERA_H

#include <QVector4D>
#include <QVector3D>
#include <QVector2D>
#include <QMatrix4x4>
#include <QQuaternion>

#include "Glm.h"

class Camera
{
public:
	Camera();
	~Camera();

	float getMovementSpeed() const { return movementSpeed; }
	float getRotationSpeed() const { return rotationSpeed; }

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getRotation() const { return rotation; }

    glm::mat4x4 getProjectionMatrix() const { return projectionMatrix; }
    glm::mat4x4 getViewProjectionMatrix() const { return projectionMatrix * viewMatrix; }
    glm::mat4x4 getViewMatrix() const { return viewMatrix; }

    void move(const glm::vec3& pos);

    void rotate(const glm::vec3& delta);

    /*void yaw(const float& angle);
    void pitch(const float& angle);
	void roll(const float& angle);

    QQuaternion yawRotation(const float& angle) const;
    QQuaternion pitchRotation(const float& angle) const;
    QQuaternion rollRotation(const float& angle) const;*/

	void setNearPlane(const float& value);
	void setFarPlane(const float& value);
	void setFieldOfView(const float& value);
	void setAspectRatio(const float& value);
    void setPerspectiveProjection(const float& FOV, const float& AR, const float& nPlane, const float& fPlane);

	void setMovementSpeed(const float& value) { movementSpeed = value; }
	void setRotationSpeed(const float& value) { rotationSpeed = value; }

    void translate(const glm::vec3& delta);

    void strafe(const float& amount);
    void walk(const float& amount);

private:
	float nearPlane = 0.1f;
	float farPlane = 1024.0f;
	float fieldOfView = 60.0f;
	float aspectRatio = 1.0f;

    glm::vec3 rotation = glm::vec3(-12.0f, 159.0f, 0.0f); // x = yaw, y = pitch, z = roll

	float movementSpeed = 1.0f;
	float rotationSpeed = 1.0f;

    glm::vec3 position = glm::vec3(18.0f, 22.5f, 57.5f);
    glm::vec3 viewCenter = glm::vec3(1.0f, 0.0f, 0.0f); // right
    glm::vec3 toCenter = glm::vec3(0.0f, 0.0f, -1.0f); // forward
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::mat4x4 yawMatrix;
    glm::mat4x4 pitchMatrix;
    glm::mat4x4 rollMatrix;

    glm::mat4x4 viewMatrix;
    glm::mat4x4 projectionMatrix;

    void clamp360(float* variable);

	void updateViewMatrix();
    void updateProjectionMatrix();
};

#endif // CAMERA_H
