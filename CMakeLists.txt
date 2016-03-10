
project(datum)

cmake_minimum_required(VERSION 2.8.12)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build Type: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g" CACHE STRING "" FORCE)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib CACHE INTERNAL "")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin CACHE INTERNAL "")

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR} CACHE INTERNAL "")

find_package(Threads REQUIRED)

#
# leap
#

find_path(LEAP_INCLUDE leap.h PATHS ${PROJECT_SOURCE_DIR}/../leap/include NO_DEFAULT_PATH)

find_library(LEAP_LIBRARIES NAMES leap PATHS ${PROJECT_SOURCE_DIR}/../leap/lib)

if(LEAP_INCLUDE AND LEAP_LIBRARIES)

  add_library(leap STATIC IMPORTED)
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

add_subdirectory(tools)
