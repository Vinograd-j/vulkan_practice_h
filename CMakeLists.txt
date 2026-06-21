cmake_minimum_required(VERSION 4.0)
project(vulkan_practice_h)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(vulkan_practice_h main.cpp)

target_link_libraries(vulkan_practice_h PRIVATE Vulkan::Vulkan glfw glm::glm )