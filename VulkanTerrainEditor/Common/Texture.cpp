#include "Texture.h"

#include "Vulkan/Initializers.h"
#include "Vulkan/Tools.h"

#include <QCoreApplication>
#include <QtGlobal>
#include <QImage>

double qLog2(const double x)
{
    return std::log(x) / std::log(2.0);
}

Texture::Texture(Vulkan::Manager& vkManager) : vulkanManager(vkManager)
{
}

Texture::~Texture()
{
	vulkanManager.deviceFuncs->vkDestroyImageView(vulkanManager.device, view, nullptr);
	vulkanManager.deviceFuncs->vkDestroyImage(vulkanManager.device, image, nullptr);

	if (sampler)
		vulkanManager.deviceFuncs->vkDestroySampler(vulkanManager.device, sampler, nullptr);

	vulkanManager.deviceFuncs->vkFreeMemory(vulkanManager.device, deviceMemory, nullptr);
}

void Texture::updateDescriptor()
{
	descriptor.sampler = sampler;
	descriptor.imageView = view;
	descriptor.imageLayout = imageLayout;
}

bool Texture2D::createTextureImage(VkImageTiling tiling, VkImageUsageFlags usage, uint32_t memoryIndex)
{
    VkImageCreateInfo imageCreateInfo = Vulkan::Initializers::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = tiling;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.usage = usage;

    // Ensure that the TRANSFER_DST bit is set for staging
    if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateImage(vulkanManager.device, &imageCreateInfo, nullptr, &image));

    VkMemoryRequirements memoryRequirements;

    vulkanManager.deviceFuncs->vkGetImageMemoryRequirements(vulkanManager.device, image, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo = Vulkan::Initializers::memoryAllocateInfo();
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = memoryIndex;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkAllocateMemory(vulkanManager.device, &memoryAllocateInfo, nullptr, &deviceMemory));
    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkBindImageMemory(vulkanManager.device, image, deviceMemory, 0));

    return true;
}

bool Texture2D::loadFromFile(const QString& filename, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout, bool forceLinear, bool srgb)
{
	if (!QFile::exists(filename))
		qFatal(QString("Could not load texture from %1 \n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.").arg(filename).toLatin1().data());

	QImage img(filename);

	Q_ASSERT(img.isNull());

	img = img.convertToFormat(QImage::Format_RGBA8888_Premultiplied);

    format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

	VkFormatProperties formatProperties;

	vulkanManager.functions->vkGetPhysicalDeviceFormatProperties(vulkanManager.physicalDevice, format, &formatProperties);

	width = static_cast<quint32>(img.width());
	height = static_cast<quint32>(img.height());
    mipLevels = static_cast<quint32>(1 + std::floor(qLog2(qMax(img.width(), qMax(img.height(), img.depth()))))); // 1

    const bool canSampleLinear = (formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    const bool canSampleOptimal = (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

    if (!canSampleLinear && !canSampleOptimal)
    {
        qWarning("Neither linear nor optimal image sampling is supported for RGBA8");

        return false;
    }

    // Create a defaultsampler
    VkSamplerCreateInfo samplerCreateInfo = {};

    memset(&samplerCreateInfo, 0, sizeof(samplerCreateInfo));

    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    // Max level-of-detail should match mip level count
    // Only enable anisotropic filtering if enabled on the devicec
    samplerCreateInfo.maxAnisotropy = 1.0f;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateSampler(vulkanManager.device, &samplerCreateInfo, nullptr, &sampler));

    if(canSampleLinear && forceLinear)
    {
        if(!createTextureImage(VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_SAMPLED_BIT, vulkanManager.hostVisibleMemoryIndex))
            return false;

        if(!writeLinearImage(img))
            return false;
    }
    else
    {
        if(!createTextureImage(VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, vulkanManager.hostVisibleMemoryIndex))
            return false;

        if(!createTextureImage(VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, vulkanManager.deviceLocalMemoryIndex))
            return false;

        if(!writeLinearImage(img))
            return false;
    }

    // Create image view
    // Textures are not directly accessed by the shaders and are abstracted by image views containing additional information and sub resource ranges
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    // Linear tiling usually won't support mip maps
    // Only set mip map count if optimal tiling is used
    viewCreateInfo.subresourceRange.levelCount = !forceLinear ? mipLevels : 1;
    viewCreateInfo.image = image;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateImageView(vulkanManager.device, &viewCreateInfo, nullptr, &view));

    VkCommandBuffer commandBuffer = vulkanManager.getCommandBuffer();

    VkImageMemoryBarrier imageMemoryBarrier = Vulkan::Initializers::imageMemoryBarrier();
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.levelCount = imageMemoryBarrier.subresourceRange.layerCount = 1;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageMemoryBarrier.srcAccessMask = 0; // VK_ACCESS_HOST_WRITE_BIT ### no, keep validation layer happy (??)
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier.image = image;

    vulkanManager.deviceFuncs->vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();

    return true;
}

