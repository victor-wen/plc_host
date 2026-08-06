function(add_qtest filepath)
    get_filename_component(target ${filepath} NAME_WE)
    add_executable(${target} ${filepath}.cpp)
    target_link_libraries(${target} PRIVATE Qt6::Test)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_qtest(unit/tst_smoke)
