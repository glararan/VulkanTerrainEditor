#ifndef SHADER_H
#define SHADER_H

#include <QVulkanInstance>
#include <QFuture>

struct ShaderData
{
    bool isValid() const { return shaderModule != VK_NULL_HANDLE; }

    VkShaderModule shaderModule = VK_NULL_HANDLE;
};

class Shader
{
public:
    bool isValid() { return data()->isValid(); }

    void load(const QString& fileName);
    void reset();

    ShaderData* data();

private:
    bool running = false;

    QFuture<ShaderData> future;

    ShaderData shaderData;
};

#endif // SHADER_H
