cmake_minimum_required(VERSION 3.20..3.27)
project(atshello VERSION 0.0.0)

# We make this a cache entry so that it can be configured to different values
# for testing purposes. For example, it can be used on CI to check compatibility
# with a newer standard than what our codebase currently has to comply with.
set(CMAKE_CXX_STANDARD
    17
    CACHE STRING "The C++ standard to compile with (default 17)"
)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_definitions(DEBUG _DEBUG)
endif()

set(CMAKE_CXX_COMPILER clang++-16)
set(CMAKE_C_COMPILER clang---16)

# The compiler and linker options below are copied from
# https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html
# and -Wtrampolines is removed since it is unsupported to clang.
set(CMAKE_CXX_FLAGS "-O2 -Wall -Wformat=2 -Wconversion -Wimplicit-fallthrough \
-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 \
-D_GLIBCXX_ASSERTIONS \
-fstrict-flex-arrays=3 \
-fstack-clash-protection -fstack-protector-strong")
set(CMAKE_C_FLAGS "-O2 -Wall -Wformat=2 -Wconversion -Wimplicit-fallthrough \
-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 \
-D_GLIBCXX_ASSERTIONS \
-fstrict-flex-arrays=3 \
-fstack-clash-protection -fstack-protector-strong")
set(CMAKE_MODULE_LINKER_FLAGS "-Wl,-z,nodlopen -Wl,-z,noexecstack \
-Wl,-z,relro -Wl,-z,now")

set(CMAKE_INSTALL_PREFIX /opt/trafficserver)
set(CMAKE_INSTALL_LIBDIR lib)
set(CMAKE_INSTALL_LIBEXECDIR lib/modules)

find_library(TSAPI_LIBRARY tsapi PATHS ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

function(add_atsplugin name)
  add_library(${name} MODULE ${ARGN})
  target_include_directories(${name} PRIVATE ${CMAKE_INSTALL_PREFIX}/include)
  target_link_libraries(${name} PRIVATE ${TSAPI_LIBRARY})
  set_target_properties(${name} PROPERTIES PREFIX "")
  set_target_properties(${name} PROPERTIES SUFFIX ".so")
  install(TARGETS ${name} DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
endfunction()

add_atsplugin(hello src/hello/hello.cc)
add_atsplugin(add_header src/add_header/add_header.cc)
