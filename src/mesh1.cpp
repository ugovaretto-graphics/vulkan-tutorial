#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
//#include <volk.h>

#include <meshoptimizer.h>
#include <unistd.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <bitset>
#include <cassert>
#include <climits>
#include <memory>
#include <vector>

#include "common.h"

#define TINYOBJLOADER_IMPLEMENTATION  // define this in only *one* .cc
#include "tiny_obj_loader.h"

using namespace std;

string Cwd() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        return string(cwd);
    } else {
        perror("getcwd() error");
        return "";
    }
}

template <typename ArrayT>
constexpr size_t size(const ArrayT& array) {
    return sizeof(array) / sizeof(array[0]);
}

#define VK_EXT(instance, name) \
    PFN_vk##name vk##name =    \
        (PFN_vk##name)vkGetInstanceProcAddr(instance, "vk" #name);

//==============================================================================
//------------------------------------------------------------------------------
int FindGraphicsQueueFamily(VkPhysicalDevice device) {
    uint32_t indices;
    constexpr size_t MAX_QUEUE_FAMILIES = 8;
    // compute, graphics, copy, sparse only as of 1.2

    VkQueueFamilyProperties queueFamilies[MAX_QUEUE_FAMILIES];
    uint32_t queueFamilyCount =
        sizeof(queueFamilies) / sizeof(queueFamilies[0]);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             queueFamilies);
    for (int i = 0; i != queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) return i;
    }
    return -1;
}

VkPhysicalDevice SelectPhysicalDevice(
    VkPhysicalDevice* devices, uint32_t count,
    VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    assert(devices);
    assert(count);
    VkPhysicalDeviceProperties props;
    for (VkPhysicalDevice* p = devices; p != devices + count; ++p) {
        vkGetPhysicalDeviceProperties(*p, &props);
        if (props.deviceType == type) {
            cout << "Selected device " << props.deviceName << endl;
            return *p;
        }
    }
    return VK_NULL_HANDLE;
}

const char** GetRequiredExtensions(uint32_t* count) {
    return glfwGetRequiredInstanceExtensions(count);
}

VkSemaphore CreateSemaphore(VkDevice device) {
    VkSemaphoreCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .flags = 0};
    VkSemaphore semaphore = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &semaphore));
    return semaphore;
}

VkBool32 DebugReportCallback(VkDebugReportFlagsEXT flags,
                             VkDebugReportObjectTypeEXT objectType,
                             uint64_t object, size_t location,
                             int32_t messageCode, const char* pLayerPrefix,
                             const char* pMessage, void* pUserData) {
    if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) ||
        (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)) {
        cout << location << " - " << pLayerPrefix << ": " << pMessage << endl;
    }
    assert((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) == 0);
    return VK_FALSE;
}

VkDebugReportCallbackEXT RegisterDebugCallback(VkInstance instance) {
    VkDebugReportCallbackCreateInfoEXT info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
        .flags = VK_DEBUG_REPORT_DEBUG_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT |
                 VK_DEBUG_REPORT_WARNING_BIT_EXT |
                 VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,
        .pfnCallback = DebugReportCallback};
    VK_EXT(instance, CreateDebugReportCallbackEXT);
    assert(vkCreateDebugReportCallbackEXT);

    VkDebugReportCallbackEXT callback = VK_NULL_HANDLE;
    VK_CHECK(
        vkCreateDebugReportCallbackEXT(instance, &info, nullptr, &callback));
    return callback;
}

VkImageMemoryBarrier ImageBarrier(VkImage image, VkAccessFlags srcAccessMask,
                                  VkImageLayout oldLaout,
                                  VkAccessFlags dstAccessMask,
                                  VkImageLayout newLayout) {
    VkImageMemoryBarrier result = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLaout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image};

    result.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    result.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return result;
}

bool SupportPresentation(VkInstance instance, VkPhysicalDevice physicalDevice,
                         uint32_t familyIndex) {
    return glfwGetPhysicalDevicePresentationSupport(instance, physicalDevice,
                                                    familyIndex) == GLFW_TRUE;
}

//==============================================================================
//------------------------------------------------------------------------------
VkSurfaceKHR CreateSurface(VkInstance instance, GLFWwindow* window) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkXlibSurfaceCreateInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = glfwGetX11Display(),
        .window = glfwGetX11Window(window)};
    vkCreateXlibSurfaceKHR(instance, &info, nullptr, &surface);
    return surface;
