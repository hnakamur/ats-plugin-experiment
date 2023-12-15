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

find_library(TSAPI_LIBRARY tsapi PATHS ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

find_library(YAMLCPP_LIBRARY yaml-cpp PATHS ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Werror=unknown-warning-option)
endif()

include(hardening)

add_compile_options(-flto -Wunused-parameter)
add_link_options(-flto -pthread)

find_library(libsodium sodium)

find_library(liblmdb lmdb)

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
target_link_libraries(obj_store_auth PRIVATE ${libsodium})

add_executable(lmdb_setup src/lmdb_setup/lmdb_setup.cc)
target_include_directories(lmdb_setup PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_INSTALL_PREFIX}/include)
target_link_libraries(lmdb_setup PRIVATE ${YAMLCPP_LIBRARY})
target_link_libraries(lmdb_setup PRIVATE ${liblmdb})
install(TARGETS lmdb_setup ${CMAKE_INSTALL_BINDIR})

function(add_clang_format_target target)
  add_custom_target(
    clang-format-${target}
    ${CMAKE_SOURCE_DIR}/tools/clang-format.sh ${CMAKE_SOURCE_DIR}/${target}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking clang-format for ${target}"
    VERBATIM
  )
  list(APPEND CLANG_FORMAT_TARGETS clang-format-${target})
  set(CLANG_FORMAT_TARGETS
      ${CLANG_FORMAT_TARGETS}
      PARENT_SCOPE
  )
endfunction(add_clang_format_target)

add_clang_format_target(src)
