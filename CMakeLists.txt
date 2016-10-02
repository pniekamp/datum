
project(datum)

cmake_minimum_required(VERSION 2.8.12)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build Type: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob1" CACHE STRING "" FORCE)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib CACHE INTERNAL "")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin CACHE INTERNAL "")

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR} CACHE INTERNAL "")

find_package(Threads REQUIRED)

find_package(Qt5Gui  QUIET)

#
# leap
#

add_subdirectory(src/leap)

#
# vulkan
#

if(WIN32)
  find_path(VULKAN_INCLUDE vulkan/vulkan.h PATH_SUFFIXES ../include)
  find_library(VULKAN_LIBRARIES NAMES vulkan-1 vulkan)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
endif(WIN32)

if(UNIX)
  find_path(VULKAN_INCLUDE vulkan/vulkan.h)
  find_library(VULKAN_LIBRARIES NAMES libvulkan.so)
  find_path(XCB_INCLUDE_DIRS xcb/xcb.h PATH_SUFFIXES xcb)
  find_library(XCB_LIBRARIES xcb)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XCB_KHR")
endif(UNIX)

add_library(vulkan STATIC IMPORTED)
set_property(TARGET vulkan PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${VULKAN_INCLUDE})
set_property(TARGET vulkan PROPERTY IMPORTED_LOCATION ${VULKAN_LIBRARIES})

message(STATUS ${VULKAN_INCLUDE})
message(STATUS ${VULKAN_LIBRARIES})

#
# datum
#

add_subdirectory(src)

#
# test
#

add_subdirectory(test)

#
# tools
#

if(Qt5Gui_FOUND)
  add_subdirectory(tools)
endif(Qt5Gui_FOUND)