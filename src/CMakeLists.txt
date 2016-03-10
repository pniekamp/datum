#
# datum
#

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -ffast-math")

add_definitions(-DWIN32_LEAN_AND_MEAN)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(SRCS ${SRCS} platform.h)
set(SRCS ${SRCS} memory.h)
set(SRCS ${SRCS} math/vec.h)
set(SRCS ${SRCS} math/rect.h)
set(SRCS ${SRCS} math/color.h)
set(SRCS ${SRCS} math/attenuation.h)
set(SRCS ${SRCS} math/line.h)
set(SRCS ${SRCS} math/plane.h)
set(SRCS ${SRCS} math/sphere.h)
set(SRCS ${SRCS} math/bound.h)
set(SRCS ${SRCS} math/frustum.h)
set(SRCS ${SRCS} math/transform.h)
set(SRCS ${SRCS} corepack.h)
set(SRCS ${SRCS} assetpack.h)
set(SRCS ${SRCS} asset.h asset.cpp)
set(SRCS ${SRCS} debug.h debug.cpp)

add_library(datum STATIC ${SRCS})

target_link_libraries(datum leap)

#
# install
#

INSTALL(TARGETS datum DESTINATION lib)
