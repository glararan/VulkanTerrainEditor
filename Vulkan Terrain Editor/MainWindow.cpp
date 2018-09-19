#include "MainWindow.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
	ui.setupUi(this);

	mapView = new MapView();

	setCentralWidget(QWidget::createWindowContainer((QWindow*)mapView));
}

MainWindow::~MainWindow()
{
	if (mapView)
		delete mapView;
}
