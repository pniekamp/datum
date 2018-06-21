#
# asteroids
#

set(CMAKE_CXX_STANDARD 14)

if(UNIX OR MINGW)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wfloat-conversion -Wno-unused-parameter -Wno-missing-field-initializers")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wfloat-conversion -Wno-unused-parameter -Wno-missing-field-initializers")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
endif(UNIX OR MINGW)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-braces -Wno-char-subscripts")
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /wd4800 /wd4267 /wd4146 /wd4814")
endif(MSVC)

if(WIN32)
  add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif(WIN32)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(SRCS ${SRCS} asteroids.h asteroids.cpp)

if(WIN32)
  set(SRCS ${SRCS} ../example-win32.cpp ../platform.h ../platform.cpp)
endif(WIN32)

if(UNIX)
  set(SRCS ${SRCS} ../example-xcb.cpp ../platform.h ../platform.cpp)
endif(UNIX)

add_executable(example-asteroids ${SRCS})

target_link_libraries(example-asteroids leap datum vulkan)

if(UNIX)
  target_link_libraries(example-asteroids ${XCB_LIBRARIES})
endif(UNIX)

if(MINGW)
  target_link_libraries(example-asteroids mingw32)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif(MINGW)

#
# install
#

INSTALL(TARGETS example-asteroids DESTINATION bin)
