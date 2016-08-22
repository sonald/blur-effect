cmake_minimum_required(VERSION 2.8.11)

set(target blur_image)

project(${target})

set(CMAKE_EXPORT_COMPILE_COMMANDS on)
option(BUILD_DEMO "build windowing demo" off)
#set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wno-error")

set(blur-exps_VERSION_MAJOR 0)
set(blur-exps_VERSION_MINOR 1)

find_package(PkgConfig REQUIRED)

if (BUILD_DEMO)
pkg_check_modules(DEPS REQUIRED glew glfw3 gdk-pixbuf-2.0)
endif()

pkg_check_modules(DEPS2 REQUIRED gdk-pixbuf-2.0 libdrm gbm egl glesv2)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR})
if (BUILD_DEMO)
include_directories(${DEPS_INCLUDE_DIRS})
endif()
include_directories(${DEPS2_INCLUDE_DIRS})

if (BUILD_DEMO)
add_executable(blur-exp src/main.cc)
target_link_libraries(blur-exp ${DEPS_LIBRARIES})
endif()

add_executable(blur_image src/blur_image.cc)
target_link_libraries(blur_image ${DEPS2_LIBRARIES})

# install stage
set(exes blur_image)
if (BUILD_DEMO)
    set(exes ${exes} blur-exp)
endif()
install(TARGETS ${exes} RUNTIME DESTINATION bin)
