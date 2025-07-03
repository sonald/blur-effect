cmake_minimum_required(VERSION 2.8.11)

set(target blur_image)

project(${target})

set(CMAKE_EXPORT_COMPILE_COMMANDS on)
option(BUILD_DEMO "build windowing demo" off)
option(BUILD_TESTS "build unit tests" on)
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

# Unit tests
if (BUILD_TESTS)
    # Download and unpack googletest at configure time
    configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
    if(result)
      message(STATUS "CMake step for googletest failed: ${result}")
      # Fall back to system gtest if available
      find_package(GTest)
      if (NOT GTEST_FOUND)
        message(FATAL_ERROR "GoogleTest not found and download failed")
      endif()
    else()
      execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
      if(result)
        message(FATAL_ERROR "Build step for googletest failed: ${result}")
      endif()

      # Prevent overriding the parent project's compiler/linker
      # settings on Windows
      set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

      # Add googletest directly to our build. This defines
      # the gtest and gtest_main targets.
      add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                       ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                       EXCLUDE_FROM_ALL)
    endif()

    # Create test library with shared code
    add_library(blur_lib STATIC
        src/blur_utils.cc
    )
    target_link_libraries(blur_lib ${DEPS2_LIBRARIES})

    # Add test executable
    add_executable(blur_tests
        tests/test_gaussian_kernel.cc
        tests/test_shader_utils.cc
        tests/test_device_utils.cc
        tests/test_main.cc
    )
    
    if (GTEST_FOUND)
        target_link_libraries(blur_tests blur_lib ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} pthread)
        target_include_directories(blur_tests PRIVATE ${GTEST_INCLUDE_DIRS})
    else()
        target_link_libraries(blur_tests blur_lib gtest_main pthread)
    endif()
    
    # Enable testing
    enable_testing()
    add_test(NAME blur_unit_tests COMMAND blur_tests)
endif()

# install stage
set(exes blur_image)
if (BUILD_DEMO)
    set(exes ${exes} blur-exp)
endif()
install(TARGETS ${exes} RUNTIME DESTINATION bin)
