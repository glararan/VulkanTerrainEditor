#include "Shader.h"

#include "Vulkan/Initializers.h"
#include "Vulkan/Manager.h"
#include "Vulkan/Tools.h"

#include <QtConcurrentRun>
#include <QFile>
#include <QVulkanDeviceFunctions>

ShaderData* Shader::data()
{
    if(running && !shaderData.isValid())
        shaderData = future.result();

    return &shaderData;
}

void Shader::load(const QString& fileName)
{
    reset();

    running = true;

    future = QtConcurrent::run([fileName]()
    {
        ShaderData data;

        QFile file(fileName);

        if(!file.open(QIODevice::ReadOnly))
        {
            qWarning("Failed to open %s", qPrintable(fileName));

            return data;
        }

        QByteArray blob = file.readAll();

        VkShaderModuleCreateInfo shaderInfo;

        memset(&shaderInfo, 0, sizeof(shaderInfo));

        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = blob.size();
        shaderInfo.pCode = reinterpret_cast<const uint32_t *>(blob.constData());

        VK_CHECK_RESULT(VulkanManager->deviceFuncs->vkCreateShaderModule(VulkanManager->device, &shaderInfo, nullptr, &data.shaderModule));

        return data;
    });
}

void Shader::reset()
{
    *data() = ShaderData();

    running = false;
}
