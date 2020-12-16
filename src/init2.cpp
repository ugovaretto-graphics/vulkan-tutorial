#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <bitset>
#include <cassert>
#include <memory>
#include <vector>

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
    VkSemaphoreCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .flags = 0};
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
        .compositeAlpha = VkCompositeAlphaFlagBitsKHR(
            surfaceCapabilities.supportedCompositeAlpha),
        .presentMode = VK_PRESENT_MODE_FIFO_KHR};
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
        0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};  // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};//VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkAttachmentDescription attachments[1] = {};
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].format = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].initialLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                                                   // //
                                                   // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    info.pMultisampleState = &multisampleState;

    VkPipelineColorBlendAttachmentState colorAttachmentState = {
        .colorWriteMask = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_R_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        .sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
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
    VkSwapchainKHR swapchain = CreateSwapChain(
        physicalDevice, device, surface, graphicsQueueFamily, width, height);

    const size_t MAX_SWAPCHAIN_IMAGES = 8;
    VkImage images[MAX_SWAPCHAIN_IMAGES];
    uint32_t imageCount = sizeof(images) / sizeof(images[0]);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images));
    cout << imageCount << endl;
    VkRenderPass renderPass = CreateRenderPass(device);

    const size_t MAX_IMAGE_VIEWS = MAX_SWAPCHAIN_IMAGES;
    VkImageView imageViews[MAX_IMAGE_VIEWS];
    for (uint32_t i = 0; i != imageCount; ++i) {
        imageViews[i] = CreateImageView(device, images[i]);
    }

    const size_t MAX_FRAMEBUFFERS = MAX_SWAPCHAIN_IMAGES;
    VkFramebuffer framebuffers[MAX_FRAMEBUFFERS];
    for (uint32_t i = 0; i != imageCount; ++i) {
        framebuffers[i] =
            CreateFramebuffer(device, renderPass, imageViews[i], width, height);
    }

    VkSemaphore acquireSemaphore = CreateSemaphore(device);
    VkSemaphore releaseSemaphore = CreateSemaphore(device);

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &queue);
    assert(queue != VK_NULL_HANDLE);

    const char* VSPATH = "/home/ugovaretto/projects/vulkan/zeux-niagara/src/shaders/vert.spv";
    const char* FSPATH = "/home/ugovaretto/projects/vulkan/zeux-niagara/src/shaders/frag.spv";
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

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        uint32_t imageIndex = 0;
        VK_CHECK(vkAcquireNextImageKHR(device, swapchain, ~uint64_t(0),
                                       acquireSemaphore, VK_NULL_HANDLE,
                                       &imageIndex));

        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        VkClearColorValue color = {48.f / 255.f, 10.f / 255.f, 36.f / 255.f, 1};
        VkClearValue clearColor = {.color = color};

        VkRenderPassBeginInfo passBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        passBeginInfo.renderPass = renderPass;
        passBeginInfo.framebuffer = framebuffers[imageIndex];
        passBeginInfo.renderArea.extent.width = width;
        passBeginInfo.renderArea.extent.height = height;
        passBeginInfo.clearValueCount = 1,
        passBeginInfo.pClearValues = &clearColor;

        //-------------------------------------------------
        vkCmdBeginRenderPass(commandBuffer, &passBeginInfo,
                             VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = {
            .x = 0, .y = 0, .width = float(width), .height = float(height)};
        VkRect2D scissor = {.offset = {0, 0},
                            .extent = {uint32_t(width), uint32_t(height)}};

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        // DRAW CALLS HERE!!!
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          trianglePipeline);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);
        //-------------------------------------------------

        // Not needed because clear color set in render pass
        // VkImageSubresourceRange range = {
        //     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        //     .levelCount = 1,
        //     .layerCount = 1};

        // vkCmdClearColorImage(commandBuffer, images[imageIndex],
        //                      VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);

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
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex};

        VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

        // VK_CHECK(vkQueueWaitIdle(queue));
        VK_CHECK(vkDeviceWaitIdle(device));
    }
    glfwDestroyWindow(win);
    vkDestroyInstance(instance, nullptr);
    return 0;
}
