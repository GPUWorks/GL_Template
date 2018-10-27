#include "VKUtilities.hpp"

#ifdef VULKAN_BACKEND
/// Data.

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};


VkDebugReportCallbackEXT VKUtilities::callback;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags,
													VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code,
													const char* layerPrefix, const char* msg, void* userData) {
	Log::Error() << "validation layer: " << msg << std::endl;
	return VK_FALSE;
}

// The callback registration methid is itself an extension, so we have to query it by hand and wrap it...
VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if(func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

// Same for destruction.
void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if(func != nullptr) {
		func(instance, callback, pAllocator);
	}
}

bool VKUtilities::checkValidationLayerSupport(){
	// Get available layers.
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
	// Cross check with those we want.
	for(const char* layerName : validationLayers){
		bool layerFound = false;
		for(const auto& layerProperties : availableLayers){
			if(strcmp(layerName, layerProperties.layerName) == 0){
				layerFound = true;
				break;
			}
		}
		if(!layerFound){
			return false;
		}
	}
	return true;
}

std::vector<const char*> VKUtilities::getRequiredInstanceExtensions(const bool enableValidationLayers){
	// Default Vulkan has no notion of surface/window. GLFW provide an implementation of the corresponding KHR extensions.
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	
	// If the validation layers are enabled, add associated extensions.
	if(enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}
	return extensions;
}



int VKUtilities::createInstance(const std::string & name, const bool debugEnabled, VkInstance & instance)
{
	// Create a Vulkan instance.
	// Creation setup.
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = name.c_str();
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "GL_Template";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;
	VkInstanceCreateInfo createInstanceInfo = {};
	createInstanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInstanceInfo.pApplicationInfo = &appInfo;
	
	// We have to tell Vulkan the extensions we need.
	const std::vector<const char*> extensions = VKUtilities::getRequiredInstanceExtensions(debugEnabled);
	// Add them to the instance infos.
	createInstanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInstanceInfo.ppEnabledExtensionNames = extensions.data();
	// Validation layers.
	if(debugEnabled){
		createInstanceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInstanceInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createInstanceInfo.enabledLayerCount = 0;
	}
	
	if(vkCreateInstance(&createInstanceInfo, nullptr, &instance) != VK_SUCCESS){
		Log::Error() << "Unable to create a Vulkan instance." << std::endl;
		return 3;
	}
	
	/// Debug callback creation.
	if(debugEnabled){
		VkDebugReportCallbackCreateInfoEXT createCallbackInfo = {};
		createCallbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		createCallbackInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		createCallbackInfo.pfnCallback = &debugCallback;
		if(CreateDebugReportCallbackEXT(instance, &createCallbackInfo, nullptr, &callback) != VK_SUCCESS) {
			Log::Error() << "Unable to register the debug callback." << std::endl;
			return 3;
		}
	}
	return 0;
}

void VKUtilities::cleanupDebug(VkInstance & instance )
{
	DestroyDebugReportCallbackEXT(instance, callback, nullptr);
}

VKUtilities::ActiveQueues VKUtilities::getGraphicsQueueFamilyIndex(VkPhysicalDevice device, VkSurfaceKHR surface){
	ActiveQueues queues;
	// Get all queues.
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
	// Find a queue with graphics support.
	int i = 0;
	for(const auto& queueFamily : queueFamilies){
		// Check if queue support graphics.
		if(queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			queues.graphicsQueue = i;
		}
		// CHeck if queue support presentation.
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if(queueFamily.queueCount > 0 && presentSupport) {
			queues.presentQueue = i;
		}
		if(queues.isComplete()){
			break;
		}
		
		++i;
	}
	return queues;
}


int VKUtilities::createPhysicalDevice(VkInstance & instance, VkSurfaceKHR & surface, VkPhysicalDevice & physicalDevice, VkDeviceSize & minUniformOffset)
{
	physicalDevice = VK_NULL_HANDLE;
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if(deviceCount == 0){
		Log::Error() << "No Vulkan GPU available." << std::endl;
		return 3;
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	// Check which one is ok for our requirements.
	for(const auto& device : devices) {
		if(VKUtilities::isDeviceSuitable(device, surface)) {
			physicalDevice = device;
			break;
		}
	}
	
	if(physicalDevice == VK_NULL_HANDLE) {
		Log::Error() << "No GPU satisifies the requirements." << std::endl;
		return 3;
	}
	
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	minUniformOffset = properties.limits.minUniformBufferOffsetAlignment;
	
	return 0;
}


int VKUtilities::createDevice(VkPhysicalDevice & physicalDevice, std::set<int> & queuesIds, VkPhysicalDeviceFeatures & features, VkDevice & device, bool debugLayersEnabled){
	float queuePriority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	for(int queueFamily : queuesIds) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}
	// Device setup.
	VkDeviceCreateInfo createDeviceInfo = {};
	createDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createDeviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createDeviceInfo.pQueueCreateInfos = queueCreateInfos.data();
	createDeviceInfo.pEnabledFeatures = &features;
	// Extensions.
	createDeviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createDeviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
	// Debug layers.
	if(debugLayersEnabled) {
		createDeviceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createDeviceInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createDeviceInfo.enabledLayerCount = 0;
	}
	if(vkCreateDevice(physicalDevice, &createDeviceInfo, nullptr, &device) != VK_SUCCESS) {
		Log::Error() << "Unable to create logical Vulkan device." << std::endl;
		return 3;
	}
	return 0;
}

