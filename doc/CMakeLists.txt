cmake_minimum_required(VERSION 3.6)
cmake_policy(SET CMP0048 NEW)

set(DOC_DEST_DIR "${CMAKE_INSTALL_PREFIX}/../doc")
file(MAKE_DIRECTORY ${DOC_DEST_DIR})

set(GUIDE_PDF "guide/build/LIBSOPHON使用手册.pdf")
add_custom_command(OUTPUT ${GUIDE_PDF}
    COMMAND rm -rf build
    COMMAND make html BUILD_RELEASE_VERSION="${CPACK_PACKAGE_VERSION}"
    COMMAND make pdf BUILD_RELEASE_VERSION="${CPACK_PACKAGE_VERSION}"
    COMMAND cp ${CMAKE_SOURCE_DIR}/doc/${GUIDE_PDF} ${DOC_DEST_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc/guide
)

set(GUIDE_EN_PDF "guide_en/build/LIBSOPHON_User_Guide.pdf")
add_custom_command(OUTPUT ${GUIDE_EN_PDF}
    COMMAND rm -rf build
    COMMAND make html BUILD_RELEASE_VERSION="${CPACK_PACKAGE_VERSION}"
    COMMAND make pdf BUILD_RELEASE_VERSION="${CPACK_PACKAGE_VERSION}"
    COMMAND cp ${CMAKE_SOURCE_DIR}/doc/${GUIDE_EN_PDF} ${DOC_DEST_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc/guide_en
)

set(REFERENCE_EN_PDF "reference_en/build/BMLib_Technical_Reference_Manual.pdf")
add_custom_command(OUTPUT ${REFERENCE_EN_PDF}
    COMMAND rm -rf build
    COMMAND make html BUILD_RELEASE_VERSION="${CPACK_PACKAGE_VERSION}"
    COMMAND make pdf BUILD_RELEASE_VERSION="${CPACK_PACKAGE_VERSION}"
    COMMAND cp ${CMAKE_SOURCE_DIR}/doc/${REFERENCE_EN_PDF} ${DOC_DEST_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc/reference_en
)

set(REFERENCE_PDF "reference/build/BMLIB开发参考手册.pdf")
add_custom_command(OUTPUT ${REFERENCE_PDF}
    COMMAND rm -rf build
    COMMAND make html BUILD_RELEASE_VERSION="${CPACK_PACKAGE_VERSION}"
    COMMAND make pdf BUILD_RELEASE_VERSION="${CPACK_PACKAGE_VERSION}"
    COMMAND cp ${CMAKE_SOURCE_DIR}/doc/${REFERENCE_PDF} ${DOC_DEST_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc/reference
)

add_custom_target(doc
                  DEPENDS ${GUIDE_PDF}
                  DEPENDS ${GUIDE_EN_PDF}
                  DEPENDS ${REFERENCE_PDF}
                  DEPENDS ${REFERENCE_EN_PDF}
                  )
