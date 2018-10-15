#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QVulkanInstance>

#include "MainWindow.h"

class Application : public QApplication
{
	Q_OBJECT

public:
	Application(int& argc, char**);
	~Application();

	QVulkanInstance* getVkInstance() const { return vulkanInstance; }

private:
    MainWindow* mainWindow = Q_NULLPTR;

	QVulkanInstance* vulkanInstance = Q_NULLPTR;
};

Application& app();

#endif // APPLICATION_H
