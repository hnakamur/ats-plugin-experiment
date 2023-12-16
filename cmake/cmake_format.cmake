set(CMAKE_FORMAT
    cmake-format
    CACHE STRING "Path to cmake-format command"
)

function(add_cmake_format_target_toplevel target)
  find_program(CMAKE_FORMAT_PATH ${CMAKE_FORMAT} REQUIRED)
  add_custom_target(
    ${target}
    COMMAND ${CMAKE_FORMAT_PATH} -i ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    VERBATIM
  )
endfunction(add_cmake_format_target_toplevel)

function(add_cmake_format_target target directory)
  find_program(CMAKE_FORMAT_PATH ${CMAKE_FORMAT} REQUIRED)
  set(patterns CMakeLists.txt;*.cmake)
  list(TRANSFORM patterns PREPEND ${directory}/)
  file(
    GLOB_RECURSE files FOLLOW_SYMLINKS
    LIST_DIRECTORIES false
    ${patterns}
  )
  add_custom_target(
    ${target}
    COMMAND ${CMAKE_FORMAT_PATH} -i ${files}
    VERBATIM
  )
endfunction(add_cmake_format_target)
