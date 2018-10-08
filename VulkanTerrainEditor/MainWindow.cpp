#include "MainWindow.h"

#include "Editor/MapViewRenderer.h"
#include "Common/Camera.h"

MainWindow::MainWindow(QVulkanInstance* vulkanInstance, QWidget* parent) : QMainWindow(parent)
{
	ui.setupUi(this);

	mapView = new MapView();
    mapView->setVulkanInstance(vulkanInstance);

    QWidget* wrapper = QWidget::createWindowContainer((QWindow*)mapView);
    wrapper->setFocusPolicy(Qt::StrongFocus);
    wrapper->setFocus();

    setCentralWidget(wrapper);

    connect(mapView, &MapView::cameraChanged, this, &MainWindow::updateStatusBar);
}

MainWindow::~MainWindow()
{
	if (mapView)
		delete mapView;
}

void MainWindow::updateStatusBar()
{
    Camera* camera = mapView->getRenderer()->getCamera();

    glm::vec3 position = camera->getPosition();
    glm::vec3 rotation = camera->getRotation();

    statusBar()->showMessage(QString("Camera (%1, %2, %3), Rotation (%4, %5, %6)").arg(position.x).arg(position.y).arg(position.z).arg(rotation.x).arg(rotation.y).arg(rotation.z));
}
