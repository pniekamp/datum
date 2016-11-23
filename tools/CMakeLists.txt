#
# datum tools
#

set(CMAKE_CXX_STANDARD 14)

if(UNIX OR MINGW)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
endif(UNIX OR MINGW)

add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)

find_package(Qt5Gui REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(SRCS ${SRCS} atlaspacker.h assetpacker.h assetpacker.cpp bc3.h bc3.cpp hdr.h hdr.cpp ibl.h ibl.cpp glslang.h glslang.cpp)

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



if(WIN32)
  set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif(WIN32)



#
# install
#

INSTALL(TARGETS assetbuilder DESTINATION bin)
INSTALL(TARGETS assetcompressor DESTINATION bin)
INSTALL(TARGETS objparser DESTINATION bin)
