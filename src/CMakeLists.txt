
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

add_executable(mesh1 mesh1.cpp)
add_shader(mesh1 mesh1.frag.glsl)
add_shader(mesh1 mesh1.vert.glsl)
#target_compile_definitions(mesh1 PRIVATE VK_NO_PROTOTYPES)
target_compile_options(mesh1 PRIVATE)
target_link_libraries(mesh1 ${LIBS} meshoptimizer)# volk  dl)

add_executable(mesh2 mesh2.cpp)
add_shader(mesh2 mesh1.frag.glsl)
add_shader(mesh2 mesh2.vert.glsl)
#target_compile_definitions(mesh2 PRIVATE VK_NO_PROTOTYPES)
target_compile_options(mesh2 PRIVATE)
target_link_libraries(mesh2 ${LIBS} meshoptimizer)# volk  dl)

add_executable(hello_triangle_vulkan_samples hello_triangle_vulkan_samples.cpp)
target_link_libraries(hello_triangle_vulkan_samples ${LIBS})
