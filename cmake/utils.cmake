function(enable_warnings TARGET_NAME)
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
    else()
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

function(msvc_enable_multiprocess_global)
    add_compile_options(/MP)
endfunction()

function(msvc_startup_project STARTUP_PROJECT)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${STARTUP_PROJECT})
endfunction()

function(msvc_working_directory target directory)
    set_target_properties(${target} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${directory}")
endfunction()