bool Texture2D::fromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat vkFormat, const QSize& size, VkQueue copyQueue, VkFilter filter, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
    Q_ASSERT(buffer);

    format = vkFormat;

    width = size.width();
    height = size.height();

    mipLevels = 1;

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = Vulkan::Initializers::bufferCreateInfo();
    bufferCreateInfo.size = bufferSize;
    // This buffer is used as a transfer source for the buffer copy
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateBuffer(vulkanManager.device, &bufferCreateInfo, nullptr, &stagingBuffer));

    VkMemoryRequirements memoryRequirements;

    vulkanManager.deviceFuncs->vkGetImageMemoryRequirements(vulkanManager.device, image, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo = Vulkan::Initializers::memoryAllocateInfo();
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = vulkanManager.hostVisibleMemoryIndex;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkAllocateMemory(vulkanManager.device, &memoryAllocateInfo, nullptr, &stagingMemory));
    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkBindBufferMemory(vulkanManager.device, stagingBuffer, stagingMemory, 0));

    // Copy texture data into staging buffer
    uint8_t* data;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkMapMemory(vulkanManager.device, stagingMemory, 0, memoryRequirements.size, 0, reinterpret_cast<void**>(&data)));

    memcpy(data, buffer, bufferSize);

	vulkanManager.deviceFuncs->vkUnmapMemory(vulkanManager.device, stagingMemory);

    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = width;
    bufferCopyRegion.imageExtent.height = height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = 0;

    VkImageCreateInfo imageCreateInfo = Vulkan::Initializers::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.usage = imageUsageFlags;

    // Ensure that the TRANSFER_DST bit is set for staging
    if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateImage(vulkanManager.device, &imageCreateInfo, nullptr, &image));

    vulkanManager.deviceFuncs->vkGetImageMemoryRequirements(vulkanManager.device, image, &memoryRequirements);

    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = vulkanManager.deviceLocalMemoryIndex;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkAllocateMemory(vulkanManager.device, &memoryAllocateInfo, nullptr, &deviceMemory));
    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkBindImageMemory(vulkanManager.device, image, deviceMemory, 0));

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.layerCount = 1;

    // Use a separate command buffer for texture loading
    VkCommandBuffer commandBuffer = vulkanManager.getCommandBuffer();

    // Image barrier for optimal image (target)
    // Optimal image will be used as destination for the copy
    VkImageMemoryBarrier imageMemoryBarrier = Vulkan::Initializers::imageMemoryBarrier();
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vulkanManager.deviceFuncs->vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

    // Copy mip levels from staging buffer
    vulkanManager.deviceFuncs->vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

    // Change texture image layout to shader read after all mip levels have been copied
    this->imageLayout = imageLayout;

    imageMemoryBarrier = Vulkan::Initializers::imageMemoryBarrier();
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.newLayout = imageLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vulkanManager.deviceFuncs->vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

    // Clean up staging resources
	vulkanManager.deviceFuncs->vkFreeMemory(vulkanManager.device, stagingMemory, nullptr);
	vulkanManager.deviceFuncs->vkDestroyBuffer(vulkanManager.device, stagingBuffer, nullptr);

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = filter;
    samplerCreateInfo.minFilter = filter;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    samplerCreateInfo.maxAnisotropy = 1.0f;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateSampler(vulkanManager.device, &samplerCreateInfo, nullptr, &sampler));

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext = NULL;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.image = image;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateImageView(vulkanManager.device, &viewCreateInfo, nullptr, &view));

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();

    return true;
}

bool Texture2D::writeLinearImage(const QImage& img)
{
    VkImageSubresource subresource = {};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.mipLevel = 0;

    VkSubresourceLayout layout;

    vulkanManager.deviceFuncs->vkGetImageSubresourceLayout(vulkanManager.device, image, &subresource, &layout);

    uchar* p;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkMapMemory(vulkanManager.device, deviceMemory, layout.offset, layout.size, 0, reinterpret_cast<void**>(&p)));

    for (int y = 0; y < img.height(); ++y)
    {
        const uchar *line = img.constScanLine(y);

        memcpy(p, line, img.width() * 4);

        p += layout.rowPitch;
    }

    vulkanManager.deviceFuncs->vkUnmapMemory(vulkanManager.device, deviceMemory);

    return true;
}

