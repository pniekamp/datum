#
# datum test
#

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")

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

if(WIN32)
  target_link_libraries(datumtest mingw32)
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-static -static-libgcc -static-libstdc++")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-static -static-libgcc -static-libstdc++")
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "-static -static-libgcc -static-libstdc++")
endif(WIN32)


#
# install
#

INSTALL(TARGETS datumtest DESTINATION bin)

