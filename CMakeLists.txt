cmake_minimum_required(VERSION 3.10)
project(Fluid)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

include_directories("include/")

add_executable(fluid-main 
  "src/main.cpp"
  "src/resource-manager.cpp"
  "include/utils.hpp"
  "include/resource-manager.hpp"
)
target_compile_options(fluid-main PRIVATE 
  "-freflection")
target_link_libraries(fluid-main PRIVATE
  GL glfw GLEW glm::glm)