#if 0
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    glfwCreateWindowSurface(instance, window, nullptr, &surface);
    assert(surface != VK_NULL_HANDLE);
    return surface;
#endif
}

//------------------------------------------------------------------------------
VkInstance CreateInstance() {
    VkApplicationInfo appInfo = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                 .apiVersion = VK_API_VERSION_1_1};
    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo};

// CMake adds -DNDEBUG to the CMAKE_C_FLAGS_{RELEASE, MINSIZEREL} by default
#ifndef NDEBUG
    const char* debugLayers[] = {"VK_LAYER_KHRONOS_validation"};
    createInfo.ppEnabledLayerNames = debugLayers;
    createInfo.enabledLayerCount = sizeof(debugLayers) / sizeof(debugLayers[0]);
#endif

    const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME,
                                VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
                                VK_KHR_XLIB_SURFACE_EXTENSION_NAME};
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledExtensionCount =
        sizeof(extensions) / sizeof(extensions[0]);

    // createInfo.ppEnabledExtensionNames =
    // GetRequiredExtensions(&createInfo.enabledExtensionCount);

    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
    return instance;
}

//------------------------------------------------------------------------------
VkPhysicalDevice CreatePhysicalDevice(
    VkInstance instance,
    VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    constexpr size_t MAX_PHYSICAL_DEVICES = 16;
    VkPhysicalDevice physicalDevices[MAX_PHYSICAL_DEVICES];
    uint32_t numPhysicalDevices =
        sizeof(physicalDevices) / sizeof(physicalDevices[0]);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &numPhysicalDevices,
                                        physicalDevices));
    VkPhysicalDevice physicalDevice =
        SelectPhysicalDevice(physicalDevices, numPhysicalDevices, type);
    return physicalDevice;
}

//------------------------------------------------------------------------------
VkDevice CreateDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamily) {
    const float priorities[] = {1.0f};
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamily,
        .queueCount = 1,
        .pQueuePriorities = priorities};

    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]),
        .ppEnabledExtensionNames = extensions};

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));
    assert(device != VK_NULL_HANDLE);
    return device;
}

//------------------------------------------------------------------------------
VkSwapchainKHR CreateSwapChain(VkPhysicalDevice physicalDevice, VkDevice device,
                               VkSurfaceKHR surface, uint32_t familyIndex, /*
                                uint32_t width, uint32_t height,*/
                               VkSwapchainKHR oldSwapchain = 0) {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                       &surfaceCapabilities));
    VkSurfaceCapabilitiesKHR caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                       &caps));
    const uint32_t width = caps.currentExtent.width;
    const uint32_t height = caps.currentExtent.height;

    VkCompositeAlphaFlagBitsKHR surfaceComposite =
        (surfaceCapabilities.supportedCompositeAlpha &
         VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
            ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
            : (surfaceCapabilities.supportedCompositeAlpha &
               VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
                  ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
                  : (surfaceCapabilities.supportedCompositeAlpha &
                     VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
                        ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
                        : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

#ifdef PRINT_SURFACE_CAPABILITIES
    cout << "currentExtent: "
         << "width: " << surfaceCapabilities.currentExtent.width
         << " height: " << surfaceCapabilities.currentExtent.height << endl;
    bitset<sizeof(surfaceCapabilities.currentTransform)> bits(
        surfaceCapabilities.currentTransform);
    string bs = bits.to_string();
    reverse(begin(bs), end(bs));
    cout << "currentTransform: " << bs << endl;

#endif
#ifdef PRINT_COLOR_FORMAT
    const size_t MAX_SURFACE_FORMATS = 128;
    VkSurfaceFormatKHR surfaceFormats[MAX_SURFACE_FORMATS];
    uint32_t count = sizeof(surfaceFormats) / sizeof(surfaceFormats[0]);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                                  &count, surfaceFormats));
    for (int i = 0; i != count; ++i) {
        cout << surfaceFormats[i].colorSpace << " " << surfaceFormats[i].format
             << endl;
    }
#endif
    VkSwapchainCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = 2,
        .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = {.width = width, .height = height},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .queueFamilyIndexCount = 1,
        .preTransform = surfaceCapabilities.currentTransform,
        // VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,  //
        // surfaceCapabilities.currentTransform,
        .compositeAlpha = surfaceComposite,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .oldSwapchain = oldSwapchain};
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain));
#ifdef PRINT_SWAPCHAIN_INFO
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    cout << "Image count: " << imageCount << endl;
#endif
    return swapchain;
}

