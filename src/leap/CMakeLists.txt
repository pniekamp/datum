#
# libleap
#

if(UNIX)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -ffast-math")
  add_definitions(-fpic)
endif(UNIX)

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /wd4800 /wd4267")
endif(MSVC)

if(MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mthreads")
endif(MINGW)

if(WIN32)
  add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif(WIN32)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(SRCS ${SRCS} pathstring.cpp)

if(UNIX)
  set(SRCS ${SRCS} threadcontrol-linux.cpp)
endif(UNIX)

if(WIN32)
  set(SRCS ${SRCS} threadcontrol-win32.cpp)
endif(WIN32)

set(SRCS ${SRCS} lz4.cpp)

add_library(leap STATIC ${SRCS})

target_link_libraries(leap ${CMAKE_THREAD_LIBS_INIT})

if(WIN32)
  target_link_libraries(leap WS2_32)
endif(WIN32)


#
# install
#

INSTALL(TARGETS leap DESTINATION lib)

