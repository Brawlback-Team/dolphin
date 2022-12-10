find_package(PkgConfig)

macro(_internal_message msg)
	if(NOT ${_is_quiet})
		message(STATUS "${msg}")
	endif()
endmacro()

macro(check_lib var pc lib)
	set(_is_required 0)
	set(_is_quiet 0)
	set(_arg_list ${ARGN})
	foreach(_arg ${ARGN})
		if(_arg STREQUAL "REQUIRED")
			list(REMOVE_ITEM _arg_list "REQUIRED")
			set(_is_required 1)
		endif()
		if(_arg STREQUAL "QUIET")
			list(REMOVE_ITEM _arg_list "QUIET")
			set(_is_quiet 1)
		endif()
	endforeach()

	if(PKG_CONFIG_FOUND AND NOT ${var}_FOUND)
		pkg_search_module(${var} QUIET ${pc})
	endif()

	if(${var}_FOUND)
		if(${var}_INCLUDE_DIRS)
			include_directories(${${var}_INCLUDE_DIRS})
		endif()
		# Make sure include directories for headers found using find_path below
		# are re-added when reconfiguring
		if(${var}_INCLUDE)
			include_directories(${${var}_INCLUDE})
		endif()
		_internal_message("${lib} found")
	else()
		find_library(${var} ${lib})
		if(_arg_list)
			find_path(${var}_INCLUDE ${_arg_list})
		else()
			set(${var}_INCLUDE FALSE)
		endif()
		if(${var} AND ${var}_INCLUDE)
			include_directories(${${var}_INCLUDE})
			_internal_message("${lib} found")
			set(${var}_FOUND 1 CACHE INTERNAL "")
		else()
			if(_is_required)
				message(FATAL_ERROR "${lib} is required but not found")
			else()
				_internal_message("${lib} not found")
			endif()
		endif()
	endif()
endmacro()

