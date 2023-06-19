
function (build_doc name)
    add_custom_command(
        OUTPUT ${name}
        COMMAND make pdf
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${name})
endfunction()

build_doc("reference")

add_custom_target(rtdoc DEPENDS "reference")
if (TARGET doc)
    add_dependencies(doc rtdoc)
endif()
