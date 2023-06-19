function(get_git_revision out_var)
    find_package(Git)
    if (NOT Git_FOUND)
        message(WARNING "Git not found.")
        set(git_revision "N/A")
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
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD OUTPUT_VARIABLE git_revision
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    set(${out_var} ${git_revision} PARENT_SCOPE)
endfunction()
