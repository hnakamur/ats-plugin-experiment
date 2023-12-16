set(CLANG_FORMAT
    clang-format
    CACHE STRING "Path to clang-format command"
)
set(CLANG_FORMART_TARGET_DIRECTORIES
    src
    CACHE STRING "clang-format target filename directories"
)
set(CLANG_FORMART_TARGET_PATTERNS
    *.[ch];*.cc;*.cpp;*.h.in
    CACHE STRING "clang-format target filename patterns"
)

function(add_clang_format_target)
  list(JOIN CLANG_FORMART_TARGET_DIRECTORIES " " dirs)
  set(cmdList echo;${dirs};|;tr;\ ;\\n; |;xargs;-I;{};find;${CMAKE_SOURCE_DIR}/{})
  foreach(pattern IN LISTS CLANG_FORMART_TARGET_PATTERNS)
    message(STATUS pattern=${pattern})
    list(APPEND cmdList -iname ${pattern} -o)
  endforeach()
  list(POP_BACK cmdList)
  list(
    APPEND
    cmdList
    |
    xargs
    --verbose
    ${CLANG_FORMAT}
    -i
  )
  list(JOIN cmdList " " cmd)
  message(STATUS cmd=${cmd})

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
