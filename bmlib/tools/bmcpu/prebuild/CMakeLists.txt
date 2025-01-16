PROJECT (prebuild)

file(INSTALL user_algo.so DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/user_algo.so
        DESTINATION ./lib/sophon-rpc
        COMPONENT libsophon)
