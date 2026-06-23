#include <iostream>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include <GLFW/glfw3.h>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef __MACH__
    std::vector<const char*> requiredDeviceExtension = { vk::KHRSwapchainExtensionName, "VK_KHR_portability_subset" };
#else
    std::vector<const char*> requiredDeviceExtension = { vk::KHRSwapchainExtensionName };
#endif


#ifdef NDEBUG
    constexpr bool enableValidationLayers = false;
#else
    constexpr bool enableValidationLayers = true;
#endif

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                      vk::DebugUtilsMessageTypeFlagsEXT type,
                                                      const vk::DebugUtilsMessengerCallbackDataEXT * pCallbackData,
                                                      void* pUserData)
{
    std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
}

class HelloTriangleApplication
{

public:

    void Run()
    {
        InitWindow();
        InitVulkan();
        MainLoop();
        Cleanup();
    }

private:

    GLFWwindow* _window {};

    vk::Instance _instance {};

    vk::DebugUtilsMessengerEXT _debugMessenger {};

    vk::SurfaceKHR _surface {};

    vk::PhysicalDevice _physicalDevice {};

    vk::Device _device {};

    vk::Queue _graphicsQueue {};

    vk::Extent2D _swapChainExtent {};

    vk::SurfaceFormatKHR _surfaceFormat {};

    vk::SwapchainKHR _swapChain {};
    std::vector<vk::Image> _swapChainImages {};
    std::vector<vk::ImageView> _swapChainImageViews {};

private:

    void InitWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        _window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void InitVulkan()
    {
        CreateInstance();
        SetupDebugMessenger();
        CreateSurface();
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapChain();
        CreateImageViews();
    }

    void MainLoop()
    {
        while (!glfwWindowShouldClose(_window))
        {
            glfwPollEvents();
        }
    }

    void Cleanup()
    {
        if (enableValidationLayers)
            _instance.destroyDebugUtilsMessengerEXT(_debugMessenger);

        _device.destroySwapchainKHR(_swapChain);

        for (auto& imageView : _swapChainImageViews)
            _device.destroyImageView(imageView);

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        _device.destroy();
        _instance.destroy();

        glfwDestroyWindow(_window);
        glfwTerminate();
    }

private:

    void CreateInstance()
    {
        vk::detail::DynamicLoader loader;

        auto vkGetInstanceProcAddr = loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");

        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        vk::ApplicationInfo appInfo {};

        appInfo.pApplicationName = "Triangle Application";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = vk::ApiVersion13;

        auto requiredExtensions = GetRequiredInstanceExtensions();

        const std::vector<vk::ExtensionProperties> extensionProperties = vk::enumerateInstanceExtensionProperties();
        auto unsupportedPropertyIt = std::ranges::find_if(requiredExtensions, [&extensionProperties](const auto & requiredExtension)
        {
            return std::ranges::none_of(extensionProperties, [requiredExtension](const auto& extensionProperty)
            {
                return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
            });
        });

        if (unsupportedPropertyIt != requiredExtensions.end())
            throw std::runtime_error("Unsupported required extension " + std::string(*unsupportedPropertyIt));

        std::vector<const char*> requiredLayers;
        if (enableValidationLayers)
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());

        auto layerProperties = vk::enumerateInstanceLayerProperties();

