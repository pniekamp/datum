#
# datum test
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

set(SRCS ${SRCS} datumtest.h datumtest.cpp platform.h platform.cpp)

if(WIN32)
  set(SRCS ${SRCS} datumtest-win32.cpp)
endif(WIN32)

if(UNIX)
  set(SRCS ${SRCS} datumtest-xcb.cpp)
endif(UNIX)

add_executable(datumtest ${SRCS})

target_link_libraries(datumtest leap datum)

if(UNIX)
  target_link_libraries(datumtest ${XCB_LIBRARIES})
endif(UNIX)

if(MINGW)
  target_link_libraries(datumtest mingw32)
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-static -static-libgcc -static-libstdc++")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-static -static-libgcc -static-libstdc++")
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "-static -static-libgcc -static-libstdc++")
endif(MINGW)


#
# install
#

INSTALL(TARGETS datumtest DESTINATION bin)

