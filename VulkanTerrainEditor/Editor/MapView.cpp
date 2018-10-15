#include "MapView.h"

#include "World.h"

#include "Vulkan/Tools.h"

#include <QDebug>
#include <QFile>
#include <QMouseEvent>

MapView::MapView(QVulkanInstance* vulkanInstance) : VulkanWindow(vulkanInstance), world(new World())
{
    camera.setMovementSpeed(7.5f);
}

MapView::~MapView()
{
    delete world;
}

void MapView::initializeResources()
{
    camera.setPerspectiveProjection(45.0f, (float)width() / (float)height(), 0.1f, 512.0f);

    world->create();
}

void MapView::releaseResources()
{
    world->destroy();

    VulkanWindow::releaseResources();
}

void MapView::mousePressEvent(QMouseEvent* event)
{
	mousePressed = true;

	lastMousePosition = event->pos();
}

void MapView::mouseReleaseEvent(QMouseEvent*)
{
    mousePressed = false;
}

void MapView::mouseMoveEvent(QMouseEvent* event)
{
	if (!mousePressed)
		return;

	int dx = event->pos().x() - lastMousePosition.x();
	int dy = event->pos().y() - lastMousePosition.y();

    if(dx || dy)
        camera.rotate(glm::vec3(-dy, dx, 0.0f));

    emit cameraChanged();

	lastMousePosition = event->pos();
}

void MapView::keyPressEvent(QKeyEvent* event)
{
	const float amount = event->modifiers().testFlag(Qt::ShiftModifier) ? 1.0f : 0.1f;

	switch (event->key())
	{
	case Qt::Key_W:
        camera.walk(amount);
        emit cameraChanged();
		break;

	case Qt::Key_S:
        camera.walk(-amount);
        emit cameraChanged();
		break;

	case Qt::Key_A:
        camera.strafe(-amount);
        emit cameraChanged();
		break;

	case Qt::Key_D:
        camera.strafe(amount);
        emit cameraChanged();
		break;

	default:
		break;
	}
}

void MapView::buildCommandBuffers()
{
    world->buildCommandBuffers();
}

void MapView::render()
{
    VulkanWindow::prepareFrame();

    world->draw(camera);

    // Command buffer to be sumitted to the queue
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

    // Submit to queue
    VK_CHECK_RESULT(vkDevFunctions->vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

    VulkanWindow::submitFrame();
}
