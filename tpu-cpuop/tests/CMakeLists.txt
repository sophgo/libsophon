find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

set(test_examples
    boxnms_test
    roialign_test)
foreach(name ${test_examples})
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} cpuop Threads::Threads ${CMAKE_DL_LIBS})
endforeach()

set(test_cases
    test_cpu_random_uniform
    test_grid_sampler)
foreach(name ${test_cases})
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE
        GTest::gtest GTest::main cpuop Threads::Threads ${CMAKE_DL_LIBS})
    add_test(${name} ${CMAKE_CURRENT_BINARY_DIR}/${name})
endforeach()
