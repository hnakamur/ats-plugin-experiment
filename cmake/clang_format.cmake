set(CLANG_FORMAT
    clang-format
    CACHE STRING "Path to clang-format command"
)
set(CLANG_FORMAT_TARGET_EXTENTIONS
    h;hpp;hh;c;cc;cxx;cpp
    CACHE STRING "clang-format target filename extensions"
)

function(add_clang_format_target target directory)
  find_program(CLANG_FORMAT_PATH ${CLANG_FORMAT} REQUIRED)
  list(TRANSFORM CLANG_FORMAT_TARGET_EXTENTIONS PREPEND ${directory}/*. OUTPUT_VARIABLE patterns)
  file(
    GLOB_RECURSE files FOLLOW_SYMLINKS
    LIST_DIRECTORIES false
    ${patterns}
  )
  add_custom_target(
    ${target}
    COMMAND ${CLANG_FORMAT_PATH} -i ${files}
    COMMENT "Checking clang-format for ${target}"
    VERBATIM
  )
endfunction(add_clang_format_target)