//------------------------------------------------------------------------------
VkCommandPool CreateCommandPool(VkDevice device, uint32_t familyIndex) {
    VkCommandPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = familyIndex};
    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(device, &info, nullptr, &pool));
    return pool;
}

//------------------------------------------------------------------------------
VkRenderPass CreateRenderPass(VkDevice device) {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkAttachmentReference colorAttachments = {
        0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkAttachmentDescription attachments[1] = {};
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].format = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachments};

    VkRenderPassCreateInfo renderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = sizeof(attachments) / sizeof(attachments[0]),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass};
    VK_CHECK(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr,
                                &renderPass));

    return renderPass;
}

//------------------------------------------------------------------------------
VkFramebuffer CreateFramebuffer(VkDevice device, VkRenderPass renderPass,
                                VkImageView imageView, uint32_t width,
                                uint32_t height) {
    VkFramebufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass,
        .attachmentCount = 1,
        .pAttachments = &imageView,
        .width = width,
        .height = height,
        .layers = 1};
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFramebuffer(device, &info, nullptr, &framebuffer));
    return framebuffer;
}

VkImageView CreateImageView(VkDevice device, VkImage image) {
    VkImageViewCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .levelCount = 1,
                             .layerCount = 1}};

    VkImageView view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(device, &info, nullptr, &view));
    return view;
}

VkShaderModule LoadShader(VkDevice device, const char* path) {
    FILE* file = fopen(path, "rb");
    assert(file);
    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    assert(length > 0);
    fseek(file, 0, SEEK_SET);
    char* buffer = new char[length];
    const size_t rc = fread(buffer, 1, length, file);
    assert(rc == length);
    assert(length % sizeof(uint32_t) == 0);
    fclose(file);
    VkShaderModuleCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = length,
        .pCode = reinterpret_cast<uint32_t*>(buffer)};
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &shaderModule));
    return shaderModule;
}

VkPipelineLayout CreatePipelineLayout(VkDevice device) {
    VkPipelineLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &info, nullptr, &layout));

    return layout;
}

VkPipeline CreateGraphicsPipeline(VkDevice device,
                                  VkPipelineCache pipelineCache,
                                  VkRenderPass renderPass, VkShaderModule vs,
                                  VkShaderModule fs, VkPipelineLayout layout) {
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkGraphicsPipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = sizeof(stages) / sizeof(stages[0]),
        .pStages = stages};

    VkPipelineVertexInputStateCreateInfo vertexInput = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    info.pVertexInputState = &vertexInput;

    // TODO: temporary
    VkVertexInputBindingDescription stream = {0, 8 * sizeof(float),
                                              VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[3] = {};

    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = 12;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset = 24;

    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &stream;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions = attrs;
    //

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    info.pInputAssemblyState = &inputAssembly;

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1};
    info.pViewportState = &viewportState;

    VkPipelineRasterizationStateCreateInfo rasterizationState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0f};
    info.pRasterizationState = &rasterizationState;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

    info.pDepthStencilState = &depthStencilState;

    VkPipelineMultisampleStateCreateInfo multisampleState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    info.pMultisampleState = &multisampleState;

    VkPipelineColorBlendAttachmentState colorAttachmentState = {
        .colorWriteMask = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_R_BIT};

    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachmentState};
    info.pColorBlendState = &colorBlendState;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]),
        .pDynamicStates = dynamicStates};
    info.pDynamicState = &dynamicState;

    info.renderPass = renderPass;
    info.layout = layout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCreateGraphicsPipelines(device, pipelineCache, 1, &info, nullptr,
                              &pipeline);

    return pipeline;
}

//------------------------------------------------------------------------------
struct Swapchain {
    VkSwapchainKHR swapchain;
    vector<VkImage> images;
    vector<VkImageView> imageViews;
    vector<VkFramebuffer> framebuffers;
    uint32_t width;
    uint32_t height;
    uint32_t imageCount;
};

