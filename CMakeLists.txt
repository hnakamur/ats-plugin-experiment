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
set(CMAKE_CXX_FLAGS "-fstack-protector-strong -Wformat -Werror=format-security -g")
set(CMAKE_C_FLAGS "-fstack-protector-strong -Wformat -Werror=format-security -g")
set(CMAKE_INSTALL_PREFIX /opt/trafficserver)
set(CMAKE_INSTALL_LIBDIR lib)
set(CMAKE_INSTALL_LIBEXECDIR lib/modules)

find_library(TSAPI_LIBRARY tsapi PATHS ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

add_library(atshello MODULE src/hello/hello.cc)
target_include_directories(atshello PUBLIC ${CMAKE_INSTALL_PREFIX}/include)
target_link_libraries(atshello PRIVATE ${TSAPI_LIBRARY})
set_target_properties(atshello PROPERTIES PREFIX "")
set_target_properties(atshello PROPERTIES SUFFIX ".so")
install(TARGETS atshello DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
