cmake_minimum_required(VERSION 2.8.11)

set(target blur-exps)

project(${target})

set(CMAKE_EXPORT_COMPILE_COMMANDS on)
#set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror")

set(blur-exps_VERSION_MAJOR 0)
set(blur-exps_VERSION_MINOR 1)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED glew glfw3 gdk-pixbuf-2.0)
pkg_check_modules(DEPS2 REQUIRED gdk-pixbuf-2.0 libdrm gbm egl glesv2)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${DEPS_INCLUDE_DIRS})
include_directories(${DEPS2_INCLUDE_DIRS})

add_executable(${target} src/main.cc)
target_link_libraries(${target} ${DEPS_LIBRARIES})

add_executable(blur_image src/blur_image.cc)
message(${DEPS2_LIBRARIES})
target_link_libraries(blur_image ${DEPS2_LIBRARIES})

# install stage
install(TARGETS ${target} blur_image RUNTIME DESTINATION bin)