/*void Texture2DArray::loadFromFile(const QString& filename, VkFormat format, Vulkan::VulkanDevice* device, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
    if (!QFile::exists(filename))
        qFatal("Could not load texture from " + filename + "\n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);

    gli::texture2d_array tex2DArray(gli::load(filename));

    Q_ASSERT(!tex2DArray.empty());

    this->device = device;

    width = static_cast<quint32>(tex2DArray.extent().x);
    height = static_cast<quint32>(tex2DArray.extent().y);
    layerCount = static_cast<quint32>(tex2DArray.layers());
    mipLevels = static_cast<quint32>(tex2DArray.levels());

    VkMemoryAllocateInfo memAllocInfo = Vulkan::Initializers::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = Vulkan::Initializers::bufferCreateInfo();
    bufferCreateInfo.size = tex2DArray.size();
    // This buffer is used as a transfer source for the buffer copy
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateBuffer(vulkanManager.device, &bufferCreateInfo, nullptr, &stagingBuffer));

    // Get memory requirements for the staging buffer (alignment, memory type bits)
    deviceFuncs->vkGetBufferMemoryRequirements(vulkanManager.device, stagingBuffer, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    // Get memory type index for a host visible buffer
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkAllocateMemory(vulkanManager.device, &memAllocInfo, nullptr, &stagingMemory));
    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkBindBufferMemory(vulkanManager.device, stagingBuffer, stagingMemory, 0));

    // Copy texture data into staging buffer
    quint8* data;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkMapMemory(vulkanManager.device, stagingMemory, 0, memReqs.size, 0, (void **)&data));

    memcpy(data, tex2DArray.data(), static_cast<size_t>(tex2DArray.size()));

	vulkanManager.deviceFuncs->vkUnmapMemory(vulkanManager.device, stagingMemory);

    // Setup buffer copy regions for each layer including all of it's miplevels
    QVector<VkBufferImageCopy> bufferCopyRegions;

    size_t offset = 0;

    for (quint32 layer = 0; layer < layerCount; layer++)
    {
        for (quint32 level = 0; level < mipLevels; level++)
        {
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = level;
            bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = static_cast<quint32>(tex2DArray[layer][level].extent().x);
            bufferCopyRegion.imageExtent.height = static_cast<quint32>(tex2DArray[layer][level].extent().y);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);

            // Increase offset into staging buffer for next level / face
            offset += tex2DArray[layer][level].size();
        }
    }

    // Create optimal tiled target image
    VkImageCreateInfo imageCreateInfo = Vulkan::Initializers::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.usage = imageUsageFlags;

    // Ensure that the TRANSFER_DST bit is set for staging
    if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    imageCreateInfo.arrayLayers = layerCount;
    imageCreateInfo.mipLevels = mipLevels;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateImage(vulkanManager.device, &imageCreateInfo, nullptr, &image));

	vulkanManager.deviceFuncs->vkGetImageMemoryRequirements(vulkanManager.device, image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkAllocateMemory(vulkanManager.device, &memAllocInfo, nullptr, &deviceMemory));
    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkBindImageMemory(vulkanManager.device, image, deviceMemory, 0));

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Image barrier for optimal image (target)
    // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.layerCount = layerCount;

    Vulkan::Tools::setImageLayout(deviceFuncs, copyCmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

    // Copy the layers and mip levels from the staging buffer to the optimal tiled image
	vulkanManager.deviceFuncs->vkCmdCopyBufferToImage(copyCmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<quint32>(bufferCopyRegions.size()), bufferCopyRegions.data());

    // Change texture image layout to shader read after all faces have been copied
    this->imageLayout = imageLayout;

    Vulkan::Tools::setImageLayout(deviceFuncs, copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout, subresourceRange);

	vulkanManager.deviceFuncs->flushCommandBuffer(copyCmd, copyQueue);

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = Vulkan::Initializers::samplerCreateInfo();
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
    samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f;
    samplerCreateInfo.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = (float)mipLevels;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateSampler(vulkanManager.device, &samplerCreateInfo, nullptr, &sampler));

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = Vulkan::Initializers::imageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewCreateInfo.format = format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    viewCreateInfo.subresourceRange.layerCount = layerCount;
    viewCreateInfo.subresourceRange.levelCount = mipLevels;
    viewCreateInfo.image = image;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateImageView(vulkanManager.device, &viewCreateInfo, nullptr, &view));

    // Clean up staging resources
	vulkanManager.deviceFuncs->vkFreeMemory(vulkanManager.device, stagingMemory, nullptr);
	vulkanManager.deviceFuncs->vkDestroyBuffer(vulkanManager.device, stagingBuffer, nullptr);

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
}

void TextureCubeMap::loadFromFile(const QString& filename, VkFormat format, Vulkan::VulkanDevice* device, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
    if (!QFile::exists(filename))
        qFatal("Could not load texture from " + filename + "\n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);

    gli::texture_cube texCube(gli::load(filename));

    Q_ASSERT(!texCube.empty());

    this->device = device;

    width = static_cast<quint32>(texCube.extent().x);
    height = static_cast<quint32>(texCube.extent().y);
    mipLevels = static_cast<quint32>(texCube.levels());

    VkMemoryAllocateInfo memAllocInfo = Vulkan::Initializers::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = Vulkan::Initializers::bufferCreateInfo();
    bufferCreateInfo.size = texCube.size();
    // This buffer is used as a transfer source for the buffer copy
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateBuffer(vulkanManager.device, &bufferCreateInfo, nullptr, &stagingBuffer));

    // Get memory requirements for the staging buffer (alignment, memory type bits)
	vulkanManager.deviceFuncs->vkGetBufferMemoryRequirements(vulkanManager.device, stagingBuffer, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    // Get memory type index for a host visible buffer
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkAllocateMemory(vulkanManager.device, &memAllocInfo, nullptr, &stagingMemory));
    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkBindBufferMemory(vulkanManager.device, stagingBuffer, stagingMemory, 0));

    // Copy texture data into staging buffer
    quint8* data;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkMapMemory(vulkanManager.device, stagingMemory, 0, memReqs.size, 0, (void **)&data));

    memcpy(data, texCube.data(), texCube.size());

	vulkanManager.deviceFuncs->vkUnmapMemory(vulkanManager.device, stagingMemory);

    // Setup buffer copy regions for each face including all of it's miplevels
    QVector<VkBufferImageCopy> bufferCopyRegions;

    size_t offset = 0;

    for (quint32 face = 0; face < 6; face++)
    {
        for (quint32 level = 0; level < mipLevels; level++)
        {
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = level;
            bufferCopyRegion.imageSubresource.baseArrayLayer = face;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = static_cast<quint32>(texCube[face][level].extent().x);
            bufferCopyRegion.imageExtent.height = static_cast<quint32>(texCube[face][level].extent().y);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);

            // Increase offset into staging buffer for next level / face
            offset += texCube[face][level].size();
        }
    }

    // Create optimal tiled target image
    VkImageCreateInfo imageCreateInfo = Vulkan::Initializers::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.usage = imageUsageFlags;

    // Ensure that the TRANSFER_DST bit is set for staging
    if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Cube faces count as array layers in Vulkan
    imageCreateInfo.arrayLayers = 6;
    // This flag is required for cube map images
    imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateImage(vulkanManager.device, &imageCreateInfo, nullptr, &image));

	vulkanManager.deviceFuncs->vkGetImageMemoryRequirements(vulkanManager.device, image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkAllocateMemory(vulkanManager.device, &memAllocInfo, nullptr, &deviceMemory));
    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkBindImageMemory(vulkanManager.device, image, deviceMemory, 0));

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Image barrier for optimal image (target)
    // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.layerCount = 6;

    Vulkan::Tools::setImageLayout(deviceFuncs, copyCmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

    // Copy the cube map faces from the staging buffer to the optimal tiled image
	vulkanManager.deviceFuncs->vkCmdCopyBufferToImage(copyCmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<quint32>(bufferCopyRegions.size()), bufferCopyRegions.data());

    // Change texture image layout to shader read after all faces have been copied
    this->imageLayout = imageLayout;

    Vulkan::Tools::setImageLayout(deviceFuncs, copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout, subresourceRange);

	vulkanManager.deviceFuncs->flushCommandBuffer(copyCmd, copyQueue);

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = Vulkan::Initializers::samplerCreateInfo();
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
    samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f;
    samplerCreateInfo.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = (float)mipLevels;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateSampler(vulkanManager.device, &samplerCreateInfo, nullptr, &sampler));

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = Vulkan::Initializers::imageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCreateInfo.format = format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    viewCreateInfo.subresourceRange.layerCount = 6;
    viewCreateInfo.subresourceRange.levelCount = mipLevels;
    viewCreateInfo.image = image;

    VK_CHECK_RESULT(vulkanManager.deviceFuncs->vkCreateImageView(vulkanManager.device, &viewCreateInfo, nullptr, &view));

    // Clean up staging resources
	vulkanManager.deviceFuncs->vkFreeMemory(vulkanManager.device, stagingMemory, nullptr);
	vulkanManager.deviceFuncs->vkDestroyBuffer(vulkanManager.device, stagingBuffer, nullptr);

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
}*/
