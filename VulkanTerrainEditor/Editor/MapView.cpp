#include "MapView.h"

#include "MapViewRenderer.h"

#include "Vulkan/Tools.h"

#include <QDebug>
#include <QFile>
#include <QMouseEvent>

MapView::MapView()
{
}

MapView::~MapView()
{
}

QVulkanWindowRenderer* MapView::createRenderer()
{
	renderer = new MapViewRenderer(this, true);

	return renderer;
}

void MapView::mousePressEvent(QMouseEvent* event)
{
	mousePressed = true;

	lastMousePosition = event->pos();
}

void MapView::mouseReleaseEvent(QMouseEvent*)
{
	mousePressed = false;

    qDebug() << "viewMatrix: " << renderer->getCamera()->getViewMatrix();
    qDebug() << "position: " << renderer->getCamera()->getPosition();
}

void MapView::mouseMoveEvent(QMouseEvent* event)
{
	if (!mousePressed)
		return;

	int dx = event->pos().x() - lastMousePosition.x();
	int dy = event->pos().y() - lastMousePosition.y();

	if (dy)
        renderer->getCamera()->pitch(dy / 10.0f);

	if (dx)
		renderer->getCamera()->yaw(dx / 10.0f);

    emit cameraChanged();

	lastMousePosition = event->pos();
}

void MapView::keyPressEvent(QKeyEvent* event)
{
	const float amount = event->modifiers().testFlag(Qt::ShiftModifier) ? 1.0f : 0.1f;

	switch (event->key())
	{
	case Qt::Key_W:
        renderer->getCamera()->walk(amount);
        emit cameraChanged();
		break;

	case Qt::Key_S:
        renderer->getCamera()->walk(-amount);
        emit cameraChanged();
		break;

	case Qt::Key_A:
        renderer->getCamera()->strafe(-amount);
        emit cameraChanged();
		break;

	case Qt::Key_D:
        renderer->getCamera()->strafe(amount);
        emit cameraChanged();
		break;

	default:
		break;
	}
}
