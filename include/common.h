#pragma once
#include <vulkan/vulkan.h>

#include <cstdlib>
#include <iostream>

// https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanTools.cpp
inline const char* ErrorString(VkResult errcode) {
#define VKSTR(r) \
    case VK_##r: \
        return #r
    switch (errcode) {
        VKSTR(NOT_READY);
        VKSTR(TIMEOUT);
        VKSTR(EVENT_SET);
        VKSTR(EVENT_RESET);
        VKSTR(INCOMPLETE);
        VKSTR(ERROR_OUT_OF_HOST_MEMORY);
        VKSTR(ERROR_OUT_OF_DEVICE_MEMORY);
        VKSTR(ERROR_INITIALIZATION_FAILED);
        VKSTR(ERROR_DEVICE_LOST);
        VKSTR(ERROR_MEMORY_MAP_FAILED);
        VKSTR(ERROR_LAYER_NOT_PRESENT);
        VKSTR(ERROR_EXTENSION_NOT_PRESENT);
        VKSTR(ERROR_FEATURE_NOT_PRESENT);
        VKSTR(ERROR_INCOMPATIBLE_DRIVER);
        VKSTR(ERROR_TOO_MANY_OBJECTS);
        VKSTR(ERROR_FORMAT_NOT_SUPPORTED);
        VKSTR(ERROR_SURFACE_LOST_KHR);
        VKSTR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        VKSTR(SUBOPTIMAL_KHR);
        VKSTR(ERROR_OUT_OF_DATE_KHR);
        VKSTR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        VKSTR(ERROR_VALIDATION_FAILED_EXT);
        VKSTR(ERROR_INVALID_SHADER_NV);
#undef VKSTR
        default:
            return "UNKNOWN_ERROR";
    }
}

#define VK_CHECK(f) \
    {                                                                    \
        VkResult res = (f);                                              \
        if (res != VK_SUCCESS) {                                         \
            std::cerr << "Fatal : VkResult is \"" << ErrorString(res)    \
                      << "\" in " << __FILE__ << " at line " << __LINE__ \
                      << std::endl;                                      \
            exit(EXIT_FAILURE);                                          \
        }                                                                \
    }

#define VK_P(s, member, os, indent) \
    (os << string(indent, ' ') << #member << ": " << s.member << '\n')
