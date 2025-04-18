#   SiddiqSoft CMakeLists 
#   Copyright 2024 Abdulkareem Siddiq. All rights reserved.
#   See LICENSE file
#
#   When building from our CI server, the following options must be set
#   BUILD_TESTS ON
#   CI_BUILDID must be set to the gitversion
# 
cmake_minimum_required(VERSION 3.29)

# _________________
# Version detection; must preceed the declaration of the actual project()
# Figures out the value ${CURRENT_PROJECT_VERSION} and therefore the ${PROJECT_VERSION}
# Brings in the CPM module into pack/CPM.make
include(pack/CMakeCommonHelpers.cmake)

# ______________________
# Project Metadata block
project(asynchrony
        VERSION         "${CURRENT_PROJECT_VERSION}"
        LANGUAGES       CXX
        HOMEPAGE_URL    "%%myurl%%"
        DESCRIPTION     "Classes to allow you to build asynchrony in your applications using standard C++ jthread, semaphore, deque")

# ____________________________________
# Options; define and declare defaults
option(${PROJECT_NAME}_BUILD_TESTS "${PROJECT_NAME} - Build tests. Uncheck for install only runs" OFF)

# ____________________________________
#  Library Definition
add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
                            INTERFACE
                            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

# For std::stop_token and std::jthread on Clang 18 and Clang 19 we need to enable
# the experimental-library flag.
message(STATUS "Compiler for ${PROJECT_NAME} `${CMAKE_CXX_COMPILER_ID}`")
target_compile_options( ${PROJECT_NAME}
                        INTERFACE
                        $<$<CXX_COMPILER_ID:Clang>:-fexperimental-library> )

# ____________________________________
# Dependencies
cpmaddpackage("gh:siddiqsoft/RunOnEnd#1.3.2")
target_link_libraries(${PROJECT_NAME} INTERFACE RunOnEnd::RunOnEnd)
    

# ____________________________________
# Testing
# Available only when building in our repo; controlled by the switch: ${PROJECT_NAME}_BUILD_TESTS = ON
if(${${PROJECT_NAME}_BUILD_TESTS} AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests" )
    message(STATUS "${PROJECT_NAME} - Asked to build tests.. ${PROJECT_NAME}_BUILD_TESTS = ${${PROJECT_NAME}_BUILD_TESTS}")
    enable_testing()
    add_subdirectory(tests)
endif()
