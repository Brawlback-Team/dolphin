include(RemoveCompileFlag)

macro(dolphin_disable_warnings_msvc _target)
  if (MSVC)
    get_target_property(_target_cxx_flags ${_target} COMPILE_OPTIONS)
    if (_target_cxx_flags)
      set(new_flags "")
      foreach(flag IN LISTS _target_cxx_flags)
        # all warning flags start with "/W" or "/w" or "-W" or "-w"
        if (NOT "${flag}" MATCHES "^[-/][Ww]")
          list(APPEND new_flags "${flag}")
        endif()
      endforeach()
      set_target_properties(${_target} PROPERTIES COMPILE_OPTIONS "${new_flags}")
    endif()
    target_compile_options(${_target} PRIVATE "/W0")
  endif()
endmacro()
