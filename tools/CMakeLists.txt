#
# datum tools
#

set(CMAKE_CXX_STANDARD 14)

if(UNIX OR MINGW)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
endif(UNIX OR MINGW)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-braces -Wno-char-subscripts")
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /wd4800 /wd4267 /wd4146 /wd4814")
endif(MSVC)

add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)

find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)

set(CMAKE_AUTOMOC ON)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)


#
# embed
#

add_executable(assetembed assetembed.cpp)

target_link_libraries(assetembed leap Qt5::Gui)


#
# assetbuilder
#

add_executable(assetbuilder assetbuilder.cpp assetpacker.cpp bc3.cpp hdr.cpp ibl.cpp glslang.cpp)

target_link_libraries(assetbuilder leap Qt5::Gui)


#
# assetdump
#

add_executable(assetdump assetdump.cpp)

target_link_libraries(assetdump leap Qt5::Gui)


#
# assetcompressor
#

add_executable(assetcompressor assetcompressor.cpp assetpacker.cpp bc3.cpp)

target_link_libraries(assetcompressor leap Qt5::Gui)


#
# objparser
#

add_executable(objparser objparser.cpp assetpacker.cpp bc3.cpp)

target_link_libraries(objparser leap Qt5::Gui)


#
# debugviewer
#

add_executable(debugviewer debugviewer.cpp assetpacker.cpp bc3.cpp)

target_link_libraries(debugviewer leap Qt5::Gui Qt5::Widgets)


if(WIN32)
  set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif(WIN32)



#
# install
#

INSTALL(TARGETS assetbuilder DESTINATION bin)
INSTALL(TARGETS assetcompressor DESTINATION bin)
INSTALL(TARGETS objparser DESTINATION bin)
