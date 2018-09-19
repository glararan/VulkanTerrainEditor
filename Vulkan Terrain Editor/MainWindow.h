#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>

#include "ui_MainWindow.h"

#include "Editor/MapView.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget* parent = Q_NULLPTR);
	~MainWindow();

private:
	Ui::MainWindowClass ui;

	MapView* mapView = Q_NULLPTR;
};

#endif // MAINWINDOW_H
