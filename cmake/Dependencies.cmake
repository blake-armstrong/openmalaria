include_guard (GLOBAL)

include (FetchContent)

function (om_dependency name)
  cmake_parse_arguments (PARSE_ARGV 1 arg "NO_FIND_PACKAGE" "VERSION;URL;SHA256" "FIND_PACKAGE_ARGS")
  foreach (required VERSION URL SHA256)
    if (NOT arg_${required})
      message (FATAL_ERROR "om_dependency(${name}): ${required} is required")
    endif ()
  endforeach ()
  if (arg_UNPARSED_ARGUMENTS)
    message (FATAL_ERROR "om_dependency(${name}): unexpected arguments ${arg_UNPARSED_ARGUMENTS}")
  endif ()

  if (arg_NO_FIND_PACKAGE AND arg_FIND_PACKAGE_ARGS)
    message (FATAL_ERROR "om_dependency(${name}): NO_FIND_PACKAGE and FIND_PACKAGE_ARGS are exclusive")
  endif ()
  if (arg_NO_FIND_PACKAGE)
    FetchContent_Declare (${name}
      URL "${arg_URL}"
      URL_HASH SHA256=${arg_SHA256}
    )
  else ()
    FetchContent_Declare (${name}
      URL "${arg_URL}"
      URL_HASH SHA256=${arg_SHA256}
      FIND_PACKAGE_ARGS ${arg_VERSION} GLOBAL ${arg_FIND_PACKAGE_ARGS}
    )
  endif ()
  FetchContent_MakeAvailable (${name})

  string (TOLOWER "${name}" lower_name)
  if (${lower_name}_SOURCE_DIR)
    message (STATUS "Dependency ${name}: fetched ${arg_VERSION} into ${${lower_name}_SOURCE_DIR}")
  else ()
    message (STATUS "Dependency ${name}: using installed ${${name}_VERSION} from ${${name}_DIR}")
  endif ()
endfunction ()
