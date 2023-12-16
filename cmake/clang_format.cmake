set(CLANG_FORMAT
    clang-format
    CACHE STRING "Path to clang-format command"
)

function(add_clang_format_target)
  list(JOIN ARGN " " dirs)
  set(cmdList
      echo;${dirs};|;tr;\ ;\\n;
      |;xargs;-I;{};find;${CMAKE_SOURCE_DIR}/{};-iname;*.[ch];-o;-iname;*.cc;-o;-iname;*.h.in;
      |;xargs;--verbose;${CLANG_FORMAT};-i
  )

  add_custom_target(
    clang-format
    COMMAND ${cmdList}
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
