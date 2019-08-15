
project(datum)

cmake_minimum_required(VERSION 3.5.2)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build Type: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if(UNIX OR MINGW)
  set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -latomic")
endif(UNIX OR MINGW)

if(UNIX OR MINGW)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g" CACHE STRING "" FORCE)
endif(UNIX OR MINGW)

if(MSVC)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob1" CACHE STRING "" FORCE)
endif(MSVC)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib CACHE INTERNAL "")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin CACHE INTERNAL "")

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR} CACHE INTERNAL "")

find_package(Threads REQUIRED)

find_package(Qt5Gui QUIET)

#
# leap
#

find_path(LEAP_INCLUDE leap.h PATHS ${PROJECT_SOURCE_DIR}/../leap/include NO_DEFAULT_PATH)

find_library(LEAP_LIBRARIES NAMES leap PATHS ${PROJECT_SOURCE_DIR}/../leap/lib)

if(LEAP_INCLUDE AND LEAP_LIBRARIES)
  add_library(leap STATIC IMPORTED)
  set_property(TARGET leap PROPERTY NO_SYSTEM_FROM_IMPORTED 1)
  set_property(TARGET leap PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LEAP_INCLUDE})
  set_property(TARGET leap PROPERTY IMPORTED_LOCATION ${LEAP_LIBRARIES})
  set_property(TARGET leap PROPERTY INTERFACE_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

  if(WIN32)
    set_property(TARGET leap APPEND PROPERTY INTERFACE_LINK_LIBRARIES WS2_32)
  endif(WIN32)

  message(STATUS "Found Leap: ${LEAP_INCLUDE}")
else(LEAP_INCLUDE AND LEAP_LIBRARIES)
  message(FATAL_ERROR "Could not find Leap Library")
endif(LEAP_INCLUDE AND LEAP_LIBRARIES)

#
# vulkan
#

if(WIN32)
  find_path(VULKAN_INCLUDE vulkan/vulkan.h HINTS "$ENV{VULKAN_SDK}/Include")
  find_library(VULKAN_LIBRARIES NAMES vulkan-1 HINTS "$ENV{VULKAN_SDK}/Lib" "$ENV{VULKAN_SDK}/Bin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
endif(WIN32)

if(UNIX)
  find_path(VULKAN_INCLUDE vulkan/vulkan.h HINTS "$ENV{VULKAN_SDK}/include")
  find_library(VULKAN_LIBRARIES NAMES libvulkan.so HINTS "$ENV{VULKAN_SDK}/lib")
  find_path(XCB_INCLUDE_DIRS xcb/xcb.h PATH_SUFFIXES xcb)
  find_library(XCB_LIBRARIES xcb)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XCB_KHR")
endif(UNIX)

if(VULKAN_INCLUDE AND VULKAN_LIBRARIES)
  add_library(vulkan STATIC IMPORTED)
  set_property(TARGET vulkan PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${VULKAN_INCLUDE})
  set_property(TARGET vulkan PROPERTY IMPORTED_LOCATION ${VULKAN_LIBRARIES})

  message(STATUS "Found Vulkan: ${VULKAN_INCLUDE} ${VULKAN_LIBRARIES}")
else(VULKAN_INCLUDE AND VULKAN_LIBRARIES)
  message(FATAL_ERROR "Could not find Vulkan Library")
endif(VULKAN_INCLUDE AND VULKAN_LIBRARIES)

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

#
# examples
#

option(EXAMPLES "Examples" OFF)

if(EXAMPLES)
  add_subdirectory(examples)
endif(EXAMPLES)
