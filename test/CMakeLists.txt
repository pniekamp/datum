#
# datum test
#

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -ffast-math")

include_directories(${PROJECT_SOURCE_DIR}/include)

set(SRCS ${SRCS} datumtest.h datumtest.cpp datumtest-win32.cpp platform.h platform.cpp)

add_executable(datumtest ${SRCS})

target_link_libraries(datumtest leap datum)

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

