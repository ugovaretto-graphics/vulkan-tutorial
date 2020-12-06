#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>

#include <cassert>
#include <memory>

#include "common.h"

using namespace std;

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

VkPhysicalDevice SelectPhysicalDevice(VkPhysicalDevice* devices,
                                      uint32_t count) {
    assert(devices);
    assert(count);
    VkPhysicalDeviceProperties props;
    for (VkPhysicalDevice* p = devices; p != devices + count; ++p) {
        vkGetPhysicalDeviceProperties(*p, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            cout << "Selected device " << props.deviceName << endl;
            return *p;
        }
    }
    return devices[0];
}

const char** GetRequiredExtensions(uint32_t* count) {
    return glfwGetRequiredInstanceExtensions(count);
}

VkSemaphore CreateSemaphore(VkDevice device) {
    VkSemaphoreCreateInfo info = {.sType =
                                      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &semaphore));
    return semaphore;
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
VkPhysicalDevice CreatePhysicalDevice(VkInstance instance) {
    constexpr size_t MAX_PHYSICAL_DEVICES = 16;
    VkPhysicalDevice physicalDevices[MAX_PHYSICAL_DEVICES];
    uint32_t numPhysicalDevices =
        sizeof(physicalDevices) / sizeof(physicalDevices[0]);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &numPhysicalDevices,
                                        physicalDevices));
    VkPhysicalDevice physicalDevice =
        SelectPhysicalDevice(physicalDevices, numPhysicalDevices);
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
                               VkSurfaceKHR surface, uint32_t familyIndex,
                               uint32_t width, uint32_t height) {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                       &surfaceCapabilities));
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
    // surfaceCapabilities.
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
        .preTransform =
            VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,  // surfaceCapabilities.currentTransform,
        .compositeAlpha = VkCompositeAlphaFlagBitsKHR(
            surfaceCapabilities.supportedCompositeAlpha),
        .presentMode = VK_PRESENT_MODE_FIFO_KHR};
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain));
    return swapchain;
}
//==============================================================================
//------------------------------------------------------------------------------
int main(int argc, char const* argv[]) {
    assert(glfwInit());
    assert(glfwVulkanSupported() == GLFW_TRUE);
    GLFWwindow* win = glfwCreateWindow(1024, 768, "niagara", nullptr, nullptr);
    assert(win);

    VkInstance instance = CreateInstance();

    VkPhysicalDevice physicalDevice = CreatePhysicalDevice(instance);

    const int graphicsQueueFamily = FindGraphicsQueueFamily(physicalDevice);
    assert(graphicsQueueFamily >= 0);

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
    VkSwapchainKHR swapChain = CreateSwapChain(
        physicalDevice, device, surface, graphicsQueueFamily, width, height);
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
    }

    return 0;
}
