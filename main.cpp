#include <iostream>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <fstream>
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
    uint32_t _queueIndex = ~0;

    vk::Queue _graphicsQueue {};

    vk::Extent2D _swapChainExtent {};

    vk::SurfaceFormatKHR _surfaceFormat {};

    vk::SwapchainKHR _swapChain {};
    std::vector<vk::Image> _swapChainImages {};
    std::vector<vk::ImageView> _swapChainImageViews {};

    vk::ShaderModule _shaderModule;
    vk::Pipeline _pipeline;
    vk::PipelineLayout _pipelineLayout {};

    vk::CommandPool _commandPool {};
    vk::CommandBuffer _commandBuffer;

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
        CreateGraphicsPipeline();
        CreateCommandPool();
        CreateCommandBuffer();
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

        _device.destroyPipelineLayout(_pipelineLayout);
        _device.destroyShaderModule(_shaderModule);
        _device.destroyPipeline(_pipeline);

        _device.destroySwapchainKHR(_swapChain);

        for (auto& imageView : _swapChainImageViews)
            _device.destroyImageView(imageView);

        vkDestroySurfaceKHR(_instance, _surface, nullptr);

        _device.freeCommandBuffers(_commandPool, _commandBuffer);
        _device.destroyCommandPool(_commandPool);

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

        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); ++qfpIndex)
        {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) && _physicalDevice.getSurfaceSupportKHR(qfpIndex, _surface))
            {
                _queueIndex = qfpIndex;
                break;
            }
        }

        if (_queueIndex == ~0)
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");

        float queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo queueCreateInfo {};
        queueCreateInfo.queueFamilyIndex = _queueIndex;
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
        _graphicsQueue = _device.getQueue(_queueIndex, 0);
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

    void CreateGraphicsPipeline()
    {
        _shaderModule = CreateShaderModule(ReadFile("slang.spv"));

        vk::PipelineShaderStageCreateInfo vertShaderInfo {};
        vertShaderInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderInfo.module = _shaderModule;
        vertShaderInfo.pName = "vertMain";

        vk::PipelineShaderStageCreateInfo fragShaderInfo {};
        fragShaderInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderInfo.module = _shaderModule;
        fragShaderInfo.pName = "fragMain";

        vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderInfo, fragShaderInfo };

        std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo {};
        dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
        vk::PipelineViewportStateCreateInfo viewportStateCreateInfo {};
        viewportStateCreateInfo.viewportCount = 1;
        viewportStateCreateInfo.scissorCount = 1;

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo {};

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly {};
        inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

        vk::PipelineRasterizationStateCreateInfo rasterizerCreateInfo {};
        rasterizerCreateInfo.depthClampEnable = vk::False;
        rasterizerCreateInfo.rasterizerDiscardEnable = vk::False;
        rasterizerCreateInfo.polygonMode = vk::PolygonMode::eFill;
        rasterizerCreateInfo.cullMode = vk::CullModeFlagBits::eBack;
        rasterizerCreateInfo.frontFace = vk::FrontFace::eClockwise;
        rasterizerCreateInfo.depthBiasEnable = vk::False;
        rasterizerCreateInfo.lineWidth = 1.0f;

        vk::PipelineMultisampleStateCreateInfo multisampling {};
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
        multisampling.sampleShadingEnable = vk::False;

        vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable    = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

        vk::PipelineColorBlendStateCreateInfo colorBlending{
            .logicOpEnable = vk::False, .logicOp = vk::LogicOp::eCopy, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};

        vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
        pipelineLayoutCreateInfo.setLayoutCount = 0;
        pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
        _pipelineLayout = _device.createPipelineLayout(pipelineLayoutCreateInfo);

        vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo {};
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &_surfaceFormat.format;

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> createInfoChain = {
            {
                  .stageCount = 2,
                  .pStages = shaderStages,
                  .pVertexInputState = &vertexInputInfo,
                  .pInputAssemblyState = &inputAssembly,
                  .pViewportState = &viewportStateCreateInfo,
                  .pRasterizationState = &rasterizerCreateInfo,
                  .pMultisampleState = &multisampling,
                  .pColorBlendState = &colorBlending,
                  .pDynamicState = &dynamicStateCreateInfo,
                  .layout = _pipelineLayout,
                  .renderPass = nullptr
                },
        { .colorAttachmentCount = 1, .pColorAttachmentFormats = &_surfaceFormat.format }
        };

        _pipeline = _device.createGraphicsPipeline(nullptr, createInfoChain.get<vk::GraphicsPipelineCreateInfo>()).value;
    }

    void CreateCommandPool()
    {
        vk::CommandPoolCreateInfo createInfo {};
        createInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        createInfo.queueFamilyIndex = _queueIndex;

        _commandPool = _device.createCommandPool(createInfo);
    }

    void CreateCommandBuffer()
    {
        vk::CommandBufferAllocateInfo allocInfo {};
        allocInfo.commandPool = _commandPool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = 1;

        _commandBuffer = _device.allocateCommandBuffers(allocInfo).front();
    }

    void RecordCommandBuffer(uint32_t imageIndex)
    {
        TransitImageLayout(
            imageIndex,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
    {},
    vk::AccessFlagBits2::eColorAttachmentWrite,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput
        );

        vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        vk::RenderingAttachmentInfo attachmentInfo {};
        attachmentInfo.imageView = _swapChainImageViews[imageIndex];
        attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
        attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
        attachmentInfo.clearValue = clearColor;

        vk::RenderingInfo renderingInfo {};
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = _swapChainExtent;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &attachmentInfo;

        _commandBuffer.beginRendering(renderingInfo);
        _commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);

        _commandBuffer.setViewport(0, vk::Viewport(0.0, 0.0, static_cast<float>(_swapChainExtent.width), static_cast<float>(_swapChainExtent.height), 0.0f, 1.0f));
        _commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), _swapChainExtent));

        _commandBuffer.draw(3, 1, 0, 0);

        _commandBuffer.endRendering();

        TransitImageLayout(
            imageIndex,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            {},
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eBottomOfPipe
        );

        _commandBuffer.end();
    }

    void TransitImageLayout(uint32_t imageIndex,
        vk::ImageLayout         oldLayout,
        vk::ImageLayout         newLayout,
        vk::AccessFlags2        srcAccessMask,
        vk::AccessFlags2        dstAccessMask,
        vk::PipelineStageFlags2 srcStageMask,
        vk::PipelineStageFlags2 dstStageMask)
    {
        vk::ImageMemoryBarrier2 barrier =
        {
            .srcStageMask        = srcStageMask,
            .srcAccessMask       = srcAccessMask,
            .dstStageMask        = dstStageMask,
            .dstAccessMask       = dstAccessMask,
            .oldLayout           = oldLayout,
            .newLayout           = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = _swapChainImages[imageIndex],
            .subresourceRange    =
         {
                .aspectMask      = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel    = 0,
                .levelCount      = 1,
                .baseArrayLayer  = 0,
                .layerCount      = 1
            }
        };
        vk::DependencyInfo dependencyInfo = {
            .dependencyFlags         = {},
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &barrier};

        _commandBuffer.pipelineBarrier2(dependencyInfo);
    }

    vk::ShaderModule CreateShaderModule(const std::vector<char>& code) const
    {
        vk::ShaderModuleCreateInfo createInfo {};
        createInfo.codeSize = code.size() * sizeof(char);
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        return _device.createShaderModule(createInfo);
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

    static std::vector<char> ReadFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
            throw std::runtime_error("failed to open a file");

        std::vector<char> buffer(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

        file.close();

        return buffer;
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