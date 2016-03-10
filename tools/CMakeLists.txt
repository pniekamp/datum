#
# datum tools
#

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -ffast-math")

find_package(Qt5Gui REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include)

#
# assetbuilder
#

add_executable(assetbuilder atlaspacker.h assetpacker.h assetpacker.cpp assetbuilder.cpp)

target_link_libraries(assetbuilder leap Qt5::Gui)


#
# assetcompressor
#

add_executable(assetcompressor assetpacker.h assetpacker.cpp assetcompressor.cpp)

target_link_libraries(assetcompressor leap Qt5::Gui)


#
# objparser
#

add_executable(objparser assetpacker.h assetpacker.cpp objparser.cpp)

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
