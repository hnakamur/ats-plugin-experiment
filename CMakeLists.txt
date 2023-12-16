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
add_atsplugin(remap src/remap/remap.cc)
add_atsplugin(remap_header_add src/remap_header_add/remap_header_add.cc)
add_atsplugin(remap_passthru src/remap_passthru/remap_passthru.cc)
add_atsplugin(remap_echo src/remap_echo/remap_echo.cc)
add_atsplugin(obj_store_auth src/obj_store_auth/obj_store_auth.cc src/obj_store_auth/aws_auth_v4.cc)
target_link_libraries(obj_store_auth PRIVATE ${SODIUM_LIBRARY})

add_executable(lmdb_setup src/lmdb_setup/lmdb_setup.cc)
target_include_directories(lmdb_setup PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_INSTALL_PREFIX}/include)
target_link_libraries(lmdb_setup PRIVATE ${YAMLCPP_LIBRARY} ${LMDB_LIBRARY} ${SWOC_LIBRARY})
install(TARGETS lmdb_setup ${CMAKE_INSTALL_BINDIR})

include(clang_format)
set(CLANG_FORMART_TARGET_DIRECTORIES src)
set(CLANG_FORMART_TARGET_PATTERNS *.h;*.cpp)
add_clang_format_target()