void CreateSwapchain(Swapchain& result, VkPhysicalDevice physicalDevice,
                     VkDevice device, VkSurfaceKHR surface,
                     uint32_t familyIndex, VkRenderPass renderPass,
                     VkSwapchainKHR oldSwapchain = 0) {
    VkSurfaceCapabilitiesKHR caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                       &caps));
    const uint32_t width = caps.currentExtent.width;
    const uint32_t height = caps.currentExtent.height;
    VkSwapchainKHR swapchain = CreateSwapChain(physicalDevice, device, surface,
                                               familyIndex, oldSwapchain);
    uint32_t imageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
    vector<VkImage> images(imageCount);
    VK_CHECK(
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data()));

    vector<VkImageView> imageViews(imageCount);
    for (uint32_t i = 0; i != imageCount; ++i) {
        imageViews[i] = CreateImageView(device, images[i]);
    }

    vector<VkFramebuffer> framebuffers(imageCount);
    for (uint32_t i = 0; i != imageCount; ++i) {
        framebuffers[i] =
            CreateFramebuffer(device, renderPass, imageViews[i], width, height);
    }

    result.swapchain = swapchain;
    result.images = images;
    result.imageViews = imageViews;
    result.framebuffers = framebuffers;
    result.width = width;
    result.height = height;
    result.imageCount = imageCount;
}

void DestroySwapchain(VkDevice device, Swapchain& swapchain) {
    for (uint32_t i = 0; i != swapchain.imageCount; ++i) {
        vkDestroyFramebuffer(device, swapchain.framebuffers[i], nullptr);
    }
    for (uint32_t i = 0; i != swapchain.imageCount; ++i) {
        vkDestroyImageView(device, swapchain.imageViews[i], nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);
}

void ResizeSwapchain(Swapchain& result, VkPhysicalDevice physicalDevice,
                     VkDevice device, VkSurfaceKHR surface,
                     uint32_t familyIndex, VkRenderPass renderPass,
                     VkSwapchainKHR oldSwapchain = 0) {
    VkSurfaceCapabilitiesKHR caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                       &caps));
    const uint32_t newWidth = caps.currentExtent.width;
    const uint32_t newHeight = caps.currentExtent.height;

    if (result.width == newWidth && result.height == newHeight) return;

    Swapchain old = result;
    CreateSwapchain(result, physicalDevice, device, surface, familyIndex,
                    renderPass, old.swapchain);
    VK_CHECK(vkDeviceWaitIdle(device));
    DestroySwapchain(device, old);
}

//==============================================================================
//------------------------------------------------------------------------------
struct Vertex {
    float vx, vy, vz;
    float nx, ny, nz;
    float tu, tv;
};

struct Mesh {
    vector<Vertex> vertices;
    vector<uint32_t> indices;
};

union Triangle {
    Vertex v[3];
    char data[sizeof(Vertex) * 3];
};

bool LoadMesh(Mesh& result, const char* path) {
    std::string inputfile = path;
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = "./";  // Path to material files

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(inputfile, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        exit(1);
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            int fv = shapes[s].mesh.num_face_vertices[f];
            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++) {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                const ssize_t vidx = idx.vertex_index;
                const ssize_t nidx = idx.normal_index;
                const ssize_t tidx = idx.texcoord_index;
                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
                tinyobj::real_t nx =
                    nidx >= 0 ? attrib.normals[3 * nidx + 0] : 0;
                tinyobj::real_t ny =
                    nidx >= 0 ? attrib.normals[3 * nidx + 1] : 0;
                tinyobj::real_t nz =
                    nidx >= 0 ? attrib.normals[3 * nidx + 2] : 0;
                tinyobj::real_t tx =
                    tidx >= 0 ? attrib.texcoords[2 * tidx + 0] : 0;
                tinyobj::real_t ty =
                    tidx >= 0 ? attrib.texcoords[2 * tidx + 1] : 0;
                // Optional: vertex colors
                // tinyobj::real_t red = attrib.colors[3*idx.vertex_index+0];
                // tinyobj::real_t green = attrib.colors[3*idx.vertex_index+1];
                // tinyobj::real_t blue = attrib.colors[3*idx.vertex_index+2];
            }
            index_offset += fv;

            // per-face material
            shapes[s].mesh.material_ids[f];
        }
    }

    assert(!"To be completed...");
}

//------------------------------------------------------------------------------

