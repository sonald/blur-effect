cmake_minimum_required(VERSION 2.8.11)

set(target blur-exps)

project(${target})

set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(blur-exps_VERSION_MAJOR 0)
set(blur-exps_VERSION_MINOR 1)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLEW REQUIRED glew)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(PIXBUF REQUIRED gdk-pixbuf-2.0)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${GLFW_INCLUDE_DIRS})
include_directories(${GLEW_INCLUDE_DIRS})
include_directories(${PIXBUF_INCLUDE_DIRS})

add_executable(${target} src/main.cc)
target_link_libraries(${target} ${GLFW_LIBRARIES} ${GLEW_LIBRARIES})

add_executable(blur_image src/blur_image.cc)
target_link_libraries(blur_image ${GLFW_LIBRARIES} ${GLEW_LIBRARIES} 
    ${PIXBUF_LIBRARIES})

# install stage
install(TARGETS ${target} ${client} RUNTIME DESTINATION bin)
