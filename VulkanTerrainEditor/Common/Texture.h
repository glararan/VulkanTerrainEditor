#ifndef TEXTURE_H
#define TEXTURE_H

#include <QVulkanFunctions>
#include <QFile>

#include "Vulkan/Manager.h"

class Texture
{
public:
    Texture(Vulkan::Manager& vkManager);
	~Texture(); // Release all Vulkan resources held by this texture

	void updateDescriptor(); // Update image descriptor from current sampler, view and image layout

	Vulkan::Manager vulkanManager;

    VkFormat format;

	VkImage image;
	VkImageLayout imageLayout;
	VkDeviceMemory deviceMemory;
	VkImageView view;

	quint32 width, height;
	quint32 mipLevels;
	quint32 layerCount;

	VkDescriptorImageInfo descriptor;

	VkSampler sampler; // Optional sampler to use with this texture
};

class Texture2D : public Texture
{
public:
	/**
	* Load a 2D texture including all mip levels
	*
	* @param filename File to load (supports .ktx and .dds)
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	* @param (Optional) forceLinear Force linear tiling (not advised, defaults to false)
	*
	*/
    bool loadFromFile(const QString& filename, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, bool forceLinear = false, bool srgb = true);

	/**
	* Creates a 2D texture from a buffer
	*
	* @param buffer Buffer containing texture data to upload
	* @param bufferSize Size of the buffer in machine units
	* @param width Width of the texture to create
	* @param height Height of the texture to create
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) filter Texture filtering for the sampler (defaults to VK_FILTER_LINEAR)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	*/
    bool fromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat vkFormat, const QSize& size, VkQueue copyQueue, VkFilter filter = VK_FILTER_LINEAR, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

private:
    bool createTextureImage(VkImageTiling tiling, VkImageUsageFlags usage, uint32_t memoryIndex);

    bool writeLinearImage(const QImage& img);
};

/*class Texture2DArray : public Texture
{
public:
    /
	* Load a 2D texture array including all mip levels
	*
	* @param filename File to load (supports .ktx and .dds)
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	*

    void loadFromFile(const QString& filename, VkFormat format, Vulkan::VulkanDevice* device, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};

class TextureCubeMap : public Texture
{
public:

	* Load a cubemap texture including all mip levels from a single file
	*
	* @param filename File to load (supports .ktx and .dds)
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	*

    void loadFromFile(const QString& filename, VkFormat format, Vulkan::VulkanDevice* device, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};*/

#endif // TEXTURE_H