VKUtilities::SwapchainParameters VKUtilities::generateSwapchainParameters(VkPhysicalDevice & physicalDevice, VkSurfaceKHR & surface, const int width, const int height){
	SwapchainParameters parameters;
	parameters.support = VKUtilities::querySwapchainSupport(physicalDevice, surface);
	parameters.extent = VKUtilities::chooseSwapExtent(parameters.support.capabilities, width, height);
	parameters.surface = VKUtilities::chooseSwapSurfaceFormat(parameters.support.formats);
	parameters.mode = VKUtilities::chooseSwapPresentMode(parameters.support.presentModes);
	// Set the number of images in the chain.
	Log::Info() << "Swapchain can have between " << parameters.support.capabilities.minImageCount << " and " << parameters.support.capabilities.maxImageCount << " images." << std::endl;
	parameters.count = parameters.support.capabilities.minImageCount + 1;
	return parameters;
}

VkExtent2D VKUtilities::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const int width, const int height) {
	if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {
		
		VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
		
		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
		
		return actualExtent;
	}
}

VkSurfaceFormatKHR VKUtilities::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	// If undefined, the surface doesn't care, we pick what we want.
	// ie RGBA8 with a sRGB display.
	if(availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
		return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}
	// Else, is our preferred choice available?
	for(const auto& availableFormat : availableFormats) {
		if(availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}
	// Else just take what is given.
	return availableFormats[0];
}

VkPresentModeKHR VKUtilities::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
	// By default only FIFO (~V-sync mode) is available.
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
	
	for(const auto& availablePresentMode : availablePresentModes) {
		if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			// If available, we directly pick triple buffering.
			Log::Info() << "Mailbox mode." << std::endl;
			return availablePresentMode;
		} else if(availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			// Uncapped framerate.
			//bestMode = availablePresentMode;
		}
	}
	Log::Info() << "Swapchain using " << (bestMode == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "FIFO") << " mode." << std::endl;
	return bestMode;
}

int VKUtilities::createSwapchain(SwapchainParameters & parameters, VkSurfaceKHR & surface, VkDevice & device, ActiveQueues & queues,VkSwapchainKHR & swapchain, VkSwapchainKHR oldSwapchain){
	
	// maxImageCount = 0 if there is no constraint.
	if(parameters.support.capabilities.maxImageCount > 0 && parameters.count > parameters.support.capabilities.maxImageCount) {
		parameters.count = parameters.support.capabilities.maxImageCount;
	}
	/// Swap chain setup.
	VkSwapchainCreateInfoKHR createSwapInfo = {};
	createSwapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createSwapInfo.surface = surface;
	createSwapInfo.minImageCount = parameters.count;
	createSwapInfo.imageFormat = parameters.surface.format;
	createSwapInfo.imageColorSpace = parameters.surface.colorSpace;
	createSwapInfo.imageExtent = parameters.extent;
	createSwapInfo.imageArrayLayers = 1;
	createSwapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	//VK_IMAGE_USAGE_TRANSFER_DST_BIT if not rendering directly to it.
	// Establish a link with both queues, handling the case where they are the same.
	uint32_t queueFamilyIndices[] = { (uint32_t)(queues.graphicsQueue), (uint32_t)(queues.presentQueue) };
	if(queues.graphicsQueue != queues.presentQueue){
		createSwapInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createSwapInfo.queueFamilyIndexCount = 2;
		createSwapInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createSwapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createSwapInfo.queueFamilyIndexCount = 0; // Optional
		createSwapInfo.pQueueFamilyIndices = nullptr; // Optional
	}
	createSwapInfo.preTransform = parameters.support.capabilities.currentTransform;
	createSwapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createSwapInfo.presentMode = parameters.mode;
	createSwapInfo.clipped = VK_TRUE;
	createSwapInfo.oldSwapchain = oldSwapchain;
	
	if(vkCreateSwapchainKHR(device, &createSwapInfo, nullptr, &swapchain) != VK_SUCCESS) {
		Log::Error() << "Unable to create swap chain." << std::endl;
		return 3;
	}
	return 0;
}

