#include "Tools.h"

#include <QVector>
#include <QFile>
#include <QTextStream>

#include "Initializers.h"

namespace Vulkan
{
	namespace Tools
	{
		bool errorModeSilent = false;

		QString errorString(VkResult errorCode)
		{
			switch (errorCode)
			{
#define STR(r) case VK_ ##r: return #r
				STR(NOT_READY);
				STR(TIMEOUT);
				STR(EVENT_SET);
				STR(EVENT_RESET);
				STR(INCOMPLETE);
				STR(ERROR_OUT_OF_HOST_MEMORY);
				STR(ERROR_OUT_OF_DEVICE_MEMORY);
				STR(ERROR_INITIALIZATION_FAILED);
				STR(ERROR_DEVICE_LOST);
				STR(ERROR_MEMORY_MAP_FAILED);
				STR(ERROR_LAYER_NOT_PRESENT);
				STR(ERROR_EXTENSION_NOT_PRESENT);
				STR(ERROR_FEATURE_NOT_PRESENT);
				STR(ERROR_INCOMPATIBLE_DRIVER);
				STR(ERROR_TOO_MANY_OBJECTS);
				STR(ERROR_FORMAT_NOT_SUPPORTED);
				STR(ERROR_SURFACE_LOST_KHR);
				STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
				STR(SUBOPTIMAL_KHR);
				STR(ERROR_OUT_OF_DATE_KHR);
				STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
				STR(ERROR_VALIDATION_FAILED_EXT);
				STR(ERROR_INVALID_SHADER_NV);
#undef STR
			default:
				return "UNKNOWN_ERROR";
			}
		}

		QString physicalDeviceTypeString(VkPhysicalDeviceType type)
		{
			switch (type)
			{
#define STR(r) case VK_PHYSICAL_DEVICE_TYPE_ ##r: return #r
				STR(OTHER);
				STR(INTEGRATED_GPU);
				STR(DISCRETE_GPU);
				STR(VIRTUAL_GPU);
#undef STR
                default:
                    return "UNKNOWN_DEVICE_TYPE";
			}
		}
				
		VkShaderModule loadShader(const QString& fileName, VkDevice device, QVulkanDeviceFunctions* functions)
		{
			QFile file(fileName);

			if (!file.open(QIODevice::ReadOnly))
			{
				qWarning("Failed to read shader %s", qPrintable(fileName));

				return VK_NULL_HANDLE;
			}

			QByteArray blob = file.readAll();

			file.close();

			VkShaderModuleCreateInfo shaderInfo;

			memset(&shaderInfo, 0, sizeof(shaderInfo));

			shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			shaderInfo.codeSize = blob.size();
			shaderInfo.pCode = reinterpret_cast<const uint32_t*>(blob.constData());

			VkShaderModule shaderModule;

			VkResult result = functions->vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule);

			if (result != VK_SUCCESS)
			{
				qWarning("Failed to create shader module: %d", result);

				return VK_NULL_HANDLE;
			}

			return shaderModule;
		}

		VkShaderModule loadShaderGLSL(const QString& fileName, VkDevice device, QVulkanDeviceFunctions* functions, VkShaderStageFlagBits stage)
		{
			QFile file(fileName);

			QTextStream in(&file);

			QString shaderSrc = in.readAll();

			Q_ASSERT(shaderSrc.length() > 0);

			VkShaderModule shaderModule;
			VkShaderModuleCreateInfo moduleCreateInfo;
			moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			moduleCreateInfo.pNext = NULL;
			moduleCreateInfo.codeSize = 3 * sizeof(quint32) + shaderSrc.length() + 1;
			moduleCreateInfo.pCode = (quint32*)malloc(moduleCreateInfo.codeSize);
			moduleCreateInfo.flags = 0;

			// Magic SPV number
			((quint32*)moduleCreateInfo.pCode)[0] = 0x07230203;
			((quint32*)moduleCreateInfo.pCode)[1] = 0;
			((quint32*)moduleCreateInfo.pCode)[2] = stage;

			memcpy(((quint32*)moduleCreateInfo.pCode + 3), shaderSrc.toLatin1().data(), shaderSrc.length() + 1);

			VK_CHECK_RESULT(functions->vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule));

			return shaderModule;
		}
	}
}
