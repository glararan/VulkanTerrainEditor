#ifndef MAPVIEW_H
#define MAPVIEW_H

#include "VulkanWindow.h"

class World;

class MapView : public VulkanWindow
{
	Q_OBJECT

public:
    MapView(QVulkanInstance* vulkanInstance);
	~MapView();

signals:
    void cameraChanged();

private:
	void mousePressEvent(QMouseEvent*) override;
	void mouseReleaseEvent(QMouseEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

    void buildCommandBuffers() override;
    void initializeResources() override;
    void releaseResources() override;
    void render() override;

	bool mousePressed = false;

	QPoint lastMousePosition;

    World* world;
};

#endif // MAPVIEW_H
