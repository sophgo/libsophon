function(get_version_from_tag ver_var sover_var revision_var)
    find_package(Git)
    set(ver "9.9.9")
    set(sover "9.9")
    set(revision "9.9.9")
    if (NOT Git_FOUND)
        message(WARNING "Git not found.")
    else()
        execute_process(
            COMMAND stat -c "%u" ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE repo_owner
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(
            COMMAND id -u
            OUTPUT_VARIABLE uid
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if (NOT ${repo_owner} STREQUAL ${uid})
            set(cmd
                ${GIT_EXECUTABLE} config --global --add safe.directory
                ${CMAKE_CURRENT_SOURCE_DIR})
            string(REPLACE ";" " " cmd_str "${cmd}")
            message(WARNING "${cmd_str}")
            execute_process(COMMAND ${cmd})
        endif()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0 OUTPUT_VARIABLE ver
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if (NOT ver STREQUAL "")
            string(REGEX MATCH "[0-9]+\\.[0-9]+" sover ${ver})
            string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" ver ${ver})
            execute_process(
                COMMAND ${GIT_EXECUTABLE} describe --tags HEAD OUTPUT_VARIABLE revision
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_STRIP_TRAILING_WHITESPACE)
        endif()
    endif()
    set(${ver_var} ${ver} PARENT_SCOPE)
    set(${sover_var} ${sover} PARENT_SCOPE)
    set(${revision_var} ${revision} PARENT_SCOPE)
endfunction()
