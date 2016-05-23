#
# datum
#

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -ffast-math")

add_definitions(-DWIN32_LEAN_AND_MEAN)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)

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
set(SRCS ${SRCS} scene/storage.h)
set(SRCS ${SRCS} scene/scene.h scene/scene.cpp)
set(SRCS ${SRCS} scene/entity.h scene/entity.cpp)
set(SRCS ${SRCS} scene/basiccomponent.h scene/delegate.h)
set(SRCS ${SRCS} scene/namecomponent.h scene/namecomponent.cpp)
set(SRCS ${SRCS} scene/transformcomponent.h scene/transformcomponent.cpp)
set(SRCS ${SRCS} scene/spritecomponent.h scene/spritecomponent.cpp)
set(SRCS ${SRCS} scene/meshcomponent.h scene/meshcomponent.cpp)
set(SRCS ${SRCS} scene/lightcomponent.h scene/lightcomponent.cpp)
set(SRCS ${SRCS} scene/model.h scene/model.cpp)
set(SRCS ${SRCS} renderer/camera.h renderer/camera.cpp)
set(SRCS ${SRCS} renderer/resource.h renderer/resource.cpp)
set(SRCS ${SRCS} renderer/resourcepool.h renderer/resourcepool.cpp)
set(SRCS ${SRCS} renderer/commandlist.h renderer/commandlist.cpp)
set(SRCS ${SRCS} renderer/spritelist.h renderer/spritelist.cpp)
set(SRCS ${SRCS} renderer/meshlist.h renderer/meshlist.cpp)
set(SRCS ${SRCS} renderer/casterlist.h renderer/casterlist.cpp)
set(SRCS ${SRCS} renderer/lightlist.h renderer/lightlist.cpp)
set(SRCS ${SRCS} renderer/skybox.h renderer/skybox.cpp)
set(SRCS ${SRCS} renderer/texture.h renderer/texture.cpp)
set(SRCS ${SRCS} renderer/sprite.h renderer/sprite.cpp)
set(SRCS ${SRCS} renderer/font.h renderer/font.cpp)
set(SRCS ${SRCS} renderer/mesh.h renderer/mesh.cpp)
set(SRCS ${SRCS} renderer/material.h renderer/material.cpp)
set(SRCS ${SRCS} renderer/occlusion.h renderer/occlusion.cpp)
set(SRCS ${SRCS} renderer/renderer.h renderer/renderer.cpp)
set(SRCS ${SRCS} renderer/vulkan.h renderer/vulkan.cpp)
set(SRCS ${SRCS} renderlist.h renderlist.cpp)
set(SRCS ${SRCS} debug.h debug.cpp)

add_library(datum STATIC ${SRCS})

target_link_libraries(datum leap vulkan)

#
# install
#

INSTALL(TARGETS datum DESTINATION lib)