uint32_t SelectMemoryType(const VkPhysicalDeviceMemoryProperties& memProps,
                          uint32_t memTypeBits, VkMemoryPropertyFlags flags) {
    for (uint32_t i = 0; i != memProps.memoryTypeCount; ++i) {
        if ((memTypeBits & (1 << i)) != 0 &&
            (memProps.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    assert(!"No compatible memory type found");
    return ~0u;
}

//------------------------------------------------------------------------------
struct Buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* data;
    size_t size;
};

void CreateBuffer(Buffer& result, VkDevice device,
                  VkPhysicalDeviceMemoryProperties memProps, size_t size,
                  VkBufferUsageFlags usage) {
    VkBufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                     .size = size, .usage = usage};
    VkBuffer buffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device, &createInfo, nullptr, &buffer));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
    uint32_t memoryTypeIndex =
        SelectMemoryType(memProps, memoryRequirements.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    assert(memoryTypeIndex != ~0u);

    VkMemoryAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memoryRequirements.size,
        memoryTypeIndex = memoryTypeIndex};

    VkDeviceMemory memory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device, &allocateInfo, nullptr, &memory));

    VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));

    void* data = nullptr;
    VK_CHECK(vkMapMemory(device, memory, 0, size, 0, &data));

    result.buffer = buffer;
    result.memory = memory;
    result.data = data;
    result.size = size;
}

void DestroyBuffer(Buffer& buffer, VkDevice device) {
    vkFreeMemory(device, buffer.memory, nullptr);
    vkDestroyBuffer(device, buffer.buffer, nullptr);
}

