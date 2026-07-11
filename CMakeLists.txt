cmake_minimum_required(VERSION 3.16)
project(raylib-python-ui VERSION 1.0.0 LANGUAGES C)

option(BUILD_SHARED_LIBS "Build shared libraries instead of static" OFF)

# Dependencies
find_package(raylib REQUIRED)
find_package(Python3 COMPONENTS Development REQUIRED)

# Library Target
add_library(raylib-python-ui
    src/ui/python_api.c
    src/ui/python_frame.c
)

target_include_directories(raylib-python-ui PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${Python3_INCLUDE_DIRS}
)

target_link_libraries(raylib-python-ui PUBLIC raylib Python3::Python)

if(UNIX AND NOT APPLE)
    target_link_libraries(raylib-python-ui PRIVATE m)
endif()