        auto unsupportedLayerIt = std::ranges::find_if(requiredLayers, [&layerProperties](auto const &requiredLayer)
        {
            return std::ranges::none_of(layerProperties,
            [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
        });

        if (unsupportedLayerIt != requiredLayers.end())
        {
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
        }

        vk::InstanceCreateInfo createInfo;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = requiredExtensions.size();
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        createInfo.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size());
        createInfo.ppEnabledLayerNames = requiredLayers.data();

#ifdef __MACH__
        createInfo.flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

        _instance = vk::createInstance(createInfo);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);
    }

    void SetupDebugMessenger()
    {
        if (!enableValidationLayers)
            return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
                vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT {};
        debugUtilsMessengerCreateInfoEXT.messageSeverity = severityFlags;
        debugUtilsMessengerCreateInfoEXT.messageType = messageTypeFlags;
        debugUtilsMessengerCreateInfoEXT.pfnUserCallback = &debugCallback;

        _debugMessenger = _instance.createDebugUtilsMessengerEXT( debugUtilsMessengerCreateInfoEXT );
    }

    void CreateSurface()
    {
        VkSurfaceKHR surface;
        if (glfwCreateWindowSurface(_instance, _window, nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("failed to create window surface");

        _surface = surface;
    }

    bool IsDeviceSuitable(const vk::PhysicalDevice& physicalDevice) const
    {
        bool supportsVulkan13 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

        auto queueFamilies = physicalDevice.getQueueFamilyProperties();
        bool supportsGraphics = std::ranges::any_of(queueFamilies, [](const auto& pfp)
        {
            return static_cast<bool>(pfp.queueFlags & vk::QueueFlagBits::eGraphics);
        });

        auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions =
            std::ranges::all_of(requiredDeviceExtension, [&availableDeviceExtensions](const auto& requiredDeviceExtension)
            {
                return std::ranges::any_of(availableDeviceExtensions, [requiredDeviceExtension](const auto& availableDeviceExtension)
                {
                    return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
                });
            });

        auto features = physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2,
                                                                             vk::PhysicalDeviceVulkan11Features,
                                                                             vk::PhysicalDeviceVulkan13Features,
                                                                             vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

        bool supportsRequiredFeatures = features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
                                        features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                        features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        return supportsVulkan13 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
    }

    void PickPhysicalDevice()
    {
        auto devices = _instance.enumeratePhysicalDevices();

        auto const deviceIterator = std::ranges::find_if(devices, [&](const auto& physicalDevice)
        {
            return IsDeviceSuitable(physicalDevice);
        });

        if (deviceIterator == devices.end())
            throw std::runtime_error("Physical device not found");

        _physicalDevice = *deviceIterator;
    }

    void CreateLogicalDevice()
    {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = _physicalDevice.getQueueFamilyProperties();
        uint32_t queueIndex = ~0;
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); ++qfpIndex)
        {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) && _physicalDevice.getSurfaceSupportKHR(qfpIndex, _surface))
            {
                queueIndex = qfpIndex;
                break;
            }
        }

        if (queueIndex == ~0)
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");

        float queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo queueCreateInfo {};
        queueCreateInfo.queueFamilyIndex = queueIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        vk::StructureChain<vk::PhysicalDeviceFeatures2,
                           vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        featureChain = {
            {},
            {.shaderDrawParameters = true },
            {.dynamicRendering = true },
            {.extendedDynamicState = true }
        };

        vk::DeviceCreateInfo createInfo {};
        createInfo.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>();
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size());
        createInfo.ppEnabledExtensionNames = requiredDeviceExtension.data();

        _device = _physicalDevice.createDevice(createInfo);
        _graphicsQueue = _device.getQueue(queueIndex, 0);
    }

    void CreateSwapChain()
    {
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_surface);
        _swapChainExtent = ChooseSwapExtent(surfaceCapabilities);
        uint32_t minImageCount = ChooseSwapMinImageCount(surfaceCapabilities);
        std::vector<vk::SurfaceFormatKHR> availableFormats = _physicalDevice.getSurfaceFormatsKHR(_surface);
        _surfaceFormat = ChooseSwapSurfaceFormat(availableFormats);
        std::vector<vk::PresentModeKHR> availablePresentModes = _physicalDevice.getSurfacePresentModesKHR(_surface);

        vk::SwapchainCreateInfoKHR createInfo {};
        createInfo.surface = _surface;
        createInfo.minImageCount = minImageCount;
        createInfo.imageFormat = _surfaceFormat.format;
        createInfo.imageColorSpace = _surfaceFormat.colorSpace;
        createInfo.imageExtent = _swapChainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.preTransform = surfaceCapabilities.currentTransform;
        createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        createInfo.presentMode = ChoosePresentMode(availablePresentModes);
        createInfo.clipped = true;

        _swapChain = _device.createSwapchainKHR(createInfo, nullptr);
        _swapChainImages = _device.getSwapchainImagesKHR(_swapChain);
    }

    void CreateImageViews()
    {
        vk::ImageViewCreateInfo createInfo {};
        createInfo.viewType = vk::ImageViewType::e2D;
        createInfo.format = _surfaceFormat.format;
        createInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

        for (const auto& image : _swapChainImages)
        {
            createInfo.image = image;
            _swapChainImageViews.emplace_back(_device.createImageView(createInfo));
        }
    }

    vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) const
    {
        const auto formatIt = std::ranges::find_if(availableFormats, [](const auto& format)
        {
            return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });

        return formatIt != availableFormats.end() ? *formatIt : availableFormats.front();
    }

    vk::PresentModeKHR ChoosePresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) const
    {
        return std::ranges::any_of(availablePresentModes, [](const auto& mode)
        {
            return mode == vk::PresentModeKHR::eMailbox;

        }) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            return capabilities.currentExtent;

        int width, height;
        glfwGetFramebufferSize(_window, &width, &height);

        return
        {
            std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }

    uint32_t ChooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        uint32_t minImageCount = std::max(3u, capabilities.minImageCount);
        if ((0 < capabilities.maxImageCount) && (capabilities.maxImageCount < minImageCount))
        {
            minImageCount = capabilities.maxImageCount;
        }
        return minImageCount;
    }

    std::vector<const char*> GetRequiredInstanceExtensions() const
    {
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers)
            extensions.push_back(vk::EXTDebugUtilsExtensionName);

#ifdef __MACH__
        extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
#endif

        return extensions;
    }

};

int main()
{
    try
    {
        HelloTriangleApplication app;
        app.Run();
    }catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}