//==============================================================================
//------------------------------------------------------------------------------
int main(int argc, char const* argv[]) {
    assert(glfwInit());
    assert(glfwVulkanSupported() == GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // VK_CHECK(volkInitialize());
    GLFWwindow* win = glfwCreateWindow(1024, 768, "niagara", nullptr, nullptr);
    assert(win);

    VkInstance instance = CreateInstance();
    // volkLoadInstance(instance);

    VkDebugReportCallbackEXT debugCallback = RegisterDebugCallback(instance);

    VkPhysicalDevice physicalDevice =
        CreatePhysicalDevice(instance, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
    assert(physicalDevice != VK_NULL_HANDLE);

    const int graphicsQueueFamily = FindGraphicsQueueFamily(physicalDevice);
    assert(graphicsQueueFamily >= 0);

    // if(!SupportPresentation(instance, physicalDevice, graphicsQueueFamily)) {
    //     cerr << "Device does not support presentation" << endl;
    //     exit(EXIT_FAILURE);
    // }

    VkDevice device =
        CreateDevice(physicalDevice, uint32_t(graphicsQueueFamily));

    VkSurfaceKHR surface = CreateSurface(instance, win);

    VkBool32 supported = VK_FALSE;
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
        physicalDevice, graphicsQueueFamily, surface, &supported));
    assert(supported == VK_TRUE);

    int width = 0;
    int height = 0;
    glfwGetWindowSize(win, &width, &height);
    VkRenderPass renderPass = CreateRenderPass(device);
    Swapchain swapchain;
    CreateSwapchain(swapchain, physicalDevice, device, surface,
                    graphicsQueueFamily, renderPass);

    VkSemaphore acquireSemaphore = CreateSemaphore(device);
    VkSemaphore releaseSemaphore = CreateSemaphore(device);

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &queue);
    assert(queue != VK_NULL_HANDLE);

    // cmake build path: build/bin/debug|release
    // cmake shaders build path: build/shaders
    const char* VSPATH = "../../shaders/mesh1.vert.glsl.spv";
    const char* FSPATH = "../../shaders/mesh1.frag.glsl.spv";
    VkShaderModule triangleVS = LoadShader(device, VSPATH);
    VkShaderModule triangleFS = LoadShader(device, FSPATH);

    VkPipelineLayout layout = CreatePipelineLayout(device);
    VkPipelineCache cache = VK_NULL_HANDLE;
    VkPipeline trianglePipeline = CreateGraphicsPipeline(
        device, cache, renderPass, triangleVS, triangleFS, layout);

    VkCommandPool commandPool = CreateCommandPool(device, graphicsQueueFamily);

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1};
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    const char* meshPath = "../../../assets/tmp-data/kitten.obj";
    Mesh mesh;
    bool rcm = LoadMesh(mesh, meshPath);
    assert(rcm);
    Buffer vb = {};
    const size_t BUFSIZE = 128 * 1024 * 1024;
    CreateBuffer(vb, device, memProps, BUFSIZE,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    Buffer ib = {};
    CreateBuffer(ib, device, memProps, BUFSIZE,
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    assert(vb.size >= mesh.vertices.size() * sizeof(Vertex));
    memcpy(vb.data, mesh.vertices.data(), mesh.vertices.size());
    assert(ib.size >= mesh.indices.size() * sizeof(uint32_t));
    memcpy(ib.data, mesh.indices.data(), mesh.indices.size());

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        glfwGetWindowSize(win, &width, &height);
        ResizeSwapchain(swapchain, physicalDevice, device, surface,
                        graphicsQueueFamily, renderPass, swapchain.swapchain);
        uint32_t imageIndex = 0;
        VK_CHECK(vkAcquireNextImageKHR(device, swapchain.swapchain,
                                       ~uint64_t(0), acquireSemaphore,
                                       VK_NULL_HANDLE, &imageIndex));

        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        VkImageMemoryBarrier renderBeginBarrier = ImageBarrier(
            swapchain.images[imageIndex], 0, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0,
                             nullptr, 1, &renderBeginBarrier);

        VkClearColorValue color = {48.f / 255.f, 10.f / 255.f, 36.f / 255.f, 1};
        VkClearValue clearColor = {.color = color};

        VkRenderPassBeginInfo passBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        passBeginInfo.renderPass = renderPass;
        passBeginInfo.framebuffer = swapchain.framebuffers[imageIndex];
        passBeginInfo.renderArea.extent.width = swapchain.width;
        passBeginInfo.renderArea.extent.height = swapchain.height;
        passBeginInfo.clearValueCount = 1,
        passBeginInfo.pClearValues = &clearColor;

        //-------------------------------------------------
        vkCmdBeginRenderPass(commandBuffer, &passBeginInfo,
                             VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = {.x = 0,
                               .y = float(height),
                               .width = float(width),
                               .height = -float(height)};
        VkRect2D scissor = {.offset = {0, 0},
                            .extent = {uint32_t(width), uint32_t(height)}};

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        // DRAW CALLS HERE!!!
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          trianglePipeline);

        VkDeviceSize dummyOffset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb.buffer, &dummyOffset);
        vkCmdBindIndexBuffer(commandBuffer, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
        // vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        vkCmdDrawIndexed(commandBuffer, mesh.indices.size(), 1, 0, 0, 0);
        vkCmdEndRenderPass(commandBuffer);
        //-------------------------------------------------

        // Not needed because clear color set in render pass
        // VkImageSubresourceRange range = {
        //     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        //     .levelCount = 1,
        //     .layerCount = 1};

        // vkCmdClearColorImage(commandBuffer, images[imageIndex],
        //                      VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);

        VkImageMemoryBarrier renderEndBarrier = ImageBarrier(
            swapchain.images[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkCmdPipelineBarrier(
            commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0,
            nullptr, 0, nullptr, 1, &renderEndBarrier);
        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkPipelineStageFlags submitStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                   .waitSemaphoreCount = 1,
                                   .pWaitSemaphores = &acquireSemaphore,
                                   .pWaitDstStageMask = &submitStageMask,
                                   .commandBufferCount = 1,
                                   .pCommandBuffers = &commandBuffer,
                                   .signalSemaphoreCount = 1,
                                   .pSignalSemaphores = &releaseSemaphore};

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &releaseSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain.swapchain,
            .pImageIndices = &imageIndex};

        VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

        VK_CHECK(vkDeviceWaitIdle(device));
        // VK_CHECK(vkQueueWaitIdle(queue));

        // TODO: remove when we switch to desktop compute
        glfwWaitEvents();
    }

    VK_CHECK(vkDeviceWaitIdle(device));
    DestroyBuffer(vb, device);
    DestroyBuffer(ib, device);
    ;
    vkDestroyCommandPool(device, commandPool, nullptr);
    DestroySwapchain(device, swapchain);
    vkDestroyPipeline(device, trianglePipeline, nullptr);
    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyShaderModule(device, triangleVS, nullptr);
    vkDestroyShaderModule(device, triangleFS, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
    vkDestroySemaphore(device, releaseSemaphore, nullptr);
    vkDestroySemaphore(device, acquireSemaphore, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    glfwDestroyWindow(win);
    vkDestroyDevice(device, nullptr);
    VK_EXT(instance, DestroyDebugReportCallbackEXT);
    vkDestroyDebugReportCallbackEXT(instance, debugCallback, nullptr);
    vkDestroyInstance(instance, nullptr);
    return 0;
}
