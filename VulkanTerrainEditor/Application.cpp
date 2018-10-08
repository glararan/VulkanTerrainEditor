#include "Application.h"

#include <QLoggingCategory>
#include <QTime>

#include "Version.h"

Q_LOGGING_CATEGORY(lcVk, "qt.vulkan")

Application::Application(int& argc, char** arg) : QApplication(argc, arg)
{
	QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));

	qDebug() << qApp->applicationDirPath();

	setOrganizationDomain(QString(VER_COMPANYDOMAIN_STR));
	setOrganizationName(QString(VER_COMPANYNAME_STR));
	setApplicationName(QString(VER_PRODUCTNAME_STR));
	setApplicationVersion(QString(VER_FILEVERSION_STR));

    qsrand(QTime(0, 0, 0).secsTo(QTime::currentTime()));
	
	vulkanInstance = new QVulkanInstance();
	vulkanInstance->setLayers(QByteArrayList()
        << "VK_LAYER_LUNARG_standard_validation");
    vulkanInstance->setApiVersion(QVersionNumber(1, 1, 74));

	if (!vulkanInstance->create())
		qFatal("Failed to create Vulkan instance: %d", vulkanInstance->errorCode());

    mainWindow = new MainWindow(vulkanInstance);
	mainWindow->show();

//	vulkanWindow = new VulkanWindow();
//	vulkanWindow->setVulkanInstance(vulkanInstance);
//	vulkanWindow->show();
}

Application::~Application()
{
	delete mainWindow;

//	vulkanWindow->destroy();

    //delete vulkanWindow;

	vulkanInstance->destroy();

	delete vulkanInstance;
}
