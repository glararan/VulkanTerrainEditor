#include "Texture.h"

#include "Vulkan/Tools.h"

Texture::Texture(Vulkan::VulkanDevice* vulkanDevice, QVulkanDeviceFunctions* functions) : device(vulkanDevice), deviceFuncs(functions)
{
}

Texture::~Texture()
{
    deviceFuncs->vkDestroyImageView(device->logicalDevice, view, nullptr);
    deviceFuncs->vkDestroyImage(device->logicalDevice, image, nullptr);

	if (sampler)
        deviceFuncs->vkDestroySampler(device->logicalDevice, sampler, nullptr);

    deviceFuncs->vkFreeMemory(device->logicalDevice, deviceMemory, nullptr);
}

void Texture::updateDescriptor()
{
	descriptor.sampler = sampler;
	descriptor.imageView = view;
	descriptor.imageLayout = imageLayout;
}

void Texture2D::loadFromFile(const QString& filename, VkFormat format, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout, bool forceLinear)
{
	if (!QFile::exists(filename))
		qFatal(QString("Could not load texture from %1 \n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.").arg(filename).toLatin1().data());

	gli::texture2d tex2D(gli::load(filename.c_str()));

    Q_ASSERT(!tex2D.empty());

	width = static_cast<quint32>(tex2D[0].extent().x);
	height = static_cast<quint32>(tex2D[0].extent().y);
	mipLevels = static_cast<quint32>(tex2D.levels());

	// Get device properites for the requested texture format
	VkFormatProperties formatProperties;
	deviceFuncs->vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);

	// Only use linear tiling if requested (and supported by the device)
	// Support for linear tiling is mostly limited, so prefer to use optimal tiling instead
	// On most implementations linear tiling will only support a very limited amount of formats and features (mip maps, cubemaps, arrays, etc.)
	VkBool32 useStaging = !forceLinear;

    VkMemoryAllocateInfo memAllocInfo = Vulkan::Initializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	// Use a separate command buffer for texture loading
	VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	if (useStaging)
	{
		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo = Vulkan::Initializers::bufferCreateInfo();
		bufferCreateInfo.size = tex2D.size();
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK_RESULT(deviceFuncs->vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get memory requirements for the staging buffer (alignment, memory type bits)
        deviceFuncs->vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK_RESULT(deviceFuncs->vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
        VK_CHECK_RESULT(deviceFuncs->vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		quint8* data;

        VK_CHECK_RESULT(deviceFuncs->vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));

		memcpy(data, tex2D.data(), tex2D.size());

        deviceFuncs->vkUnmapMemory(device->logicalDevice, stagingMemory);

		// Setup buffer copy regions for each mip level
        QVector<VkBufferImageCopy> bufferCopyRegions;

		quint32 offset = 0;

		for (quint32 i = 0; i < mipLevels; i++)
		{
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = i;
			bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
			bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = static_cast<quint32>(tex2D[i].extent().x);
            bufferCopyRegion.imageExtent.height = static_cast<quint32>(tex2D[i].extent().y);
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;

			bufferCopyRegions.push_back(bufferCopyRegion);

            offset += static_cast<quint32>(tex2D[i].size());
		}

		// Create optimal tiled target image
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
		
        VK_CHECK_RESULT(deviceFuncs->vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

        deviceFuncs->vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK_RESULT(deviceFuncs->vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
        VK_CHECK_RESULT(deviceFuncs->vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipLevels;
		subresourceRange.layerCount = 1;

		// Image barrier for optimal image (target)
		// Optimal image will be used as destination for the copy
        Vulkan::Tools::setImageLayout(deviceFuncs, copyCmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

		// Copy mip levels from staging buffer
		deviceFuncs->vkCmdCopyBufferToImage(copyCmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<quint32>(bufferCopyRegions.size()), bufferCopyRegions.data());

		// Change texture image layout to shader read after all mip levels have been copied
		this->imageLayout = imageLayout;

        Vulkan::Tools::setImageLayout(deviceFuncs, copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout, subresourceRange);

		device->flushCommandBuffer(copyCmd, copyQueue);

		// Clean up staging resources
		deviceFuncs->vkFreeMemory(device, stagingMemory, nullptr);
		deviceFuncs->vkDestroyBuffer(device, stagingBuffer, nullptr);
	}
	else
	{
		// Prefer using optimal tiling, as linear tiling 
		// may support only a small set of features 
		// depending on implementation (e.g. no mip maps, only one layer, etc.)

		// Check if this support is supported for linear tiling
		Q_ASSERT(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

		VkImage mappableImage;
		VkDeviceMemory mappableMemory;

        VkImageCreateInfo imageCreateInfo = Vulkan::Initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.extent = { width, height, 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
		imageCreateInfo.usage = imageUsageFlags;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		// Load mip map level 0 to linear tiling image
		VK_CHECK_RESULT(deviceFuncs->vkCreateImage(device, &imageCreateInfo, nullptr, &mappableImage));

		// Get memory requirements for this image 
		// like size and alignment
		deviceFuncs->vkGetImageMemoryRequirements(device, mappableImage, &memReqs);

		// Set memory allocation size to required memory size
		memAllocInfo.allocationSize = memReqs.size;

		// Get memory type that can be mapped to host memory
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		// Allocate host memory
		VK_CHECK_RESULT(deviceFuncs->vkAllocateMemory(device, &memAllocInfo, nullptr, &mappableMemory));

		// Bind allocated image for use
		VK_CHECK_RESULT(deviceFuncs->vkBindImageMemory(device, mappableImage, mappableMemory, 0));

		// Get sub resource layout
		// Mip map count, array layer, etc.
		VkImageSubresource subRes = {};
		subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subRes.mipLevel = 0;

		VkSubresourceLayout subResLayout;

		void* data;

		// Get sub resources layout 
		// Includes row pitch, size offsets, etc.
		deviceFuncs->vkGetImageSubresourceLayout(device, mappableImage, &subRes, &subResLayout);

		// Map image memory
		VK_CHECK_RESULT(deviceFuncs->vkMapMemory(device, mappableMemory, 0, memReqs.size, 0, &data));

		// Copy image data into memory
		memcpy(data, tex2D[subRes.mipLevel].data(), tex2D[subRes.mipLevel].size());

		deviceFuncs->vkUnmapMemory(device, mappableMemory);

		// Linear tiled images don't need to be staged
		// and can be directly used as textures
		image = mappableImage;
		deviceMemory = mappableMemory;
		this->imageLayout = imageLayout;

		// Setup image memory barrier
        Vulkan::Tools::setImageLayout(deviceFuncs, copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, imageLayout);

		device->flushCommandBuffer(copyCmd, copyQueue);
	}

	// Create a defaultsampler
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	// Max level-of-detail should match mip level count
	samplerCreateInfo.maxLod = (useStaging) ? (float)mipLevels : 0.0f;
	// Only enable anisotropic filtering if enabled on the devicec
	samplerCreateInfo.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f;
	samplerCreateInfo.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	VK_CHECK_RESULT(deviceFuncs->vkCreateSampler(device, &samplerCreateInfo, nullptr, &sampler));

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
	viewCreateInfo.subresourceRange.levelCount = (useStaging) ? mipLevels : 1;
	viewCreateInfo.image = image;

	VK_CHECK_RESULT(deviceFuncs->vkCreateImageView(device, &viewCreateInfo, nullptr, &view));

	// Update descriptor image info member that can be used for setting up descriptor sets
	updateDescriptor();
}

void Texture2D::fromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat format, quint32 width, quint32 height, Vulkan::VulkanDevice* device, VkQueue copyQueue, VkFilter filter, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
    Q_ASSERT(buffer);

    this->device = device;

    width = width;
    height = height;

    mipLevels = 1;

    VkMemoryAllocateInfo memAllocInfo = Vulkan::Initializers::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = Vulkan::Initializers::bufferCreateInfo();
    bufferCreateInfo.size = bufferSize;
    // This buffer is used as a transfer source for the buffer copy
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(deviceFuncs->vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

    // Get memory requirements for the staging buffer (alignment, memory type bits)
    deviceFuncs->vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    // Get memory type index for a host visible buffer
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK_RESULT(deviceFuncs->vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
    VK_CHECK_RESULT(deviceFuncs->vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

    // Copy texture data into staging buffer
    quint8* data;

    VK_CHECK_RESULT(deviceFuncs->vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));

    memcpy(data, buffer, bufferSize);

    deviceFuncs->vkUnmapMemory(device->logicalDevice, stagingMemory);

    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = width;
    bufferCopyRegion.imageExtent.height = height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = 0;

    // Create optimal tiled target image
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

    VK_CHECK_RESULT(deviceFuncs->vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

    deviceFuncs->vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;

    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK_RESULT(deviceFuncs->vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
    VK_CHECK_RESULT(deviceFuncs->vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.layerCount = 1;

    // Image barrier for optimal image (target)
    // Optimal image will be used as destination for the copy
    Vulkan::Tools::setImageLayout(deviceFuncs, copyCmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

    // Copy mip levels from staging buffer
    deviceFuncs->vkCmdCopyBufferToImage(copyCmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

    // Change texture image layout to shader read after all mip levels have been copied
    this->imageLayout = imageLayout;

    Vulkan::Tools::setImageLayout(deviceFuncs, copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout, subresourceRange);

    device->flushCommandBuffer(copyCmd, copyQueue);

    // Clean up staging resources
    deviceFuncs->vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
    deviceFuncs->vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

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

    VK_CHECK_RESULT(deviceFuncs->vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

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

    VK_CHECK_RESULT(deviceFuncs->vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
}

void Texture2DArray::loadFromFile(const QString& filename, VkFormat format, Vulkan::VulkanDevice* device, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
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

    VK_CHECK_RESULT(deviceFuncs->vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

    // Get memory requirements for the staging buffer (alignment, memory type bits)
    deviceFuncs->vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    // Get memory type index for a host visible buffer
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK_RESULT(deviceFuncs->vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
    VK_CHECK_RESULT(deviceFuncs->vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

    // Copy texture data into staging buffer
    quint8* data;

    VK_CHECK_RESULT(deviceFuncs->vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));

    memcpy(data, tex2DArray.data(), static_cast<size_t>(tex2DArray.size()));

    deviceFuncs->vkUnmapMemory(device->logicalDevice, stagingMemory);

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

    VK_CHECK_RESULT(deviceFuncs->vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

    deviceFuncs->vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK_RESULT(deviceFuncs->vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
    VK_CHECK_RESULT(deviceFuncs->vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

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
    deviceFuncs->vkCmdCopyBufferToImage(copyCmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<quint32>(bufferCopyRegions.size()), bufferCopyRegions.data());

    // Change texture image layout to shader read after all faces have been copied
    this->imageLayout = imageLayout;

    Vulkan::Tools::setImageLayout(deviceFuncs, copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout, subresourceRange);

    device->flushCommandBuffer(copyCmd, copyQueue);

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

    VK_CHECK_RESULT(deviceFuncs->vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = Vulkan::Initializers::imageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewCreateInfo.format = format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    viewCreateInfo.subresourceRange.layerCount = layerCount;
    viewCreateInfo.subresourceRange.levelCount = mipLevels;
    viewCreateInfo.image = image;

    VK_CHECK_RESULT(deviceFuncs->vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

    // Clean up staging resources
    deviceFuncs->vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
    deviceFuncs->vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

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

    VK_CHECK_RESULT(deviceFuncs->vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

    // Get memory requirements for the staging buffer (alignment, memory type bits)
    deviceFuncs->vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    // Get memory type index for a host visible buffer
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK_RESULT(deviceFuncs->vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
    VK_CHECK_RESULT(deviceFuncs->vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

    // Copy texture data into staging buffer
    quint8* data;

    VK_CHECK_RESULT(deviceFuncs->vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));

    memcpy(data, texCube.data(), texCube.size());

    deviceFuncs->vkUnmapMemory(device->logicalDevice, stagingMemory);

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

    VK_CHECK_RESULT(deviceFuncs->vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

    deviceFuncs->vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK_RESULT(deviceFuncs->vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
    VK_CHECK_RESULT(deviceFuncs->vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

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
    deviceFuncs->vkCmdCopyBufferToImage(copyCmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<quint32>(bufferCopyRegions.size()), bufferCopyRegions.data());

    // Change texture image layout to shader read after all faces have been copied
    this->imageLayout = imageLayout;

    Vulkan::Tools::setImageLayout(deviceFuncs, copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout, subresourceRange);

    device->flushCommandBuffer(copyCmd, copyQueue);

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

    VK_CHECK_RESULT(deviceFuncs->vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = Vulkan::Initializers::imageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCreateInfo.format = format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    viewCreateInfo.subresourceRange.layerCount = 6;
    viewCreateInfo.subresourceRange.levelCount = mipLevels;
    viewCreateInfo.image = image;

    VK_CHECK_RESULT(deviceFuncs->vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

    // Clean up staging resources
    deviceFuncs->vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
    deviceFuncs->vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
}
