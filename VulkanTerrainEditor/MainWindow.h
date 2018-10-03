#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QVulkanInstance>

#include "ui_MainWindow.h"

#include "Editor/MapView.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
    MainWindow(QVulkanInstance* vulkanInstance, QWidget* parent = Q_NULLPTR);
	~MainWindow();

public slots:
    void updateStatusBar();

private:
	Ui::MainWindowClass ui;

    MapView* mapView = Q_NULLPTR;
};

#endif // MAINWINDOW_H
