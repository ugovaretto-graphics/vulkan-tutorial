# https://www.youtube.com/watch?v=BR2my8OE1Sc&t=2646s&ab_channel=ArsenyKapoulkine
cmake_minimum_required(VERSION 3.1...3.17)

if(${CMAKE_VERSION} VERSION_LESS 3.16)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
    cmake_policy(VERSION 3.16)
endif()
project(vulkan-tutorial VERSION 1.0
                   DESCRIPTION ""
                   LANGUAGES CXX)

# Separate debug-release paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/release)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#packages
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(Vulkan REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

add_compile_definitions(VK_USE_PLATFORM_XLIB_KHR GLFW_EXPOSE_NATIVE_X11 
                        GLFW_INCLUDE_VULKAN)


set(MESHOPTIMIZER_DIR /usr/local/meshoptimizer)
set(VOLK_DIR /usr/local/volk)
link_directories(${MESHOPTIMIZER_DIR}/lib ${VOLK_DIR}/lib)
#includes and libs
set(INCLUDE_DIRS ${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS} 
    ${CMAKE_SOURCE_DIR}/include ${MESHOPTIMIZER_DIR}/include
    ${VOLK_DIR}/include)
set(LIBS ${Vulkan_LIBRARY} ${GLFW_LIBRARIES})

include_directories(${INCLUDE_DIRS})
add_subdirectory(src)