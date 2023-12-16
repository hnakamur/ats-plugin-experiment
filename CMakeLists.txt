cmake_minimum_required(VERSION 3.20..3.27)
project(atsmyplugins VERSION 0.0.0)

# We make this a cache entry so that it can be configured to different values
# for testing purposes. For example, it can be used on CI to check compatibility
# with a newer standard than what our codebase currently has to comply with.
set(CMAKE_CXX_STANDARD
    20
    CACHE STRING "The C++ standard to compile with (default 20)"
)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_definitions(DEBUG _DEBUG)
endif()

set(CMAKE_INSTALL_PREFIX /opt/trafficserver)
set(CMAKE_INSTALL_LIBDIR lib)
set(CMAKE_INSTALL_LIBEXECDIR lib/modules)

find_library(SODIUM_LIBRARY sodium)
find_library(LMDB_LIBRARY lmdb)

find_library(TSAPI_LIBRARY tsapi PATHS ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
find_library(YAMLCPP_LIBRARY yaml-cpp PATHS ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
find_library(SWOC_LIBRARY swoc-static-1.5.5 PATHS ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

include(hardening)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Werror=unknown-warning-option)
endif()
add_compile_options(-flto -Wunused-parameter)
add_link_options(-flto -pthread)

add_subdirectory(src)

include(clang_format)
add_clang_format_target(clang_format_include include)
add_clang_format_target(clang_format_src src)
add_custom_target(clang_format DEPENDS clang_format_include clang_format_src)

include(cmake_format)
add_cmake_format_target_toplevel(cmake_format_toplevel)
add_cmake_format_target(cmake_format_cmake_dir cmake)
add_cmake_format_target(cmake_format_src_dir src)
add_custom_target(cmake_format DEPENDS cmake_format_toplevel cmake_format_cmake_dir cmake_format_src_dir)

# NOTE: Remove ALL below if you do not want to execute the formatting target every time.
add_custom_target(format ALL DEPENDS clang_format cmake_format)
