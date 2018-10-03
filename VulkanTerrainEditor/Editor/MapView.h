#ifndef MAPVIEW_H
#define MAPVIEW_H

#include <QVulkanWindow>

class MapViewRenderer;

class MapView : public QVulkanWindow
{
	Q_OBJECT

public:
	MapView();
	~MapView();

	QVulkanWindowRenderer* createRenderer() override;

    MapViewRenderer* getRenderer() { return renderer; }

signals:
    void cameraChanged();

private:
	void mousePressEvent(QMouseEvent*) override;
	void mouseReleaseEvent(QMouseEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
	void keyPressEvent(QKeyEvent*) override;

	MapViewRenderer* renderer = Q_NULLPTR;
	
	float fpsTimer = 0.0f;

	quint32 frameCounter = 0;
	quint32 lastFPS = 0;

	bool mousePressed = false;

	QPoint lastMousePosition;
};

#endif // MAPVIEW_H
