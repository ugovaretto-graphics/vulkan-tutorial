
include(compile_shaders.cmake)

add_executable(init init.cpp)
target_link_libraries(init ${LIBS})

add_executable(init2 init2.cpp)
target_link_libraries(init2 ${LIBS})

add_executable(init3 init3.cpp)
target_link_libraries(init3 ${LIBS})

add_executable(hello_triangle hello_triangle.cpp)
target_link_libraries(hello_triangle ${LIBS})
add_shader(hello_triangle triangle.frag.glsl)
add_shader(hello_triangle triangle.vert.glsl)

add_executable(hello_triangle_vulkan_samples hello_triangle_vulkan_samples.cpp)
target_link_libraries(hello_triangle_vulkan_samples ${LIBS})
