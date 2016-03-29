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

#pkg_check_modules(GLM REQUIRED glm)
if (NOT EXISTS /usr/include/glm)
    message(SEND_ERROR "-- missing glm headers, plz install glm first")
endif()

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${GLFW_INCLUDE_DIRS})
include_directories(${GLEW_INCLUDE_DIRS})

file(GLOB SRCS src/*.cc)

add_executable(${target} ${SRCS})
target_link_libraries(${target} ${GLFW_LIBRARIES} ${GLEW_LIBRARIES})

# install stage
install(TARGETS ${target} ${client} RUNTIME DESTINATION bin)