bool VKUtilities::checkDeviceExtensionSupport(VkPhysicalDevice device) {
	// Get available device extensions.
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	// Check if the required device extensions are available.
	for(const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}
	return requiredExtensions.empty();
}

bool VKUtilities::isDeviceSuitable(VkPhysicalDevice adevice, VkSurfaceKHR asurface){
	bool extensionsSupported = checkDeviceExtensionSupport(adevice);
	bool isComplete = VKUtilities::getGraphicsQueueFamilyIndex(adevice, asurface).isComplete();
	bool swapChainAdequate = false;
	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(adevice, &supportedFeatures);
	if(extensionsSupported) {
		SwapchainSupportDetails swapChainSupport = VKUtilities::querySwapchainSupport(adevice, asurface);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}
	return extensionsSupported && isComplete && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}


/// Swap chain support.

VKUtilities::SwapchainSupportDetails VKUtilities::querySwapchainSupport(VkPhysicalDevice adevice, VkSurfaceKHR asurface) {
	SwapchainSupportDetails details;
	// Basic capabilities.
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(adevice, asurface, &details.capabilities);
	// Supported formats.
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(adevice, asurface, &formatCount, nullptr);
	if(formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(adevice, asurface, &formatCount, details.formats.data());
	}
	// Supported modes.
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(adevice, asurface, &presentModeCount, nullptr);
	if(presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(adevice, asurface, &presentModeCount, details.presentModes.data());
	}
	return details;
}

VkFormat VKUtilities::findSupportedFormat(const VkPhysicalDevice & physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features){
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}
	
	return VK_FORMAT_UNDEFINED;
}

VkFormat VKUtilities::findDepthFormat(const VkPhysicalDevice & physicalDevice) {
	return findSupportedFormat(physicalDevice,  {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

uint32_t VKUtilities::findMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags & properties, const VkPhysicalDevice & physicalDevice) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	Log::Error() << "Unable to find proper memory." << std::endl;
	return 0;
}

int VKUtilities::createImage(const VkPhysicalDevice & physicalDevice, const VkDevice & device, const uint32_t & width, const uint32_t & height, const uint32_t & mipCount, const VkFormat & format, const VkImageTiling & tiling, const VkImageUsageFlags & usage, const VkMemoryPropertyFlags & properties, const bool cube, VkImage & image, VkDeviceMemory & imageMemory){
	// Create image.
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>(width);
	imageInfo.extent.height = static_cast<uint32_t>(height);
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipCount;
	imageInfo.arrayLayers = cube ? 6 : 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		Log::Error() << "Unable to create texture image." << std::endl;
		return 3;
	}
	// Allocate memory for image.
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties, physicalDevice);
	if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		Log::Error() << "Unable to allocate texture memory." << std::endl;
		return 3;
	}
	vkBindImageMemory(device, image, imageMemory, 0);
	return 0;
}

VkImageView VKUtilities::createImageView(const VkDevice & device, const VkImage & image, const VkFormat format, const VkImageAspectFlags aspectFlags, const bool cube, const uint32_t & mipCount) {
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = cube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipCount;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = cube ? 6 : 1;
	
	VkImageView imageView;
	if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		Log::Error() << "Unable to create image view." << std::endl;
	}
	return imageView;
}

void VKUtilities::transitionImageLayout(const VkDevice & device, const VkCommandPool & commandPool, const VkQueue & queue, VkImage & image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, const bool cube, const uint32_t & mipCount) {
	VkCommandBuffer commandBuffer = beginOneShotCommandBuffer(device, commandPool);
	
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;
	
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // We don't change queue here.
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipCount;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = cube ? 6 : 1;
	// Aspect mask.
	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (hasStencilComponent(format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	} else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}
	
	// Masks for triggers.
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0; //  As soon as possible.
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // Before transfer.
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // After a transfer.
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // Before the shader.
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0; //  As soon as possible.
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // Before using it.
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	} else {
		Log::Error() << "Unsupported layout transition." << std::endl;
	}
	
	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr,  0, nullptr,  1, &barrier );
	
	endOneShotCommandBuffer(commandBuffer, device, commandPool, queue);
}

bool VKUtilities::hasStencilComponent(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkCommandBuffer VKUtilities::beginOneShotCommandBuffer( const  VkDevice & device,  const  VkCommandPool & commandPool){
	// Create short-lived command buffer.
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
	// Record in it immediatly.
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	return commandBuffer;
}

void VKUtilities::endOneShotCommandBuffer(VkCommandBuffer & commandBuffer, const VkDevice & device,  const VkCommandPool & commandPool,  const  VkQueue & queue){
	vkEndCommandBuffer(commandBuffer);
	// Submit it.
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

#endif
